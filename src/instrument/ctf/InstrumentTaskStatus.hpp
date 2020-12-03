/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef INSTRUMENT_CTF_TASK_STATUS_HPP
#define INSTRUMENT_CTF_TASK_STATUS_HPP

#include "CTFTracepoints.hpp"

#include "instrument/api/InstrumentTaskStatus.hpp"


namespace Instrument {
	inline void taskIsPending(
		__attribute__((unused)) task_id_t taskId,
		__attribute__((unused)) InstrumentationContext const &context
	) {
	}

	inline void taskIsReady(
		__attribute__((unused)) task_id_t taskId,
		__attribute__((unused)) InstrumentationContext const &context
	) {
	}

	inline void taskIsExecuting(
		__attribute__((unused)) task_id_t taskId,
		bool wasBlocked,
		__attribute__((unused)) InstrumentationContext const &context
	) {
		if (wasBlocked) {
			tp_task_unblock();
		}
	}

	inline void taskIsBlocked(
		__attribute__((unused)) task_id_t taskId,
		__attribute__((unused)) task_blocking_reason_t reason,
		__attribute__((unused)) InstrumentationContext const &context
	) {
		tp_task_block();
	}

	inline void taskIsZombie(
		__attribute__((unused)) task_id_t taskId,
		__attribute__((unused)) InstrumentationContext const &context
	) {
	}

	inline void taskIsBeingDeleted(
		__attribute__((unused)) task_id_t taskId,
		__attribute__((unused)) InstrumentationContext const &context
	) {
	}

	inline void taskHasNewPriority(
		__attribute__((unused)) task_id_t taskId,
		__attribute__((unused)) long priority,
		__attribute__((unused)) InstrumentationContext const &context
	) {
	}

	inline void taskforCollaboratorIsExecuting(
		__attribute__((unused)) task_id_t taskforId,
		__attribute__((unused)) task_id_t collaboratorId,
		__attribute__((unused)) InstrumentationContext const &context
	) {
	}

	inline void taskforCollaboratorStopped(
		__attribute__((unused)) task_id_t taskforId,
		__attribute__((unused)) task_id_t collaboratorId,
		__attribute__((unused)) InstrumentationContext const &context
	) {
	}
}


#endif // INSTRUMENT_CTF_TASK_STATUS_HPP
