#ifndef ARGO_MESSENGER_HPP
#define ARGO_MESSENGER_HPP

#include <sstream>
#include <vector>

#pragma GCC visibility push(default)
#include <mpi.h>
#pragma GCC visibility pop

#include "../Messenger.hpp"
#include "lowlevel/PaddedTicketSpinLock.hpp"

class ClusterPlace;
class DataTransfer;

class ArgoMessenger : public Messenger {
private:

#ifdef EXTRAE_ENABLED
	PaddedTicketSpinLock<int> _lockExtrae;
#endif

	bool _mpi_comm_data_raw = 0;

	// Default value useful for asserts
	int _wrank = -1, _wsize = -1;
	MPI_Comm INTRA_COMM, INTRA_COMM_DATA_RAW, PARENT_COMM;

	// Upper bound MPI tag supported by current implementation,
	// used for masking MPI tags to prevent out-of-range MPI
	// tags when sending/receiving large number of messages.
	int _mpi_ub_tag = 0;

	int createTag(const Message::Deliverable *delv) const
	{
		return _mpi_ub_tag & ((delv->header.id << 8) | delv->header.type);
	}

	void ExtraeLock()
	{
#ifdef EXTRAE_ENABLED
		_lockExtrae.lock();
#endif
	}

	void ExtraeUnlock()
	{
#ifdef EXTRAE_ENABLED
		_lockExtrae.unlock();
#endif
	}

	int getTag(int messageId) const
	{
		return _mpi_ub_tag & ((messageId << 8) | DATA_RAW);
	}


	template <typename T>
	class RequestContainer
	{
		static size_t maxCount;
		static MPI_Request *requests;
		static int *finished;
		static MPI_Status *status;

		static void clear()
		{
			maxCount = 0;
			free(requests);
			free(finished);
			free(status);

			// Make it defensive.
			requests = nullptr;
			finished = nullptr;
			status = nullptr;
		}

		static void reserve(size_t size)
		{
			if (maxCount < size) {
				clear();

				maxCount = size;

				requests = (MPI_Request *) malloc(maxCount * sizeof(MPI_Request));
				FatalErrorHandler::failIf(requests == nullptr,
					"Could not allocate memory for requests in testCompletionInternal");

				finished = (int *) malloc(maxCount * sizeof(int));
				FatalErrorHandler::failIf(finished == nullptr,
					"Could not allocate memory for finished in testCompletionInternal");

				status = (MPI_Status *) malloc(maxCount * sizeof(MPI_Status));
				FatalErrorHandler::failIf(status == nullptr,
					"Could not allocate memory for status array in testCompletionInternal");

			}
		}

		friend class ArgoMessenger;
	};

	template <typename T>
	void testCompletionInternal(std::vector<T *> &pending);
	
public:

	ArgoMessenger();
	~ArgoMessenger();

	void sendMessage(Message *msg, ClusterNode const *toNode, bool block = false) override;

	void synchronizeAll(void) override;

	DataTransfer *sendData(
		const DataAccessRegion &region,
		const ClusterNode *toNode,
		int messageId, bool block,
		bool instrument) override;

	DataTransfer *fetchData(
		const DataAccessRegion &region,
		const ClusterNode *fromNode,
		int messageId,
		bool block,
		bool instrument) override;

	Message *checkMail() override;

	inline void testCompletion(std::vector<Message *> &pending) override
	{
		testCompletionInternal<Message>(pending);
	}

	inline void testCompletion(std::vector<DataTransfer *> &pending) override
	{
		testCompletionInternal<DataTransfer>(pending);
	}

	inline int getNodeIndex() const override
	{
		assert(_wrank >= 0);
		return _wrank;
	}

	inline int getMasterIndex() const override
	{
		return 0;
	}

	inline int getClusterSize() const override
	{
		assert(_wsize > 0);
		return _wsize;
	}

	inline bool isMasterNode() const override
	{
		assert(_wrank >= 0);
		return _wrank == 0;
	}
};

//! Register MPIMessenger with the object factory
namespace
{
	const bool __attribute__((unused))_registered_argo_msn =
		Messenger::RegisterMSNClass<ArgoMessenger>("argodsm");
}


template <typename T> size_t ArgoMessenger::RequestContainer<T>::maxCount = 0;
template <typename T> MPI_Request *ArgoMessenger::RequestContainer<T>::requests = nullptr;
template <typename T> int *ArgoMessenger::RequestContainer<T>::finished = nullptr;
template <typename T> MPI_Status *ArgoMessenger::RequestContainer<T>::status = nullptr;


#endif /* ARGO_MESSENGER_HPP */
