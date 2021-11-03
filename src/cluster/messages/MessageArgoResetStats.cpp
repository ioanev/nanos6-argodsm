/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019 Barcelona Supercomputing Center (BSC)
*/

#include "MessageArgoResetStats.hpp"

#include <argo/argo.hpp>
#include "lowlevel/EnvironmentVariable.hpp"

MessageArgoResetStats::MessageArgoResetStats(const ClusterNode *from)
	: Message(ARGO_RESET_STATS, 1, from)
{}

bool MessageArgoResetStats::handleMessage()
{
	// Get communicator type
	ConfigVariable<std::string> commType("cluster.communication");
	if(commType.getValue() == "argodsm"){
		argo::backend::release();		//Empty write buffers
		argo::backend::reset_stats();	//Reset statistics
	}
	return true;
}

static const bool __attribute__((unused))_registered_argo_reset_stats =
	Message::RegisterMSGClass<MessageArgoResetStats>(ARGO_RESET_STATS);
