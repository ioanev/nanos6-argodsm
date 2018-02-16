/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.
	
	Copyright (C) 2015-2017 Barcelona Supercomputing Center (BSC)
*/

#include "InstrumentExtrae.hpp"

#include "../generic_ids/GenericIds.hpp"

#include <nanos6/debug.h>


namespace Instrument {
	const EnvironmentVariable<bool> _traceAsThreads("NANOS6_EXTRAE_AS_THREADS", 0);
	const EnvironmentVariable<int> _sampleBacktraceDepth("NANOS6_EXTRAE_SAMPLE_BACKTRACE_DEPTH", 0);
	const EnvironmentVariable<long> _sampleBacktracePeriod("NANOS6_EXTRAE_SAMPLE_BACKTRACE_PERIOD", 1000);
	const EnvironmentVariable<bool> _sampleTaskCount("NANOS6_EXTRAE_SAMPLE_TASK_COUNT", 0);
	const EnvironmentVariable<bool> _emitGraph("NANOS6_EXTRAE_EMIT_GRAPH", 0);
	
	SpinLock                  _extraeLock;
	
	char const               *_eventStateValueStr[NANOS_EVENT_STATE_TYPES] = {
		"NOT CREATED", "NOT RUNNING", "STARTUP", "SHUTDOWN", "ERROR", "IDLE",
		"RUNTIME", "RUNNING", "SYNCHRONIZATION", "SCHEDULING", "CREATION" };
	
	SpinLock _userFunctionMapLock;
	user_fct_map_t            _userFunctionMap;
	
	SpinLock _backtraceAddressSetsLock;
	std::list<std::set<void *> *> _backtraceAddressSets;
	
	std::atomic<size_t> _nextTaskId(1);
	std::atomic<size_t> _readyTasks(0);
	std::atomic<size_t> _liveTasks(0);
	std::atomic<size_t> _nextTracingPointKey(1);
	
	RWSpinLock _extraeThreadCountLock;
	
	int _externalThreadCount = 0;
	
	unsigned int extrae_nanos_get_num_threads()
	{
		assert(_traceAsThreads);
		return GenericIds::getTotalThreads();
	}
	
	unsigned int extrae_nanos_get_num_cpus_and_external_threads()
	{
		assert(!_traceAsThreads);
		return nanos_get_num_cpus() + GenericIds::getTotalExternalThreads();
	}
	
}

