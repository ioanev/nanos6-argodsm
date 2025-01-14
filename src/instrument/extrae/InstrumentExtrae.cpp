/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/

#include <nanos6/debug.h>

#include "InstrumentExtrae.hpp"
#include "instrument/generic_ids/GenericIds.hpp"


namespace Instrument {
	namespace Extrae {
		bool _detailTaskGraph = false;
		bool _detailTaskCount = false;

		bool _extraeInstrumentCluster = false;
		bool _extraeInstrumentDependencies = false;

		unsigned int _detailLevel = 0;
		bool _traceAsThreads = false;

		bool _initialized = false;
	}

	std::map<tracing_point_type_t, std::string> _delayedNumericTracingPoints;
	std::map<tracing_point_type_t, scope_tracing_point_info_t> _delayedScopeTracingPoints;
	std::map<tracing_point_type_t, enumerated_tracing_point_info_t> _delayedEnumeratedTracingPoints;

	SpinLock _extraeLock;

	char const *_eventStateValueStr[NANOS_EVENT_STATE_TYPES] = {
		"NOT CREATED", "NOT RUNNING", "STARTUP", "SHUTDOWN", "ERROR", "IDLE",
		"RUNTIME", "RUNNING", "SYNCHRONIZATION", "SCHEDULING", "CREATION", "THREAD CREATION"};

	char const *_reductionStateValueStr[NANOS_REDUCTION_STATE_TYPES] = {
		"OUTSIDE REDUCTION",
		"ALLOCATE REDUCTION INFO",
		"RETRIEVE REDUCTION STORAGE", "ALLOCATE REDUCTION STORAGE",
		"INITIALIZE REDUCTION STORAGE", "COMBINE REDUCTION STORAGE"};

	char const *_dependencySubsystemStateValueStr[NANOS_DEPENDENCY_STATE_TYPES] = {
		"OUTSIDE DEPENDENCY SUBSYSTEM", "REGISTERTASKDATAACCESSES", "UNREGISTERTASKDATAACCESSES",
		"PROPAGATESATISFIABILITY", "RELEASEACCESSREGION", "HANDLEENTERTASKWAIT",
		"HANDLEEXITTASKWAIT",
		"UNREGISTERTASKDATAACCESSESCALLBACK", "UNREGISTERTASKDATAACCESSES2",
		"HANDLECOMPLETEDTASKWAITS", "SETUPTASKWAITWORKFLOW", "RELEASETASKWAITFRAGMENT",
		"CREATEDATACOPYSTEP_TASK", "CREATEDATACOPYSTEP_TASKWAIT"
	};

	SpinLock _userFunctionMapLock;
	user_fct_map_t _userFunctionMap;

	std::atomic<size_t> _nextTaskId(1);
	std::atomic<size_t> _readyTasks(0);
	std::atomic<size_t> _liveTasks(0);
	std::atomic<size_t> _nextTracingPointKey(1);

	RWSpinLock _extraeThreadCountLock;

	int _externalThreadCount = 0;

	unsigned int extrae_nanos6_get_num_threads()
	{
		assert(Extrae::_traceAsThreads);

		return GenericIds::getTotalThreads();
	}

	unsigned int extrae_nanos6_get_num_cpus_and_external_threads()
	{
		assert(!Extrae::_traceAsThreads);

		// We use "total_num_cpus" since, when DLB is enabled, any CPU in the
		// system might emit events
		return nanos6_get_total_num_cpus() + GenericIds::getTotalExternalThreads();
	}

} // namespace Instrument
