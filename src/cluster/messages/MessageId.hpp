/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2018 Barcelona Supercomputing Center (BSC)
*/

#ifndef MESSAGE_ID_HPP
#define MESSAGE_ID_HPP

#include <atomic>

#include "MessageType.hpp"

namespace MessageId {

	//! \brief Initialize globally unique MessageIds
	void initialize(int rank, int numRanks);

	//! \brief Get the next available MessageId
	uint32_t nextMessageId();
}

#endif /* MESSAGE_ID_HPP */
