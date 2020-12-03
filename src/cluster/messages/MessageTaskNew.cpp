/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019-2020 Barcelona Supercomputing Center (BSC)
*/

#include <nanos6.h>

#include "MessageTaskNew.hpp"
#include "system/ompss/SpawnFunction.hpp"

#include <TaskOffloading.hpp>


MessageTaskNew::MessageTaskNew(
	const ClusterNode *from,
	nanos6_task_info_t *taskInfo,
	nanos6_task_invocation_info_t *taskInvocationInfo,
	size_t flags,
	size_t numImplementations,
	nanos6_task_implementation_info_t *taskImplementations,
	size_t numSatInfo,
	const TaskOffloading::SatisfiabilityInfo *satInfo,
	size_t argsBlockSize,
	void *argsBlock,
	void *offloadedTaskId
) :
	Message("MessageTaskNew", TASK_NEW,
		sizeof(TaskNewMessageContent) +
		numImplementations * sizeof(nanos6_task_implementation_info_t) +
		numSatInfo * sizeof(TaskOffloading::SatisfiabilityInfo) +
		argsBlockSize, from)
{
	assert(taskInfo != nullptr);
	assert(taskInvocationInfo != nullptr);
	assert(taskImplementations != nullptr);
	assert(satInfo != nullptr || numSatInfo == 0);
	assert(argsBlock != nullptr || argsBlockSize == 0);

	_content = reinterpret_cast<TaskNewMessageContent *>(_deliverable->payload);
	memcpy(&_content->_taskInfo, taskInfo, sizeof(nanos6_task_info_t));
	memcpy(&_content->_taskInvocationInfo, taskInvocationInfo,
		sizeof(nanos6_task_invocation_info_t));
	memcpy(&_content->_flags, &flags, sizeof(flags));
	memcpy(&_content->_argsBlockSize, &argsBlockSize, sizeof(argsBlockSize));
	memcpy(&_content->_numImplementations, &numImplementations, sizeof(numImplementations));
	memcpy(&_content->_offloadedTaskId, &offloadedTaskId, sizeof(offloadedTaskId));
	memcpy(&_content->_numSatInfo, &numSatInfo, sizeof(numSatInfo));

	memcpy(getImplementationsPtr(), taskImplementations,
		numImplementations * sizeof(nanos6_task_implementation_info_t));

	if (satInfo != nullptr) {
		memcpy(getSatInfoPtr(), satInfo,
			numSatInfo * sizeof(TaskOffloading::SatisfiabilityInfo));
	}

	if (argsBlock != nullptr) {
		memcpy(getArgsBlockPtr(), argsBlock, argsBlockSize);
	}
}

static inline void remoteTaskWrapper(void *args)
{
	assert(args != nullptr);
	MessageTaskNew *msg = (MessageTaskNew *)args;

	TaskOffloading::remoteTaskWrapper(msg);
}

static inline void remoteTaskCleanup(void *args)
{
	assert(args != nullptr);
	MessageTaskNew *msg = (MessageTaskNew *)args;

	TaskOffloading::remoteTaskCleanup(msg);
}

bool MessageTaskNew::handleMessage()
{
	SpawnFunction::spawnFunction(
		remoteTaskWrapper, this,
		remoteTaskCleanup, this,
		"remote-task-wrapper",
		true
	);

	// The Message will be deleted by remoteTaskCleanup
	return false;
}

//! Register the Message type to the Object factory
static Message *createTaskNewMessage(Message::Deliverable *dlv)
{
	return new MessageTaskNew(dlv);
}

static const bool __attribute__((unused))_registered_tasknew =
	REGISTER_MSG_CLASS(TASK_NEW, createTaskNewMessage);
