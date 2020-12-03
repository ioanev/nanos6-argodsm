/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019-2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef HOST_REDUCTION_STORAGE_HPP
#define HOST_REDUCTION_STORAGE_HPP

#include "dependencies/discrete/DeviceReductionStorage.hpp"
#include "support/bitset/AtomicBitset.hpp"

class HostReductionStorage : public DeviceReductionStorage {
public:
	struct ReductionSlot {
		void *storage = nullptr;
		bool initialized = false;
	};

	typedef ReductionSlot slot_t;

	HostReductionStorage(void *address, size_t length, size_t paddedLength,
		std::function<void(void *, void *, size_t)> initializationFunction,
		std::function<void(void *, void *, size_t)> combinationFunction);

	void *getFreeSlotStorage(Task *task, size_t slotIndex, ComputePlace *destinationComputePlace);

	void combineInStorage(void *combineDestination);

	void releaseSlotsInUse(Task *task, ComputePlace *computePlace);

	size_t getFreeSlotIndex(Task *task, ComputePlace *destinationComputePlace);

	~HostReductionStorage(){};

private:
	std::vector<slot_t> _slots;
	std::vector<long int> _currentCpuSlotIndices;
	AtomicBitset<> _freeSlotIndices;
};

#endif // HOST_REDUCTION_STORAGE_HPP