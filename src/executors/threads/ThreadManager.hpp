/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef THREAD_MANAGER_HPP
#define THREAD_MANAGER_HPP

#include <algorithm>
#include <atomic>
#include <cassert>
#include <set>
#include <stack>
#include <vector>

#include <pthread.h>
#include <unistd.h>

#include <hardware/HardwareInfo.hpp>
#include "hardware/places/ComputePlace.hpp"
#include "lowlevel/SpinLock.hpp"

#include "CPU.hpp"
#include "WorkerThread.hpp"


class ThreadManagerDebuggingInterface;


class ThreadManager {
private:
	struct IdleThreads {
		SpinLock _lock;
		std::deque<WorkerThread *> _threads;
	};
	struct ShutdownThreads {
		SpinLock _lock;
		std::deque<WorkerThread *> _threads;
	};

	//! \brief threads blocked due to idleness by NUMA node
	static IdleThreads *_idleThreads;

	//! \brief number of threads in the system
	static std::atomic<long> _totalThreads;

	//! \brief threads that already completed the shutdown process
	static ShutdownThreads *_shutdownThreads;


public:
	static void initialize();
	static void shutdownPhase1();
	static void shutdownPhase2();


	//! \brief create a WorkerThread
	//! The thread is returned in a blocked (or about to block) status
	//!
	//! \param[in,out] cpu the CPU on which to get a new thread
	//!
	//! \returns a WorkerThread
	static inline WorkerThread *createWorkerThread(CPU *cpu);

	//! \brief create or recycle a WorkerThread
	//! The thread is returned in a blocked (or about to block) status
	//!
	//! \param[in,out] cpu the CPU on which to get a new or idle thread (advisory only)
	//! \param[in] doNotCreate true to avoid creating additional threads in case that none is available
	//!
	//! \returns a WorkerThread or nullptr
	static inline WorkerThread *getIdleThread(CPU *cpu, bool doNotCreate=false);

	//! \brief get any remaining idle thread
	static inline WorkerThread *getAnyIdleThread();

	//! \brief add a thread to the list of idle threads
	//!
	//! \param[in] idleThread a thread that has become idle
	static inline void addIdler(WorkerThread *idleThread);

	//! \brief resume an idle thread on a given CPU
	//!
	//! \param[in] idleCPU the CPU on which to resume an idle thread
	//! \param[in] inInitializationOrShutdown true if it should not enforce assertions that are not valid during initialization and shutdown
	//! \param[in] doNotCreate true to avoid creating additional threads in case that none is available
	//!
	//! \returns the thread that has been resumed or nullptr
	static inline WorkerThread *resumeIdle(CPU *idleCPU, bool inInitializationOrShutdown=false, bool doNotCreate=false);

	static inline void resumeIdle(const std::vector<CPU *> &idleCPUs, bool inInitializationOrShutdown=false, bool doNotCreate=false);

	static void addShutdownThread(WorkerThread *shutdownThread);

	friend class ThreadManagerDebuggingInterface;
	friend struct CPUThreadingModelData;
};


inline WorkerThread *ThreadManager::createWorkerThread(CPU *cpu)
{
	assert(cpu != nullptr);

	// The runtime cannot be shutting down when creating a thread
	assert(cpu->getActivationStatus() != CPU::shutting_down_status);

	// Create a new thread
	_totalThreads++;

	// The runtime cannot be shutting down when creating a thread
	assert(cpu->getActivationStatus() != CPU::shutting_down_status);

	return new WorkerThread(cpu);
}


inline WorkerThread *ThreadManager::getIdleThread(CPU *cpu, bool doNotCreate)
{
	assert(cpu != nullptr);

	// Try to recycle an idle thread
	{
		IdleThreads &idleThreads = _idleThreads[cpu->getNumaNodeId()];

		std::lock_guard<SpinLock> guard(idleThreads._lock);
		if (!idleThreads._threads.empty()) {
			WorkerThread *idleThread = idleThreads._threads.front();
			idleThreads._threads.pop_front();

			assert(idleThread != nullptr);
			assert(idleThread->getTask() == nullptr);

			return idleThread;
		}
	}

	if (doNotCreate) {
		return nullptr;
	}

	return createWorkerThread(cpu);
}


inline WorkerThread *ThreadManager::getAnyIdleThread()
{
	size_t numNumaNodes = HardwareInfo::getMemoryPlaceCount(nanos6_device_t::nanos6_host_device);
	for (size_t i = 0; i < numNumaNodes; i++) {
		IdleThreads &idleThreads = _idleThreads[i];

		std::lock_guard<SpinLock> guard(idleThreads._lock);
		if (!idleThreads._threads.empty()) {
			WorkerThread *idleThread = idleThreads._threads.front();
			idleThreads._threads.pop_front();

			assert(idleThread != nullptr);
			assert(idleThread->getTask() == nullptr);

			return idleThread;
		}
	}

	return nullptr;
}


inline void ThreadManager::addIdler(WorkerThread *idleThread)
{
	assert(idleThread != nullptr);

	// Make sure this thread has no task assigned before idling
	assert(idleThread->getTask() == nullptr);

	// Return the current thread to the idle list
	{
		size_t numaNode = idleThread->getOriginalNumaNode();
		IdleThreads &idleThreads = _idleThreads[numaNode];

		std::lock_guard<SpinLock> guard(idleThreads._lock);

		assert(std::find(idleThreads._threads.begin(), idleThreads._threads.end(), idleThread) == idleThreads._threads.end());
		idleThreads._threads.push_front(idleThread);
	}
}


inline WorkerThread *ThreadManager::resumeIdle(CPU *idleCPU, bool inInitializationOrShutdown, bool doNotCreate)
{
	assert(idleCPU != nullptr);

	// Get an idle thread for the CPU
	WorkerThread *idleThread = getIdleThread(idleCPU, doNotCreate);

	if (idleThread == nullptr) {
		return nullptr;
	}

	idleThread->resume(idleCPU, inInitializationOrShutdown);

	return idleThread;
}


inline void ThreadManager::resumeIdle(const std::vector<CPU *> &idleCPUs, bool inInitializationOrShutdown, bool doNotCreate)
{
	for (CPU *idleCPU : idleCPUs) {
		assert(idleCPU != nullptr);

		// Get an idle thread for the CPU
		WorkerThread *idleThread = getIdleThread(idleCPU, doNotCreate);

		if (idleThread != nullptr) {
			idleThread->resume(idleCPU, inInitializationOrShutdown);
		}
	}
}


#endif // THREAD_MANAGER_HPP
