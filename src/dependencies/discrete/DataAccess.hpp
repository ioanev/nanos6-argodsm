/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef DATA_ACCESS_HPP
#define DATA_ACCESS_HPP

#include <atomic>
#include <bitset>
#include <cassert>
#include <iostream>
#include <stack>

#include "DataAccessFlags.hpp"
#include "../DataAccessType.hpp"
#include "ReductionInfo.hpp"
#include "ReductionSpecific.hpp"

#include <DataAccessRegion.hpp>
#include <InstrumentDataAccessId.hpp>
#include <InstrumentDependenciesByAccessLinks.hpp>

struct DataAccess;
class Task;
struct BottomMapEntry;

//! The accesses that one or more tasks perform sequentially to a memory location that can occur concurrently (unless commutative).
struct DataAccess {
private:
	//! The type of the access
	DataAccessType _type;

	//! The originator of the access
	Task *_originator;

	//! Reduction-specific information of current access
	ReductionInfo *_reductionInfo;

	//! Reduction stuff
	size_t _reductionLength;
	reduction_type_and_operator_index_t _reductionOperator;
	reduction_index_t _reductionIndex;

	//! Next task with an access matching this one
	std::atomic<DataAccess *> _successor;
	std::atomic<DataAccess *> _child;

	//! Atomic flags for Read / Write / Deletable / Finished
	std::atomic<access_flags_t> _accessFlags;

	//! Instrumentation specific data
	Instrument::data_access_id_t _instrumentDataAccessId;

	void *_address;
	size_t _length;

	MemoryPlace const *_location;

	DataAccessMessage inAutomata(access_flags_t flags, access_flags_t oldFlags, bool toNextOnly, bool weak);
	void outAutomata(access_flags_t flags, access_flags_t oldFlags, DataAccessMessage &message, bool weak);
	void inoutAutomata(access_flags_t flags, access_flags_t oldFlags, DataAccessMessage &message, bool weak);
	void reductionAutomata(access_flags_t flags, access_flags_t oldFlags, DataAccessMessage &message);
	DataAccessMessage concurrentAutomata(access_flags_t flags, access_flags_t oldFlags, bool toNextOnly, bool weak);
	DataAccessMessage commutativeAutomata(access_flags_t flags, access_flags_t oldFlags, bool toNextOnly, bool weak);
	void readDestination(access_flags_t allFlags, DataAccessMessage &message, PropagationDestination &destination);

public:
	DataAccess(DataAccessType type, Task *originator, void *address, size_t length, bool weak) :
		_type(type),
		_originator(originator),
		_reductionInfo(nullptr),
		_successor(nullptr),
		_child(nullptr),
		_accessFlags(0),
		_address(address),
		_length(length),
		_location(nullptr)
	{
		assert(originator != nullptr);

		if (weak)
			_accessFlags.fetch_or(ACCESS_IS_WEAK, std::memory_order_relaxed);
	}

	DataAccess(const DataAccess &other) :
		_type(other.getType()),
		_originator(other.getOriginator()),
		_reductionInfo(other.getReductionInfo()),
		_successor(other.getSuccessor()),
		_child(other.getChild()),
		_accessFlags(other.getFlags()),
		_address(other.getAddress()),
		_length(other.getLength()),
		_location(other.getLocation())
	{
	}

	~DataAccess()
	{
	}

	DataAccessMessage applySingle(access_flags_t flags, mailbox_t &mailBox);

	bool apply(DataAccessMessage &message, mailbox_t &mailBox);

	bool applyPropagated(DataAccessMessage &message);

	inline void setType(DataAccessType type)
	{
		_type = type;
	}

	inline DataAccessType getType() const
	{
		return _type;
	}

	inline Task *getOriginator() const
	{
		return _originator;
	}

	inline ReductionInfo *getReductionInfo() const
	{
		return _reductionInfo;
	}

	inline void setReductionInfo(ReductionInfo *reductionInfo)
	{
		assert(_reductionInfo == nullptr);
		assert(_type == REDUCTION_ACCESS_TYPE);
		_reductionInfo = reductionInfo;
	}

	inline DataAccess *getSuccessor() const
	{
		return _successor;
	}

	inline void setSuccessor(DataAccess *successor)
	{
		_successor = successor;
	}

	inline bool isWeak() const
	{
		return (_accessFlags.load(std::memory_order_relaxed) & ACCESS_IS_WEAK);
	}

	inline void setWeak(bool value = true)
	{
		if (value)
			_accessFlags.fetch_or(ACCESS_IS_WEAK, std::memory_order_relaxed);
		else
			_accessFlags.fetch_and(~ACCESS_IS_WEAK, std::memory_order_relaxed);
	}

	inline void setInstrumentationId(Instrument::data_access_id_t instrumentDataAccessId)
	{
		_instrumentDataAccessId = instrumentDataAccessId;
	}

	inline Instrument::data_access_id_t &getInstrumentationId()
	{
		return _instrumentDataAccessId;
	}

	inline size_t getReductionLength() const
	{
		return _reductionLength;
	}

	inline void setReductionLength(size_t reductionLength)
	{
		_reductionLength = reductionLength;
	}

	inline reduction_type_and_operator_index_t getReductionOperator() const
	{
		return _reductionOperator;
	}

	inline void setReductionOperator(reduction_type_and_operator_index_t reductionOperator)
	{
		_reductionOperator = reductionOperator;
	}

	inline reduction_index_t getReductionIndex() const
	{
		return _reductionIndex;
	}

	inline void setReductionIndex(reduction_index_t reductionIndex)
	{
		_reductionIndex = reductionIndex;
	}

	inline DataAccess *getChild() const
	{
		return _child;
	}

	inline void setChild(DataAccess *child)
	{
		_child = child;
	}

	inline access_flags_t getFlags() const
	{
		return _accessFlags;
	}

	inline size_t getLength() const
	{
		return _length;
	}

	inline void *getAddress() const
	{
		return _address;
	}

	inline DataAccessRegion getAccessRegion() const {
		return DataAccessRegion(getAddress(), getLength());
	}

	void setLocation(MemoryPlace const *location)
	{
		_location = location;
		Instrument::newDataAccessLocation(_instrumentDataAccessId, location);
	}

	MemoryPlace const *getLocation() const
	{
		return _location;
	}
};


#endif // DATA_ACCESS_HPP
