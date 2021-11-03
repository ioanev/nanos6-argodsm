/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019 Barcelona Supercomputing Center (BSC)
*/

#ifndef MESSAGE_ARGO_RESET_STATS_HPP
#define MESSAGE_ARGO_RESET_STATS_HPP

#include <sstream>

#include "Message.hpp"

class MessageArgoResetStats : public Message {
public:
	MessageArgoResetStats(const ClusterNode *from);

	MessageArgoResetStats(Deliverable *dlv)
		: Message(dlv)
	{
	}

	bool handleMessage();

	//! \brief Return a string with a description of the Message
	inline std::string toString() const
	{
		return "Resetting ArgoDSM Statistics";
	}
};

#endif /* MESSAGE_ARGO_RESET_STATS_HPP */
