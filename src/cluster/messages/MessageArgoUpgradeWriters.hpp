/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019 Barcelona Supercomputing Center (BSC)
*/

#ifndef MESSAGE_ARGO_UPGRADE_WRITERS_HPP
#define MESSAGE_ARGO_UPGRADE_WRITERS_HPP

#include <sstream>

#include "Message.hpp"

class MessageArgoUpgradeWriters : public Message {
public:
	MessageArgoUpgradeWriters(const ClusterNode *from);

	MessageArgoUpgradeWriters(Deliverable *dlv)
		: Message(dlv)
	{
	}

	bool handleMessage();

	//! \brief Return a string with a description of the Message
	inline std::string toString() const
	{
		return "Upgrading ArgoDSM classification";
	}
};

#endif /* MESSAGE_ARGO_UPGRADE_WRITERS_HPP */
