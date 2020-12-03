/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef CPU_THREADING_MODEL_DATA_HPP
#define CPU_THREADING_MODEL_DATA_HPP


#include <atomic>
#include <deque>

#include "support/config/ConfigVariable.hpp"


class CPU;
class WorkerThread;


struct CPUThreadingModelData {
private:
	static ConfigVariable<StringifiedMemorySize> _defaultThreadStackSize;

	friend class WorkerThreadBase;

public:
	CPUThreadingModelData()
	{
	}

	void initialize(CPU *cpu);

	static size_t getDefaultStackSize()
	{
		return (size_t) _defaultThreadStackSize.getValue();
	}
};


#endif // CPU_THREADING_MODEL_DATA_HPP
