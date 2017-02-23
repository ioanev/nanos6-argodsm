#ifndef TASK_DATA_ACCESSES_HPP
#define TASK_DATA_ACCESSES_HPP

#include <atomic>
#include <bitset>
#include <cassert>
#include <mutex>

#include <InstrumentDependenciesByAccessLinks.hpp>

#include "BottomMapEntry.hpp"
#include "IntrusiveLinearRegionMap.hpp"
#include "IntrusiveLinearRegionMapImplementation.hpp"
#include "TaskDataAccessLinkingArtifacts.hpp"
#include "lowlevel/PaddedTicketSpinLock.hpp"


struct DataAccess;
class Task;


struct TaskDataAccesses {
	typedef PaddedTicketSpinLock<int, 128> spinlock_t;
	
	typedef IntrusiveLinearRegionMap<
		DataAccess,
		boost::intrusive::function_hook< TaskDataAccessLinkingArtifacts >
	> accesses_t;
	typedef IntrusiveLinearRegionMap<
		DataAccess,
		boost::intrusive::function_hook< TaskDataAccessLinkingArtifacts >
	> access_fragments_t;
	typedef IntrusiveLinearRegionMap<
		BottomMapEntry,
		boost::intrusive::function_hook< BottomMapEntryLinkingArtifacts >
	> subaccess_bottom_map_t;
	
#ifndef NDEBUG
	enum flag_bits {
		HAS_BEEN_DELETED_BIT=0,
		TOTAL_FLAG_BITS
	};
	typedef std::bitset<TOTAL_FLAG_BITS> flags_t;
#endif
	
	spinlock_t _lock;
	accesses_t _accesses;
	access_fragments_t _accessFragments;
	subaccess_bottom_map_t _subaccessBottomMap;
	
	int _removalBlockers;
#ifndef NDEBUG
	flags_t _flags;
#endif
	
	TaskDataAccesses()
		: _lock(),
		_accesses(), _accessFragments(),
		_subaccessBottomMap(),
		_removalBlockers(0)
#ifndef NDEBUG
		,_flags()
#endif
	{
	}
	
	~TaskDataAccesses();
	
	TaskDataAccesses(TaskDataAccesses const &other) = delete;
	
#ifndef NDEBUG
	bool hasBeenDeleted() const
	{
		return _flags[HAS_BEEN_DELETED_BIT];
	}
	flags_t::reference hasBeenDeleted()
	{
		return _flags[HAS_BEEN_DELETED_BIT];
	}
#endif
	
};


typedef typename TaskDataAccessLinkingArtifacts::hook_type TaskDataAccessesHook;


struct TaskDataAccessHooks {
	TaskDataAccessesHook _accessesHook;
};


#endif // TASK_DATA_ACCESSES_HPP
