/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019 Barcelona Supercomputing Center (BSC)
*/

#include "MessageSatisfiability.hpp"

#include <ClusterManager.hpp>
#include <TaskOffloading.hpp>

MessageSatisfiability::MessageSatisfiability(const ClusterNode *from,
		void *offloadedTaskId,
		TaskOffloading::SatisfiabilityInfo const &satInfo)
	: Message(SATISFIABILITY, sizeof(SatisfiabilityMessageContent), from)
{
	_content = reinterpret_cast<SatisfiabilityMessageContent *>(_deliverable->payload);
	_content->_offloadedTaskId = offloadedTaskId;
	_content->_satInfo = satInfo;
}

bool MessageSatisfiability::handleMessage()
{
	ClusterNode *offloader = ClusterManager::getClusterNode(getSenderId());

	TaskOffloading::propagateSatisfiabilityForHandler(
		_content->_offloadedTaskId,
		offloader,
		_content->_satInfo
	);

	return true;
}


static const bool __attribute__((unused))_registered_satisfiability =
	Message::RegisterMSGClass<MessageSatisfiability>(SATISFIABILITY);
