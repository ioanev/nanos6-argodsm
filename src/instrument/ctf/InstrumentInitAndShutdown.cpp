/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "CTFTracepoints.hpp"
#include "InstrumentCPULocalData.hpp"
#include "InstrumentInitAndShutdown.hpp"
#include "ctfapi/CTFAPI.hpp"
#include "ctfapi/CTFKernelMetadata.hpp"
#include "ctfapi/CTFUserMetadata.hpp"
#include "ctfapi/CTFTrace.hpp"
#include "ctfapi/CTFTypes.hpp"
#include "ctfapi/stream/CTFStream.hpp"
#include "ctfapi/stream/CTFStreamUnboundedPrivate.hpp"
#include "ctfapi/stream/CTFStreamUnboundedShared.hpp"
#include "ctfapi/stream/CTFKernelStream.hpp"
#include "ctfapi/context/CTFContextTaskHardwareCounters.hpp"
#include "ctfapi/context/CTFContextCPUHardwareCounters.hpp"
#include "ctfapi/context/CTFStreamContextUnbounded.hpp"
#include "executors/threads/CPUManager.hpp"
#include "hardware-counters/HardwareCounters.hpp"
#include "tasks/TaskInfo.hpp"
#include "tasks/TasktypeData.hpp"


//static void refineCTFEvents(__attribute__((unused)) CTFAPI::CTFUserMetadata *metadata)
//{
//	// TODO perform refinement based on the upcoming Nanos6 JSON
//	// TODO add custom user-defined events based JSON
//}

static void initializeCTFEvents(CTFAPI::CTFUserMetadata *userMetadata)
{
	CTFAPI::CTFEventContext *ctfContextTaskHWC = nullptr;
	CTFAPI::CTFEventContext *ctfContextCPUHWC = nullptr;

	// Initialize Contexes
	if (HardwareCounters::hardwareCountersEnabled()) {
		ctfContextTaskHWC = userMetadata->addContext(
			new CTFAPI::CTFContextTaskHardwareCounters(CTFAPI::CTFStreamBoundedId)
		);
		ctfContextCPUHWC  = userMetadata->addContext(
			new CTFAPI::CTFContextCPUHardwareCounters(CTFAPI::CTFStreamBoundedId)
		);
	}

	// Add Contexes to evens that support them
	std::map<std::string, CTFAPI::CTFEvent *> &events = userMetadata->getEvents();
	for (auto it = events.begin(); it != events.end(); ++it) {
		CTFAPI::CTFEvent *event = it->second;
		uint8_t enabledContexes = event->getEnabledContexes();
		if (enabledContexes & CTFAPI::CTFContextTaskHWC) {
			event->addContext(ctfContextTaskHWC);
		} else if (enabledContexes & CTFAPI::CTFContextRuntimeHWC) {
			event->addContext(ctfContextCPUHWC);
		}
	}
}

static void initializeUserStreams(
	CTFAPI::CTFUserMetadata *userMetadata,
	std::string userPath
) {
	CPU *cpu;
	int nodeId;
	ctf_cpu_id_t cpuId;
	ctf_cpu_id_t maxCpuId = 0;
	std::vector<CPU *> cpus = CPUManager::getCPUListReference();
	ctf_cpu_id_t totalCPUs = (ctf_cpu_id_t) cpus.size();

	const size_t defaultStreamBufferSize = 2*1024*1024;
	//const size_t defaultUserBufferSize = 4096;
	//std::cout << "WARNING: buffer size set to " << defaultUserBufferSize << std::endl;

	// create and register contexes for streams
	CTFAPI::CTFStreamContextUnbounded *context = userMetadata->addContext(
		new CTFAPI::CTFStreamContextUnbounded()
	);

	// Initialize Worker thread streams
	for (ctf_cpu_id_t i = 0; i < totalCPUs; i++) {
		cpu = cpus[i];
		cpuId = cpu->getSystemCPUId();
		nodeId = cpu->getNumaNodeId();
		Instrument::CPULocalData &cpuLocalData = cpu->getInstrumentationData();
		cpuLocalData.userStream = new CTFAPI::CTFStream(
			defaultStreamBufferSize, cpuId, nodeId, userPath.c_str()
		);
		cpuLocalData.userStream->initialize();
		if (cpuId > maxCpuId)
			maxCpuId = cpuId;
	}

	// Initialize Leader Thread Stream
	cpuId = maxCpuId + 1;
	cpu = CPUManager::getLeaderThreadCPU();
	Instrument::CPULocalData &leaderThreadCPULocalData = cpu->getInstrumentationData();
	CTFAPI::CTFStreamUnboundedPrivate *unboundedPrivateStream = new CTFAPI::CTFStreamUnboundedPrivate(
		defaultStreamBufferSize, cpuId, -1, userPath.c_str()
	);
	unboundedPrivateStream->initialize();
	unboundedPrivateStream->addContext(context);
	leaderThreadCPULocalData.userStream = unboundedPrivateStream;

	// Initialize External Threads Stream
	cpuId = maxCpuId + 2;
	Instrument::CPULocalData *virtualCPULocalData = new Instrument::CPULocalData();
	CTFAPI::CTFStreamUnboundedShared *unboundedSharedStream = new CTFAPI::CTFStreamUnboundedShared(
		defaultStreamBufferSize, cpuId, -1, userPath.c_str()
	);
	unboundedSharedStream->initialize();
	unboundedSharedStream->addContext(context);
	virtualCPULocalData->userStream = unboundedSharedStream;
	Instrument::setCTFVirtualCPULocalData(virtualCPULocalData);
}

static void initializeKernelStreams(
	CTFAPI::CTFKernelMetadata *kernelMetadata,
	std::string kernelPath
) {
	CPU *cpu;
	int nodeId;
	ctf_cpu_id_t cpuId;

	if (!kernelMetadata->enabled())
		return;

	std::vector<CPU *> cpus = CPUManager::getCPUListReference();
	ctf_cpu_id_t totalCPUs = (ctf_cpu_id_t) cpus.size();

	//const size_t defaultKernelMappingSize = 8*1024*1024;
	//const size_t defaultStreamKernelSize = 2*defaultKernelMappingSize;

	const size_t defaultKernelMappingSize = 64*1024*1024;
	const size_t defaultStreamKernelSize = 2*defaultKernelMappingSize;

	// Set reference timestamp for all kernel streams
	CTFAPI::CTFTrace &trace = CTFAPI::CTFTrace::getInstance();
	uint64_t absoluteTimestamp = trace.getAbsoluteStartTimestamp();
	CTFAPI::CTFKernelStream::setReferenceTimestamp(absoluteTimestamp);

	// Set tracepoint definitions for all kernel streams
	CTFAPI::CTFKernelStream::setEvents(
		kernelMetadata->getEnabledEvents(),
		kernelMetadata->getEventSizes()
	);

	// Initialize per-cpu Kernel streams
	for (ctf_cpu_id_t i = 0; i < totalCPUs; i++) {
		cpu = cpus[i];
		cpuId = cpu->getSystemCPUId();
		nodeId = cpu->getNumaNodeId();
		Instrument::CPULocalData &cpuLocalData = cpu->getInstrumentationData();
		cpuLocalData.kernelStream = new CTFAPI::CTFKernelStream(
			defaultStreamKernelSize, defaultKernelMappingSize,
			cpuId, nodeId, kernelPath.c_str()
		);
		cpuLocalData.kernelStream->initialize();
	}

	// Enable kernel events on all cores
	for (ctf_cpu_id_t i = 0; i < totalCPUs; i++) {
		Instrument::CPULocalData &cpuLocalData = cpus[i]->getInstrumentationData();
		cpuLocalData.kernelStream->enableKernelEvents();
	}
}

void Instrument::initialize()
{
	std::string basePath, userPath, kernelPath;

	CTFAPI::CTFTrace &trace = CTFAPI::CTFTrace::getInstance();
	CTFAPI::CTFMetadata::collectCommonInformation();
	CTFAPI::CTFUserMetadata *userMetadata = new CTFAPI::CTFUserMetadata();
	CTFAPI::CTFKernelMetadata *kernelMetadata = new CTFAPI::CTFKernelMetadata();

	trace.setMetadata(userMetadata);
	trace.setKernelMetadata(kernelMetadata);
	trace.setTracePath(".");
	trace.initializeTraceTimer();
	trace.setTotalCPUs(CPUManager::getTotalCPUs());
	trace.createTraceDirectories(basePath, userPath, kernelPath);
	kernelMetadata->initialize();
	initializeUserStreams(userMetadata, userPath);
	initializeKernelStreams(kernelMetadata, kernelPath);

	preinitializeCTFEvents(userMetadata);
	userMetadata->refineEvents();
	initializeCTFEvents(userMetadata);
	userMetadata->writeMetadataFile(userPath);
	kernelMetadata->writeMetadataFile(kernelPath);
}

void Instrument::shutdown()
{
	CPU *cpu;
	std::vector<CPU *> cpus = CPUManager::getCPUListReference();
	ctf_cpu_id_t totalCPUs = (ctf_cpu_id_t) cpus.size();
	CTFAPI::CTFTrace &trace = CTFAPI::CTFTrace::getInstance();

	// First disable kernel tracing
	for (ctf_cpu_id_t i = 0; i < totalCPUs; i++) {
		cpu = cpus[i];
		CTFAPI::CTFKernelStream *kernelStream = cpu->getInstrumentationData().kernelStream;
		if (kernelStream) {
			kernelStream->disableKernelEvents();
		}
	}

	// Shutdown Worker thread streams
	for (ctf_cpu_id_t i = 0; i < totalCPUs; i++) {
		cpu = cpus[i];
		CTFAPI::CTFStream       *userStream   = cpu->getInstrumentationData().userStream;
		CTFAPI::CTFKernelStream *kernelStream = cpu->getInstrumentationData().kernelStream;
		assert(userStream != nullptr);
		assert(kernelStream != nullptr);

		if (kernelStream) {
			CTFAPI::updateKernelEvents(kernelStream, userStream);
			uint64_t lost = kernelStream->getLostEventsCount();
			kernelStream->shutdown();
			if (lost > 0) {
				FatalErrorHandler::warn(
					lost, " lost Linux Kernel events on core ",
					cpu->getSystemCPUId()
				);
			}
		}

		userStream->shutdown();
		delete userStream;
	}

	// Shutdown Leader thread stream
	cpu = CPUManager::getLeaderThreadCPU();
	Instrument::CPULocalData &leaderThreadCPULocalData = cpu->getInstrumentationData();
	CTFAPI::CTFStream *leaderThreadStream = leaderThreadCPULocalData.userStream;
	assert(leaderThreadStream != nullptr);
	leaderThreadStream->shutdown();
	delete leaderThreadStream;

	// Shutdown External thread stream
	CTFAPI::CTFStream *externalThreadStream = Instrument::getCTFVirtualCPULocalData()->userStream;
	assert(externalThreadStream != nullptr);
	externalThreadStream->shutdown();
	delete externalThreadStream;
	delete Instrument::getCTFVirtualCPULocalData();

	// move tracing files to final directory
	trace.convertToParaver();
	trace.moveTemporalTraceToFinalDirectory();
	trace.clean();

	// Disabling kernel tracing takes a considerable amount of time. Warn
	// the user of it.
	std::cout << "Shutting down Linux Kernel tracing facility, please wait " << std::flush;
	for (ctf_cpu_id_t i = 0; i < totalCPUs; i++) {
		cpu = cpus[i];
		CTFAPI::CTFKernelStream *kernelStream = cpu->getInstrumentationData().kernelStream;
		assert(kernelStream != nullptr);

		if (kernelStream) {
			delete kernelStream;
		}
	}
	std::cout << "[DONE]" << std::endl;
}

void Instrument::nanos6_preinit_finished()
{
	// emit an event per each registered task type with its label and source
	TaskInfo::processAllTasktypes(
		[&](const std::string &tasktypeLabel, const std::string &tasktypeSource, TasktypeData &tasktypeData) {
			TasktypeInstrument &instrumentId = tasktypeData.getInstrumentationId();
			ctf_tasktype_id_t tasktypeId = instrumentId.autoAssingId();
			tp_task_label(tasktypeLabel.c_str(), tasktypeSource.c_str(), tasktypeId);
		}
	);
}
