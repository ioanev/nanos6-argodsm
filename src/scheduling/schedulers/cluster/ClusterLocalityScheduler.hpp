/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019-2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef CLUSTER_LOCALITY_SCHEDULER_HPP
#define CLUSTER_LOCALITY_SCHEDULER_HPP

#include "scheduling/schedulers/cluster/ClusterSchedulerInterface.hpp"
#include "system/RuntimeInfo.hpp"

#include <ClusterManager.hpp>

class ClusterLocalityScheduler : public ClusterSchedulerInterface::ClusterSchedulerPolicy {
private:
	inline size_t getNodeIdForLocation(MemoryPlace const *location) const
	{
		if (location->getType() == nanos6_host_device) {
			return _interface->getThisNode()->getIndex();
		}

		return location->getIndex();
	}

	size_t next_node{0};
	std::mutex ft_mutex;

	size_t getNextFtNode(){
		std::lock_guard<std::mutex> lock(ft_mutex);
		size_t ret_val = next_node;
		next_node = (next_node+1) % nanos6_get_num_cluster_nodes();
		return ret_val;
	}


public:
	ClusterLocalityScheduler(ClusterSchedulerInterface * const interface)
		: ClusterSchedulerInterface::ClusterSchedulerPolicy("ClusterLocalityScheduler", interface)
	{
	}

	~ClusterLocalityScheduler()
	{
	}

	int getScheduledNode(
		Task *task,
		ComputePlace *computePlace,
		ReadyTaskHint hint
	) override;
};

static const bool __attribute__((unused))_registered_local_sched =
	ClusterSchedulerInterface::RegisterClusterSchedulerPolicy<ClusterLocalityScheduler>(nanos6_cluster_locality);

#endif // CLUSTER_LOCALITY_SCHEDULER_HPP
