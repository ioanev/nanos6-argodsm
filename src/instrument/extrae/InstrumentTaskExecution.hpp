/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef INSTRUMENT_EXTRAE_TASK_EXECUTION_HPP
#define INSTRUMENT_EXTRAE_TASK_EXECUTION_HPP


#include "InstrumentExtrae.hpp"
#include "system/ompss/SpawnFunction.hpp"
#include "instrument/api/InstrumentTaskExecution.hpp"
#include "../support/InstrumentThreadLocalDataSupport.hpp"

#include <cassert>

namespace Instrument {

	inline void startTask(
		task_id_t taskId,
		__attribute__((unused)) InstrumentationContext const &context
	) {
		extrae_combined_events_t ce;

		ce.HardwareCounters = 1;
		ce.Callers = 0;
		ce.UserFunction = EXTRAE_USER_FUNCTION_NONE;
		ce.nEvents = 5;
		ce.nCommunications = 0;

		// Precise task count (not sampled)
		if (Extrae::_detailTaskCount) {
			ce.nEvents += 1;
		}

		// Generate graph information
		if (Extrae::_detailTaskGraph) {
			taskId._taskInfo->_lock.lock();
			ce.nCommunications += taskId._taskInfo->_predecessors.size();
		}

		ce.Types  = (extrae_type_t *)  alloca(ce.nEvents * sizeof (extrae_type_t));
		ce.Values = (extrae_value_t *) alloca(ce.nEvents * sizeof (extrae_value_t));

		if (ce.nCommunications > 0) {
			if (ce.nCommunications < 100) {
				ce.Communications = (extrae_user_communication_t *) alloca(
					sizeof(extrae_user_communication_t) * ce.nCommunications
				);
			} else {
				ce.Communications = (extrae_user_communication_t *) malloc(
					sizeof(extrae_user_communication_t) * ce.nCommunications
				);
			}
		}

		ce.Types[0] = (extrae_type_t) EventType::RUNTIME_STATE;
		ce.Values[0] = (extrae_value_t) NANOS_RUNNING;

		nanos6_task_info_t *taskInfo = taskId._taskInfo->_taskInfo;
		assert(taskInfo != nullptr);

		ce.Types[1] = (extrae_type_t) EventType::RUNNING_CODE_LOCATION;
		ce.Values[1] = (extrae_value_t) taskInfo->implementations[0].run;

		// Use the unique taskInfo address in case it is a spawned task
		if (SpawnFunction::isSpawned(taskInfo)) {
			ce.Values[1] = (extrae_value_t) taskInfo;
		}

		ce.Types[2] = (extrae_type_t) EventType::NESTING_LEVEL;
		ce.Values[2] = (extrae_value_t) taskId._taskInfo->_nestingLevel;

		ce.Types[3] = (extrae_type_t) EventType::TASK_INSTANCE_ID;
		ce.Values[3] = (extrae_value_t) taskId._taskInfo->_taskId;

		ce.Types[4] = (extrae_type_t) EventType::PRIORITY;
		ce.Values[4] = (extrae_value_t) taskId._taskInfo->_priority;

		// Generate graph information
		if (Extrae::_detailTaskGraph) {
			int index = 0;
			for (auto const &taskAndTag : taskId._taskInfo->_predecessors) {
				ce.Communications[index].type = EXTRAE_USER_RECV;
				ce.Communications[index].tag = (extrae_comm_tag_t) taskAndTag.second;
				ce.Communications[index].size = (taskAndTag.first << 32) + taskId._taskInfo->_taskId;
				ce.Communications[index].partner = EXTRAE_COMM_PARTNER_MYSELF;
				ce.Communications[index].id = (taskAndTag.first << 32) + taskId._taskInfo->_taskId;
				index++;
			}
			taskId._taskInfo->_predecessors.clear();
			taskId._taskInfo->_lock.unlock();
		}

		size_t readyTasks = --_readyTasks;

		// Precise task count (not sampled)
		if (Extrae::_detailTaskCount) {
			ce.Types[5] = (extrae_type_t) EventType::READY_TASKS;
			ce.Values[5] = (extrae_value_t) readyTasks;

			// This counter is not so reliable, so try to skip underflows
			if (((signed long long) ce.Values[5]) < 0) {
				ce.Values[5] = 0;
			}
		}

		ThreadLocalData &threadLocal = getThreadLocalData();
		threadLocal._nestingLevels.push_back(taskId._taskInfo->_nestingLevel);

		if (Extrae::_traceAsThreads) {
			_extraeThreadCountLock.readLock();
		}
		ExtraeAPI::emit_CombinedEvents ( &ce );
		if (Extrae::_traceAsThreads) {
			_extraeThreadCountLock.readUnlock();
		}

		if (ce.nCommunications >= 100) {
			free(ce.Communications);
		}
	}

	inline void endTask(
		task_id_t taskId,
		__attribute__((unused)) InstrumentationContext const &context
	) {
		extrae_combined_events_t ce;

		ce.HardwareCounters = 1;
		ce.Callers = 0;
		ce.UserFunction = EXTRAE_USER_FUNCTION_NONE;
		ce.nEvents = 5;
		ce.nCommunications = 0;

		// Precise task count (not sampled)
		if (Extrae::_detailTaskCount) {
			ce.nEvents += 1;
		}

		// Generate control dependency information
		size_t parentInTaskwait = 0;
		if (Extrae::_detailLevel >= 8) {
			if ((taskId._taskInfo->_parent != nullptr) && taskId._taskInfo->_parent->_inTaskwait) {
				taskId._taskInfo->_parent->_lock.lock();
				if (taskId._taskInfo->_parent->_inTaskwait) {
					parentInTaskwait = taskId._taskInfo->_parent->_taskId;
					ce.nCommunications++;

					taskId._taskInfo->_parent->_predecessors.emplace(taskId._taskInfo->_taskId, control_dependency_tag);
				}
				taskId._taskInfo->_parent->_lock.unlock();
			}
		}

		ce.Types  = (extrae_type_t *)  alloca (ce.nEvents * sizeof (extrae_type_t) );
		ce.Values = (extrae_value_t *) alloca (ce.nEvents * sizeof (extrae_value_t));

		if (ce.nCommunications > 0) {
			ce.Communications = (extrae_user_communication_t *) alloca(sizeof(extrae_user_communication_t) * ce.nCommunications);
		}

		ce.Types[0] = (extrae_type_t) EventType::RUNTIME_STATE;
		ce.Values[0] = (extrae_value_t) NANOS_RUNTIME;

		ce.Types[1] = (extrae_type_t) EventType::RUNNING_CODE_LOCATION;
		ce.Values[1] = (extrae_value_t) nullptr;

		ce.Types[2] = (extrae_type_t) EventType::NESTING_LEVEL;
		ce.Values[2] = (extrae_value_t) nullptr;

		ce.Types[3] = (extrae_type_t) EventType::TASK_INSTANCE_ID;
		ce.Values[3] = (extrae_value_t) nullptr;

		ce.Types[4] = (extrae_type_t) EventType::PRIORITY;
		ce.Values[4] = (extrae_value_t) nullptr;

		if (parentInTaskwait != 0) {
			ce.Communications[0].type = EXTRAE_USER_SEND;
			ce.Communications[0].tag = (extrae_comm_tag_t) control_dependency_tag;
			ce.Communications[0].size = (taskId._taskInfo->_taskId << 32) + parentInTaskwait;
			ce.Communications[0].partner = EXTRAE_COMM_PARTNER_MYSELF;
			ce.Communications[0].id = (taskId._taskInfo->_taskId << 32) + parentInTaskwait;
		}

		size_t liveTasks = --_liveTasks;

		// Precise task count (not sampled)
		if (Extrae::_detailTaskCount) {
			ce.Types[5] = (extrae_type_t) EventType::LIVE_TASKS;
			ce.Values[5] = (extrae_value_t) liveTasks;

			// This counter is not so reliable, so try to skip underflows
			if (((signed long long) ce.Values[5]) < 0) {
				ce.Values[5] = 0;
			}
		}

		if (Extrae::_traceAsThreads) {
			_extraeThreadCountLock.readLock();
		}
		ExtraeAPI::emit_CombinedEvents ( &ce );
		if (Extrae::_traceAsThreads) {
			_extraeThreadCountLock.readUnlock();
		}

		ThreadLocalData &threadLocal = getThreadLocalData();
		assert(!threadLocal._nestingLevels.empty());
		threadLocal._nestingLevels.pop_back();
	}


	inline void destroyTask(
		__attribute__((unused)) task_id_t taskId,
		__attribute__((unused)) InstrumentationContext const &context
	) {
	}


	inline void startTaskforCollaborator(
		task_id_t taskforId,
		__attribute__((unused)) task_id_t collaboratorId,
		bool first,
		__attribute__((unused)) InstrumentationContext const &context
	) {
		extrae_combined_events_t ce;

		ce.HardwareCounters = 1;
		ce.Callers = 0;
		ce.UserFunction = EXTRAE_USER_FUNCTION_NONE;
		ce.nEvents = 5;
		ce.nCommunications = 0;

		if (first) {
			// Precise task count (not sampled)
			if (Extrae::_detailTaskCount) {
				ce.nEvents += 1;
			}
		}

		ce.Types  = (extrae_type_t *)  alloca (ce.nEvents * sizeof (extrae_type_t) );
		ce.Values = (extrae_value_t *) alloca (ce.nEvents * sizeof (extrae_value_t));


		ce.Types[0] = (extrae_type_t) EventType::RUNTIME_STATE;
		ce.Values[0] = (extrae_value_t) NANOS_RUNNING;

		nanos6_task_info_t *taskInfo = taskforId._taskInfo->_taskInfo;
		assert(taskInfo != nullptr);

		ce.Types[1] = (extrae_type_t) EventType::RUNNING_CODE_LOCATION;
		ce.Values[1] = (extrae_value_t) taskInfo->implementations[0].run;

		// Use the unique taskInfo address in case it is a spawned task
		if (SpawnFunction::isSpawned(taskInfo)) {
			ce.Values[1] = (extrae_value_t) taskInfo;
		}

		ce.Types[2] = (extrae_type_t) EventType::NESTING_LEVEL;
		ce.Values[2] = (extrae_value_t) taskforId._taskInfo->_nestingLevel;

		ce.Types[3] = (extrae_type_t) EventType::TASK_INSTANCE_ID;
		ce.Values[3] = (extrae_value_t) taskforId._taskInfo->_taskId;

		ce.Types[4] = (extrae_type_t) EventType::PRIORITY;
		ce.Values[4] = (extrae_value_t) taskforId._taskInfo->_priority;

		if (first) {
			// Generate graph information
			if (Extrae::_detailLevel >= 1) {
				taskforId._taskInfo->_lock.lock();
				ce.nCommunications += taskforId._taskInfo->_predecessors.size();

				if (ce.nCommunications > 0) {
					if (ce.nCommunications < 100) {
						ce.Communications = (extrae_user_communication_t *) alloca(sizeof(extrae_user_communication_t) * ce.nCommunications);
					} else {
						ce.Communications = (extrae_user_communication_t *) malloc(sizeof(extrae_user_communication_t) * ce.nCommunications);
					}
				}

				int index = 0;
				for (auto const &taskAndTag : taskforId._taskInfo->_predecessors) {
					ce.Communications[index].type = EXTRAE_USER_RECV;
					ce.Communications[index].tag = (extrae_comm_tag_t) taskAndTag.second;
					ce.Communications[index].size = (taskAndTag.first << 32) + taskforId._taskInfo->_taskId;
					ce.Communications[index].partner = EXTRAE_COMM_PARTNER_MYSELF;
					ce.Communications[index].id = (taskAndTag.first << 32) + taskforId._taskInfo->_taskId;
					index++;
				}
				taskforId._taskInfo->_predecessors.clear();
				taskforId._taskInfo->_lock.unlock();
			}

			size_t readyTasks = --_readyTasks;

			// Precise task count (not sampled)
			if (Extrae::_detailTaskCount) {
				ce.Types[5] = (extrae_type_t) EventType::READY_TASKS;
				ce.Values[5] = (extrae_value_t) readyTasks;

				// This counter is not so reliable, so try to skip underflows
				if (((signed long long) ce.Values[5]) < 0) {
					ce.Values[5] = 0;
				}
			}
		}

		ThreadLocalData &threadLocal = getThreadLocalData();
		threadLocal._nestingLevels.push_back(taskforId._taskInfo->_nestingLevel);

		if (Extrae::_traceAsThreads) {
			_extraeThreadCountLock.readLock();
		}
		ExtraeAPI::emit_CombinedEvents ( &ce );
		if (Extrae::_traceAsThreads) {
			_extraeThreadCountLock.readUnlock();
		}

		if (ce.nCommunications >= 100) {
			free(ce.Communications);
		}
	}


	inline void endTaskforCollaborator(
		task_id_t taskforId,
		__attribute__((unused)) task_id_t collaboratorId,
		bool last,
		__attribute__((unused)) InstrumentationContext const &context
	) {
		extrae_combined_events_t ce;

		ce.HardwareCounters = 1;
		ce.Callers = 0;
		ce.UserFunction = EXTRAE_USER_FUNCTION_NONE;
		ce.nEvents = 5;
		ce.nCommunications = 0;

		size_t parentInTaskwait = 0;
		if (last) {
			// Precise task count (not sampled)
			if (Extrae::_detailTaskCount) {
				ce.nEvents += 1;
			}

			// Generate control dependency information
			if (Extrae::_detailLevel >= 8) {
				if ((taskforId._taskInfo->_parent != nullptr) && taskforId._taskInfo->_parent->_inTaskwait) {
					taskforId._taskInfo->_parent->_lock.lock();
					if (taskforId._taskInfo->_parent->_inTaskwait) {
						parentInTaskwait = taskforId._taskInfo->_parent->_taskId;
						ce.nCommunications++;

						taskforId._taskInfo->_parent->_predecessors.emplace(
							taskforId._taskInfo->_taskId,
							control_dependency_tag
						);
					}
					taskforId._taskInfo->_parent->_lock.unlock();
				}
			}

			if (ce.nCommunications > 0) {
				ce.Communications = (extrae_user_communication_t *) alloca(
					sizeof(extrae_user_communication_t) * ce.nCommunications
				);
			}
		}

		ce.Types  = (extrae_type_t *)  alloca (ce.nEvents * sizeof (extrae_type_t) );
		ce.Values = (extrae_value_t *) alloca (ce.nEvents * sizeof (extrae_value_t));


		ce.Types[0] = (extrae_type_t) EventType::RUNTIME_STATE;
		ce.Values[0] = (extrae_value_t) NANOS_IDLE;

		ce.Types[1] = (extrae_type_t) EventType::RUNNING_CODE_LOCATION;
		ce.Values[1] = (extrae_value_t) nullptr;

		ce.Types[2] = (extrae_type_t) EventType::NESTING_LEVEL;
		ce.Values[2] = (extrae_value_t) nullptr;

		ce.Types[3] = (extrae_type_t) EventType::TASK_INSTANCE_ID;
		ce.Values[3] = (extrae_value_t) nullptr;

		ce.Types[4] = (extrae_type_t) EventType::PRIORITY;
		ce.Values[4] = (extrae_value_t) nullptr;

		if (last) {
			if (parentInTaskwait != 0) {
				ce.Communications[0].type = EXTRAE_USER_SEND;
				ce.Communications[0].tag = (extrae_comm_tag_t) control_dependency_tag;
				ce.Communications[0].size = (taskforId._taskInfo->_taskId << 32) + parentInTaskwait;
				ce.Communications[0].partner = EXTRAE_COMM_PARTNER_MYSELF;
				ce.Communications[0].id = (taskforId._taskInfo->_taskId << 32) + parentInTaskwait;
			}

			size_t liveTasks = --_liveTasks;

			// Precise task count (not sampled)
			if (Extrae::_detailTaskCount) {
				ce.Types[5] = (extrae_type_t) EventType::LIVE_TASKS;
				ce.Values[5] = (extrae_value_t) liveTasks;

				// This counter is not so reliable, so try to skip underflows
				if (((signed long long) ce.Values[5]) < 0) {
					ce.Values[5] = 0;
				}
			}
		}

		if (Extrae::_traceAsThreads) {
			_extraeThreadCountLock.readLock();
		}
		ExtraeAPI::emit_CombinedEvents ( &ce );
		if (Extrae::_traceAsThreads) {
			_extraeThreadCountLock.readUnlock();
		}

		ThreadLocalData &threadLocal = getThreadLocalData();
		assert(!threadLocal._nestingLevels.empty());
		threadLocal._nestingLevels.pop_back();
	}
}


#endif // INSTRUMENT_EXTRAE_TASK_EXECUTION_HPP
