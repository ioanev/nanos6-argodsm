/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "SchedulerInterface.hpp"
#include "system/TrackingPoints.hpp"

#include <InstrumentScheduler.hpp>


class Scheduler {
	static SchedulerInterface *_instance;

public:
	static void initialize();
	static void shutdown();

	static inline void addReadyTasks(
		nanos6_device_t taskType,
		Task *tasks[],
		const size_t numTasks,
		ComputePlace *computePlace,
		ReadyTaskHint hint)
	{
		assert(computePlace == nullptr || computePlace->getType() == nanos6_host_device);

		// Runtime Tracking Point - Tasks will be added to the scheduler and will be ready
		TrackingPoints::enterAddReadyTasks(tasks, numTasks);

		_instance->addReadyTasks(taskType, tasks, numTasks, computePlace, hint);

		// Runtime Tracking Point - Exiting the addReadyTasks function
		TrackingPoints::exitAddReadyTasks();
	}

	static inline void addReadyTask(Task *task, ComputePlace *computePlace, ReadyTaskHint hint = NO_HINT)
	{
		assert(computePlace == nullptr || computePlace->getType() == nanos6_host_device);

		// Runtime Tracking Point - A task will be added to the scheduler and will be readys
		TrackingPoints::enterAddReadyTask(task);

		_instance->addReadyTask(task, computePlace, hint);

		// Runtime Tracking Point - Exiting the addReadyTasks function
		TrackingPoints::exitAddReadyTask();
	}

	static inline Task *getReadyTask(ComputePlace *computePlace)
	{
		assert(computePlace != nullptr);

		Instrument::enterGetReadyTask();
		Task *task = _instance->getReadyTask(computePlace);
		Instrument::exitGetReadyTask();

		return task;
	}

	//! \brief Check whether a compute place is serving tasks
	//!
	//! This function is called to check whether there is any
	//! compute place serving tasks. Notice that we require to
	//! have a compute place serving tasks except when there is
	//! work for all compute places. This information is considered
	//! when a compute place is about to be marked as idle. It
	//! should abort the idle process when detecting that there are
	//! no compute places serving tasks
	static inline bool isServingTasks()
	{
		return _instance->isServingTasks();
	}

	//! \brief Check whether task priority is considered
	static inline bool isPriorityEnabled()
	{
		return SchedulerInterface::isPriorityEnabled();
	}
};

#endif // SCHEDULER_HPP
