/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019-2020 Barcelona Supercomputing Center (BSC)
*/

#include "ClusterRandomScheduler.hpp"

#include <ClusterManager.hpp>
#include <DataAccessRegistrationImplementation.hpp>
#include <ExecutionWorkflow.hpp>
#include <VirtualMemoryManagement.hpp>

int ClusterRandomScheduler::getScheduledNode(
	Task *task,
	ComputePlace *computePlace __attribute__((unused)),
	ReadyTaskHint hint __attribute__((unused))
) {

	bool canBeOffloaded = true;
	DataAccessRegistration::processAllDataAccesses(task,
		[&](const DataAccess *access) -> bool {
			DataAccessRegion region = access->getAccessRegion();
			if (!VirtualMemoryManagement::isClusterMemory(region) &&
				!argo::is_argo_address(region.getStartAddress())) {
				canBeOffloaded = false;
				return false;
			}
			return true;
		}
	);

	if (!canBeOffloaded) {
		return nanos6_cluster_no_offload;
	}

	return distr(eng);
}
