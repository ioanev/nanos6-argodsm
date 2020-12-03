/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2018-2020 Barcelona Supercomputing Center (BSC)
*/

#include "ExecutionWorkflowHost.hpp"
#include "dependencies/SymbolTranslation.hpp"
#include "executors/threads/TaskFinalization.hpp"
#include "executors/threads/WorkerThread.hpp"
#include "hardware/places/ComputePlace.hpp"
#include "hardware/places/MemoryPlace.hpp"
#include "hardware-counters/HardwareCounters.hpp"
#include "monitoring/Monitoring.hpp"
#include "scheduling/Scheduler.hpp"
#include "tasks/Task.hpp"
#include "tasks/Taskfor.hpp"

#include <DataAccessRegistration.hpp>
#include <InstrumentInstrumentationContext.hpp>
#include <InstrumentLogMessage.hpp>
#include <InstrumentTaskExecution.hpp>
#include <InstrumentTaskStatus.hpp>
#include <InstrumentThreadInstrumentationContext.hpp>
#include <InstrumentThreadManagement.hpp>


namespace ExecutionWorkflow {
	void HostExecutionStep::start()
	{
		nanos6_address_translation_entry_t stackTranslationTable[SymbolTranslation::MAX_STACK_SYMBOLS];

		WorkerThread *currentThread = WorkerThread::getCurrentWorkerThread();
		CPU *cpu = (currentThread == nullptr) ? nullptr : currentThread->getComputePlace();

		//! We are trying to start the execution of the Task from within
		//! something that is not a WorkerThread, or it does not have
		//! a CPU or the task assigned to it.
		//!
		//! This will happen once the last DataCopyStep finishes and
		//! releases the ExecutionStep.
		//!
		//! In that case we need to add the Task back for scheduling.
		if ((currentThread == nullptr) || (cpu == nullptr) || (currentThread->getTask() == nullptr)) {
			_task->setExecutionStep(this);

			Scheduler::addReadyTask(_task, nullptr, BUSY_COMPUTE_PLACE_TASK_HINT);
			return;
		}

		_task->setThread(currentThread);
		Instrument::task_id_t taskId = _task->getInstrumentationTaskId();

		Instrument::ThreadInstrumentationContext instrumentationContext(
			taskId,
			cpu->getInstrumentationId(),
			currentThread->getInstrumentationId());

		if (_task->hasCode()) {
			size_t tableSize = 0;
			nanos6_address_translation_entry_t *translationTable =
				SymbolTranslation::generateTranslationTable(
					_task, cpu, stackTranslationTable, tableSize);

			// Before executing a task, read runtime-related counters
			HardwareCounters::updateRuntimeCounters();

			bool isTaskforCollaborator = _task->isTaskforCollaborator();
			if (isTaskforCollaborator) {
				bool first = ((Taskfor *)_task)->hasFirstChunk();
				Instrument::task_id_t parentTaskId = _task->getParent()->getInstrumentationTaskId();
				Instrument::startTaskforCollaborator(parentTaskId, taskId, first);
				Instrument::taskforCollaboratorIsExecuting(parentTaskId, taskId);
			} else {
				Instrument::startTask(taskId);
				Instrument::taskIsExecuting(taskId);
			}

			Monitoring::taskChangedStatus(_task, executing_status);

			// Run the task
			std::atomic_thread_fence(std::memory_order_acquire);
			_task->body(translationTable);
			std::atomic_thread_fence(std::memory_order_release);

			// Free up all symbol translation
			if (tableSize > 0)
				MemoryAllocator::free(translationTable, tableSize);

			// Update the CPU since the thread may have migrated
			cpu = currentThread->getComputePlace();
			instrumentationContext.updateComputePlace(cpu->getInstrumentationId());

			HardwareCounters::updateTaskCounters(_task);
			Monitoring::taskChangedStatus(_task, paused_status);
			Monitoring::taskCompletedUserCode(_task);

			if (isTaskforCollaborator) {
				bool last = ((Taskfor *)_task)->hasLastChunk();
				Instrument::task_id_t parentTaskId = _task->getParent()->getInstrumentationTaskId();
				Instrument::taskforCollaboratorStopped(parentTaskId, taskId);
				Instrument::endTaskforCollaborator(parentTaskId, taskId, last);
			} else {
				Instrument::taskIsZombie(taskId);
				Instrument::endTask(taskId);
			}
		} else {
			Monitoring::taskChangedStatus(_task, paused_status);
			Monitoring::taskCompletedUserCode(_task);
		}

		DataAccessRegistration::combineTaskReductions(_task, cpu);

		//! Release the subsequent steps.
		releaseSuccessors();
		delete this;
	}
}; // namespace ExecutionWorkflow
