/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2018-2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef MESSENGER_HPP
#define MESSENGER_HPP

#include <deque>
#include <string>
#include <vector>
#include <type_traits>
#include <functional>

#include <Message.hpp>
#include <DataAccessRegion.hpp>
#include <support/GenericFactory.hpp>

class ClusterNode;
class DataTransfer;

class Messenger {
public:
	Messenger()
	{
	}

	virtual ~Messenger()
	{
	}

	template<typename T>
	static bool RegisterMSNClass(const std::string &name)
	{
		static_assert(std::is_base_of<Messenger, T>::value, "Base class type is wrong.");
		return GenericFactory<std::string, Messenger*>::getInstance().emplace(
			name,
			[]() -> Messenger* {
				return new T();
			}
		);
	}

	//! \brief Send a message to a remote node
	//!
	//! \param[in] msg is the Message to send
	//! \param[in] toNode is the receiver node
	//! \param[in] block determines if the call will block until Message delivery
	virtual void sendMessage(Message *msg, ClusterNode const *toNode, bool block = false) = 0;

	//! \brief A barrier across all nodes
	//!
	//! This is a collective operation that needs to be invoked
	//! by all nodes
	virtual void synchronizeAll(void) = 0;

	//! \brief Send a data region to a remote node, related to a previous message.
	//!
	//! \param[in] region is the data region to send
	//! \param[in] toNode is the receiver node
	//! \param[in] messageId is the id of the Message related with this
	//!		data transfer
	//! \param[in] if block is true then the call will block until the data
	//!		is sent
	//!
	//! \returns A DataTransfer object representing the pending data
	//!		transfer if the data is sent in non-blocking mode,
	//!		otherwise nullptr
	virtual DataTransfer *sendData(
		const DataAccessRegion &region,
		const ClusterNode *toNode,
		int messageId,
		bool block,
		bool instrument
	) = 0;

	//! \brief Receive a data region from a remote node, related to a previous message
	//!
	//! \param[in] region is the data region to fetch
	//! \param[in] fromNode is the node to fetch the data from
	//!		with this data transfer
	//! \param[in] messageId is the id of the Message related with this
	//!		data transfer
	//! \param[in] if block is true then the call will block until the data
	//!		is received
	//!
	//! \returns A DataTransfer object representing the pending data
	//!		transfer if the data is sent in non-blocking mode,
	//!		otherwise nullptr
	virtual DataTransfer *fetchData(
		const DataAccessRegion &region,
		const ClusterNode *fromNode,
		int messageId,
		bool block,
		bool instrument
	) = 0;

	//! \brief Check for incoming messages
	//!
	//! Invoke the messenger to check from incoming messages
	//!
	//! \return A pointer to a message or nullptr if none has been received
	virtual Message *checkMail() = 0;

	//! Get the index of the current node
	virtual int getNodeIndex() const = 0;

	//! Get the index of the master node
	virtual int getMasterIndex() const = 0;

	//! Get the size of the Cluster
	virtual int getClusterSize() const = 0;

	//! Returns true if this is the master node
	virtual bool isMasterNode() const = 0;

	//! \brief Test if sending Messages has completed
	//!
	//! This tests whether messages stored in the 'messages'
	//! queue has been succesfully sent. All succesfully sent
	//! messages are marked as completed
	//!
	//! \param[in] messages holds the pending outgoing messages
	virtual void testCompletion(std::vector<Message *> &pendings) = 0;
	virtual void testCompletion(std::vector<DataTransfer *> &pendings) = 0;

};

#endif /* MESSENGER_HPP */
