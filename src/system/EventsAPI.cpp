/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>

#include "CPUDependencyData.hpp"
#include "DataAccessRegistration.hpp"
#include "LeaderThread.hpp"
#include "TrackingPoints.hpp"
#include "executors/threads/TaskFinalization.hpp"
#include "executors/threads/ThreadManager.hpp"
#include "executors/threads/WorkerThread.hpp"
#include "tasks/Task.hpp"
#include "tasks/TaskImplementation.hpp"


extern "C" void *nanos6_get_current_event_counter(void)
{
	WorkerThread *currentThread = WorkerThread::getCurrentWorkerThread();
	assert(currentThread != nullptr);

	Task *currentTask = currentThread->getTask();
	assert(currentTask != nullptr);

	return currentTask;
}


extern "C" void nanos6_increase_current_task_event_counter(void *event_counter, unsigned int increment)
{
	assert(event_counter != 0);
	if (increment == 0)
		return;

	Task *task = static_cast<Task *>(event_counter);

	task->increaseReleaseCount(increment);
}


extern "C" void nanos6_decrease_task_event_counter(void *event_counter, unsigned int decrement)
{
	assert(event_counter != 0);
	if (decrement == 0)
		return;

	Task *task = static_cast<Task *>(event_counter);

	// Release dependencies if the event counter becomes zero
	if (task->decreaseReleaseCount(decrement)) {
		CPU *cpu = nullptr;
		WorkerThread *currentThread = WorkerThread::getCurrentWorkerThread();
		if (currentThread != nullptr) {
			cpu = currentThread->getComputePlace();
			assert(cpu != nullptr);
		} else if (LeaderThread::isLeaderThread()) {
			cpu = LeaderThread::getComputePlace();
			assert(cpu != nullptr);
		}

		// Release the data accesses of the task. Do not merge these
		// two conditions; the creation of a local CPU dependency data
		// structure may introduce unnecessary overhead
		if (cpu != nullptr) {
			DataAccessRegistration::unregisterTaskDataAccesses(
				task, cpu, cpu->getDependencyData(),
				/* memory place */ nullptr,
				/* from a busy thread */ true
			);
		} else {
			CPUDependencyData localDependencyData;
			DataAccessRegistration::unregisterTaskDataAccesses(
				task, nullptr, localDependencyData,
				/* memory place */ nullptr,
				/* from a busy thread */ true
			);
		}

		TaskFinalization::taskFinished(task, cpu, true);

		// Try to dispose the task
		if (task->markAsReleased()) {
			TaskFinalization::disposeTask(task);
		}
	}
}
