/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>

#include "CPUThreadingModelData.hpp"
#include "executors/threads/ThreadManager.hpp"
#include "executors/threads/WorkerThread.hpp"
#include "system/RuntimeInfo.hpp"


ConfigVariable<StringifiedMemorySize> CPUThreadingModelData::_defaultThreadStackSize("misc.stack_size");


void CPUThreadingModelData::initialize(__attribute__((unused)) CPU *cpu)
{
	static std::atomic<bool> firstTime(true);
	bool expect = true;
	bool worked = firstTime.compare_exchange_strong(expect, false);
	if (worked) {
		RuntimeInfo::addEntry("threading_model", "Threading Model", "pthreads");
		RuntimeInfo::addEntry("stack_size", "Stack Size", getDefaultStackSize());
	}
}
