/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019 Barcelona Supercomputing Center (BSC)
*/

#include "MessageArgoUpgradeWriters.hpp"

#include <argo/argo.hpp>
#include "lowlevel/EnvironmentVariable.hpp"

MessageArgoUpgradeWriters::MessageArgoUpgradeWriters(const ClusterNode *from)
	: Message(ARGO_UPGRADE_WRITERS, 1, from)
{}

bool MessageArgoUpgradeWriters::handleMessage()
{
	// Get communicator type
	ConfigVariable<std::string> commType("cluster.communication");
	if(commType.getValue() == "argodsm"){
		argo::barrier_upgrade_writers();
	}
	return true;
}

static const bool __attribute__((unused))_registered_argo_upgrade_writers =
	Message::RegisterMSGClass<MessageArgoUpgradeWriters>(ARGO_UPGRADE_WRITERS);
