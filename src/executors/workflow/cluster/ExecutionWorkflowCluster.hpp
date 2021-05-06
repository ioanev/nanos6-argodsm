/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019 Barcelona Supercomputing Center (BSC)
*/

#ifndef EXECUTION_WORKFLOW_CLUSTER_HPP
#define EXECUTION_WORKFLOW_CLUSTER_HPP

#include <functional>

#include "../ExecutionStep.hpp"

#include <ClusterManager.hpp>
#include <ClusterTaskContext.hpp>
#include <DataAccess.hpp>
#include <Directory.hpp>
#include <InstrumentLogMessage.hpp>
#include <SatisfiabilityInfo.hpp>
#include <TaskOffloading.hpp>
#include <VirtualMemoryManagement.hpp>
#include <tasks/Task.hpp>
#include <ClusterUtil.hpp>
#include <DataAccessRegistration.hpp>

#include <argo/argo.hpp>
#include <argo/virtual_memory/virtual_memory.hpp>
#include <argo/backend/mpi/coherence.hpp>

class ComputePlace;
class MemoryPlace;

namespace ExecutionWorkflow {
	class ClusterDataLinkStep : public DataLinkStep {
		//! The MemoryPlace that holds the data at the moment
		MemoryPlace const *_sourceMemoryPlace;

		//! The MemoryPlace that requires the data
		MemoryPlace const *_targetMemoryPlace;

		//! DataAccessRegion that the Step covers
		DataAccessRegion _region;

		//! The task in which the access belongs to
		Task *_task;

		//! read satisfiability at creation time
		bool _read;

		//! write satisfiability at creation time
		bool _write;

		Task *_namespacePredecessor;
		WriteID _writeID;

		bool _started;

	public:
		ClusterDataLinkStep(
			MemoryPlace const *sourceMemoryPlace,
			MemoryPlace const *targetMemoryPlace,
			DataAccess *access
		) : DataLinkStep(access),
			_sourceMemoryPlace(sourceMemoryPlace),
			_targetMemoryPlace(targetMemoryPlace),
			_region(access->getAccessRegion()),
			_task(access->getOriginator()),
			_read(access->readSatisfied()),
			_write(access->writeSatisfied()),
			_namespacePredecessor(nullptr),
			_writeID(access->getWriteID()),
			_started(false)
		{
			access->setDataLinkStep(this);

			assert(targetMemoryPlace->getType() == nanos6_device_t::nanos6_cluster_device);
			int targetNamespace = targetMemoryPlace->getIndex();

			/* Starting workflow on another node: set the namespace and predecessor task */
			if (ClusterManager::getDisableRemote()) {
				_namespacePredecessor = nullptr;
			} else {
				if (access->getValidNamespacePrevious() == targetNamespace) {
					_namespacePredecessor = access->getNamespacePredecessor(); // remote propagation valid if predecessor task and offloading node matches
				} else {
					_namespacePredecessor = nullptr;
				}
			}

			DataAccessRegistration::setNamespaceSelf(access, targetNamespace);
		}

		void linkRegion(
			DataAccessRegion const &region,
			MemoryPlace const *location,
			WriteID writeID,
			bool read,
			bool write
		) override;

		//! Start the execution of the Step
		void start() override;
	};

	class ClusterDataCopyStep : public Step {
		//! The MemoryPlace that the data will be copied from.
		MemoryPlace const *_sourceMemoryPlace;

		//! The MemoryPlace that the data will be copied to.
		MemoryPlace const *_targetMemoryPlace;

		//! A mapping of the address range in the source node to the target node.
		DataAccessRegion const _fullRegion;
		std::vector<DataAccessRegion> _regionsFragments;

		//! The task on behalf of which we perform the data copy
		Task *_task;

		WriteID _writeID;

		//! The data copy is for a taskwait
		bool _isTaskwait;

		//! The access is weak
		bool _isWeak;

		//! An actual data transfer is required
		bool _needsTransfer;

		//! Number of fragments messages
		size_t _nFragments;

		DataTransfer::data_transfer_callback_t _postcallback;

	public:
		ClusterDataCopyStep(
			MemoryPlace const *sourceMemoryPlace,
			MemoryPlace const *targetMemoryPlace,
			DataAccessRegion const &region,
			Task *task,
			WriteID writeID,
			bool isTaskwait,
			bool isWeak,
			bool needsTransfer
		);

		//! Start the execution of the Step
		void start() override
		{
		};

		bool requiresDataFetch();

		MemoryPlace const *getSourceMemoryPlace() const
		{
			return _sourceMemoryPlace ;
		}

		MemoryPlace const *getTargetMemoryPlace() const
		{
			return _targetMemoryPlace ;
		}

		size_t getNumFragments() const
		{
			return _nFragments;
		}

		const std::vector<DataAccessRegion> &getFragments() const
		{
			return _regionsFragments;
		}

		DataTransfer::data_transfer_callback_t getPostCallback() const
		{
			return _postcallback;
		}
	};

	class ClusterDataReleaseStep : public DataReleaseStep {
		//! identifier of the remote task
		void *_remoteTaskIdentifier;

		//! the cluster node we need to notify
		ClusterNode const *_offloader;

	public:
		ClusterDataReleaseStep(TaskOffloading::ClusterTaskContext *context, Task *task)
			: DataReleaseStep(task),
			_remoteTaskIdentifier(context->getRemoteIdentifier()),
			_offloader(context->getRemoteNode())
		{
			task->setDataReleaseStep(this);
		}

		void addAccess(DataAccess *access)
		{
			_bytesToRelease += access->getAccessRegion().getSize();
		}


		void releaseRegion(
			DataAccessRegion const &region,
			WriteID writeID,
			MemoryPlace const *location) override
		{
			/*
			 * location == nullptr means that the access was propagated in this node's
			 * namespace rather than being released to the offloader. This means that
			 * the RELEASE_ACCESS message should not be sent. This function is still
			 * called so that the workflow step can be deleted once all accesses are
			 * accounted for.
			 */
			if (location != nullptr) {
				Instrument::logMessage(
					Instrument::ThreadInstrumentationContext::getCurrent(),
					"releasing remote region:", region
				);

				TaskOffloading::sendRemoteAccessRelease(
					_remoteTaskIdentifier, _offloader, region, writeID, location
				);
			}

			_bytesToRelease -= region.getSize();
			if (_bytesToRelease == 0) {
				delete this;
			}
		}

		bool checkDataRelease(DataAccess const *access) override
		{
			Task *task = access->getOriginator();

			const bool mustWait = task->mustDelayRelease() && !task->allChildrenHaveFinished();

			const bool releases = ( (access->getObjectType() == taskwait_type) // top level sink
			                        || !access->hasSubaccesses()) // or no fragments (i.e. no subtask to wait for)
				&& task->hasFinished()     // must have finished; i.e. not taskwait inside task
				&& access->readSatisfied() && access->writeSatisfied()
				&& access->getOriginator()->isRemoteTask()  // only offloaded tasks: necessary (e.g. otherwise taskwait on will release)
				&& access->complete()                       // access must be complete
				&& !access->hasNext()                       // no next access at the remote side
				&& !mustWait;

			Instrument::logMessage(
				Instrument::ThreadInstrumentationContext::getCurrent(),
				"Checking DataRelease access:", access->getInstrumentationId(),
				" object_type:", access->getObjectType(),
				" spawned originator:", access->getOriginator()->isSpawned(),
				" read:", access->readSatisfied(),
				" write:", access->writeSatisfied(),
				" complete:", access->complete(),
				" has-next:", access->hasNext(),
				" task finished:", task->hasFinished(),
				" releases:", releases
			);

			return releases;
		}

		void start() override
		{
			releaseSuccessors();
		}
	};

	class ClusterExecutionStep : public Step {
	private:
		std::vector<TaskOffloading::SatisfiabilityInfo> _satInfo;
		ClusterNode *_remoteNode;
		Task *_task;

	public:
		ClusterExecutionStep(Task *task, ComputePlace *computePlace);

		//! Inform the execution Step about the existence of a
		//! pending data copy.
		//!
		//! \param[in] source is the id of the MemoryPlace that the data
		//!            is currently located
		//! \param[in] region is the memory region being copied
		//! \param[in] size is the size of the region being copied.
		//! \param[in] read is true if access is read-satisfied
		//! \param[in] write is true if access is write-satisfied
		//! \param[in] namespacePredecessorId is nullptr or predecessor remote task ID
		void addDataLink(int source, DataAccessRegion const &region, WriteID writeID, bool read, bool write, void *namespacePredecessorId);

		//! Start the execution of the Step
		void start() override;
	};

	class ClusterNotificationStep : public Step {
	private:
		std::function<void ()> const _callback;

	public:
		ClusterNotificationStep(std::function<void ()> const &callback)
			: Step(), _callback(callback)
		{
		}

		//! Start the execution of the Step
		void start() override;
	};

	class ArgoAcquireStep : public Step {
		//! The MemoryPlace that the data will be copied from.
		MemoryPlace const *_sourceMemoryPlace;

		//! The MemoryPlace that the data will be copied to.
		MemoryPlace const *_targetMemoryPlace;

		//! The DataAccessRegion corresponding to this data release
		DataAccessRegion _region;

	public:
		ArgoAcquireStep(
			MemoryPlace const *sourceMemoryPlace,
			MemoryPlace const *targetMemoryPlace,
			DataAccessRegion const &region
		) : Step(),
			_sourceMemoryPlace(sourceMemoryPlace),
			_targetMemoryPlace(targetMemoryPlace),
			_region(region)
		{
		}

		void start();
	};

	class ArgoReleaseStepLocal : public DataReleaseStep {
		DataAccess *_dataAccess;

		public:
		ArgoReleaseStepLocal(
				DataAccess *access
				) : DataReleaseStep(access),
		_dataAccess(access)
		{
		}

		// Overload start with ArgoDSM functionality
		void start();
	};


	class ArgoReleaseStep : public DataReleaseStep {
		//! identifier of the remote task
		void *_remoteTaskIdentifier;

		//! the cluster node we need to notify
		ClusterNode const *_offloader;

		public:
			ArgoReleaseStep(
				TaskOffloading::ClusterTaskContext *context,
				DataAccess *access
			) : DataReleaseStep(access),
				_remoteTaskIdentifier(context->getRemoteIdentifier()),
				_offloader(context->getRemoteNode())
			{
				access->setDataReleaseStep(this);
			}

		void releaseRegion(DataAccessRegion const &region,
				MemoryPlace const *location);

		bool checkDataRelease(DataAccess const *access);

		void start();
	};


	class ArgoDataLinkStep : public DataLinkStep {
		//! The MemoryPlace that holds the data at the moment
		MemoryPlace const *_sourceMemoryPlace;

		//! The MemoryPlace that requires the data
		MemoryPlace const *_targetMemoryPlace;

		//! DataAccessRegion that the Step covers
		DataAccessRegion _region;

		//! The task in which the access belongs to
		Task *_task;

		//! read satisfiability at creation time
		bool _read;

		//! write satisfiability at creation time
		bool _write;

		public:
		ArgoDataLinkStep(
				MemoryPlace const *sourceMemoryPlace,
				MemoryPlace const *targetMemoryPlace,
				DataAccess *access
				) : DataLinkStep(access),
		_sourceMemoryPlace(sourceMemoryPlace),
		_targetMemoryPlace(targetMemoryPlace),
		_region(access->getAccessRegion()),
		_task(access->getOriginator()),
		_read(access->readSatisfied()),
		_write(access->writeSatisfied())
		{
			access->setDataLinkStep(this);
		}

		void linkRegion(
				DataAccessRegion const &region,
				MemoryPlace const *location,
				bool read,
				bool write
				);

		//! Start the execution of the Step
		void start();
	};

	inline Step *clusterFetchData(
		MemoryPlace const *source,
		MemoryPlace const *target,
		DataAccessRegion const &inregion,
		DataAccess *access
	) {
		assert(source != nullptr);
		nanos6_device_t sourceType = source->getType();
		assert(target == ClusterManager::getCurrentMemoryNode());

		//! Currently, we cannot have a cluster data copy where the source
		//! location is in the Directory. This would mean that the data
		//! have not been written yet (that's why they're not in a
		//! non-directory location), so we are reading something that is
		//! not initialized yet
		assert(!Directory::isDirectoryMemoryPlace(source) &&
			"You're probably trying to read something "
			"that has not been initialized yet!"
		);

		assert(source->getType() == nanos6_cluster_device);
		DataAccessObjectType objectType = access->getObjectType();
		DataAccessType type = access->getType();
		DataAccessRegion region = access->getAccessRegion();
		bool isDistributedRegion = VirtualMemoryManagement::isDistributedRegion(region);

		//! The source device is a host MemoryPlace of the current
		//! ClusterNode. We do not really need to perform a
		//! DataTransfer
		//! || The source and the destination is the same
		//! || I already have the data.
		if (sourceType == nanos6_host_device
			|| source == target
			|| WriteIDManager::checkWriteIDLocal(access->getWriteID(), region)) {

			// NULL copy (do nothing, just release succesor and delete itself.)
			return new Step();
		}


		bool needsTransfer =
			(
			 	//! We need a DataTransfer for a taskwait access
				//! in the following cases:
				//! 1) the access is not a NO_ACCESS_TYPE, so it
				//!    is part of the calling task's dependencies,
				//!    which means that the latest version of
				//!    the region needs to be present in the
				//!    context of the task at all times.
				//! 2) the access is a NO_ACCESS_TYPE access, so
				//!    it represents a region allocated within
				//!    the context of the Task but it is local
				//!    memory, so it needs to be present in the
				//!    context of the Task after the taskwait.
				//!    Distributed memory regions, do not need
				//!    to trigger a DataCopy, since anyway can
				//!    only be accessed from within subtasks.
				//!
				//! In both cases, we can avoid the copy if the
				//! access is a read-only access.
			 	(objectType == taskwait_type)
				&& (type != READ_ACCESS_TYPE)
				&& ((type != NO_ACCESS_TYPE) || !isDistributedRegion)
			) ||
			(
				//! We need a DataTransfer for an access_type
				//! access, if the access is not read-only
			 	(objectType == access_type)
				&& (type != WRITE_ACCESS_TYPE)
			);

		if (needsTransfer) {
			/* If the memory address belongs to ArgoDSM memory space,
			 * perform an Argo step instead of a Nanos6 step */
			ConfigVariable<std::string> commType("cluster.communication");
			if(commType.getValue() == "argo"){
				if (static_cast<char*>(region.getStartAddress()) >=
						static_cast<char*>(argo::virtual_memory::start_address()) &&
						static_cast<char*>(region.getStartAddress()) <
						static_cast<char*>(argo::virtual_memory::start_address()) +
						argo::virtual_memory::size()
				   ){
					return new ArgoAcquireStep(source, target, region);
				}
			}
		}

		return new ClusterDataCopyStep(
			source,
			target,
			inregion,
			access->getOriginator(),
			access->getWriteID(),
			(objectType == taskwait_type),
			access->isWeak(),
			needsTransfer
		);

	}

	inline Step *clusterCopy(
		MemoryPlace const *source,
		MemoryPlace const *target,
		DataAccessRegion const &region,
		DataAccess *access
	) {
		assert(target != nullptr);
		assert(access != nullptr);

		ClusterMemoryNode *current = ClusterManager::getCurrentMemoryNode();

		if (source != nullptr
			&& source->getType() != nanos6_cluster_device) {

			assert(source->getType() == nanos6_host_device);
			if (!Directory::isDirectoryMemoryPlace(source)) {
				source = current;
			}
		}

		if (target->getType() != nanos6_cluster_device) {
			//! At the moment cluster copies take into account only
			//! Cluster and host devices
			assert(target->getType() == nanos6_host_device);
			assert(!Directory::isDirectoryMemoryPlace(target));
			target = current;
		}

		if (target == current) {
			return clusterFetchData(source, target, region, access);
		}

		assert(access->getObjectType() == access_type);
		/* If the memory address belongs to ArgoDSM memory space,
		 * perform an Argo step instead of a Nanos6 step */
		ConfigVariable<std::string> commType("cluster.communication");
		if(commType.getValue() == "argo"){
			if (static_cast<char*>(access->getAccessRegion().getStartAddress()) >=
					static_cast<char*>(argo::virtual_memory::start_address()) &&
					static_cast<char*>(access->getAccessRegion().getStartAddress()) <
					static_cast<char*>(argo::virtual_memory::start_address()) +
					argo::virtual_memory::size()
			   ){
			return new ArgoDataLinkStep(source, target, access);
			}
		}
		return new ClusterDataLinkStep(source, target, access);
	}
}


#endif // EXECUTION_WORKFLOW_CLUSTER_HPP
