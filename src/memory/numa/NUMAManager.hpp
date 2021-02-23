/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef MANAGER_NUMA_HPP
#define MANAGER_NUMA_HPP

#include <cstring>
#include <map>
#include <numa.h>
#include <numaif.h>
#include <sys/mman.h>

#include <nanos6.h>

#include "executors/threads/CPUManager.hpp"
#include "hardware/HardwareInfo.hpp"
#include "hardware/places/NUMAPlace.hpp"
#include "lowlevel/FatalErrorHandler.hpp"
#include "lowlevel/RWSpinLock.hpp"
#include "support/BitManipulation.hpp"
#include "support/Containers.hpp"
#include "support/MathSupport.hpp"
#include "support/config/ConfigVariable.hpp"

#include <MemoryAllocator.hpp>

struct DirectoryInfo {
	size_t _size;
	uint8_t _homeNode;

	DirectoryInfo(size_t size, uint8_t homeNode) :
		_size(size),
		_homeNode(homeNode)
	{
	}
};

class NUMAManager {
private:
	typedef nanos6_bitmask_t bitmask_t;
	typedef Container::map<void *, DirectoryInfo> directory_t;
	typedef Container::map<void *, uint64_t> alloc_info_t;

	//! Directory to store the homeNode of each memory region
	static directory_t _directory;

	//! RWlock to access the directory
	static RWSpinLock _lock;

	//! Map to store the size of each allocation, to be able to free memory
	static alloc_info_t _allocations;

	//! Lock to access the allocations map
	static SpinLock _allocationsLock;

	//! The bitmask for wildcard NUMA_ALL
	static bitmask_t _bitmaskNumaAll;

	//! The bitmask for wildcard NUMA_ALL_ACTIVE
	static bitmask_t _bitmaskNumaAllActive;

	//! The bitmask for wildcard NUMA_ANY_ACTIVE
	static bitmask_t _bitmaskNumaAnyActive;

	//! Whether the tracking is enabled
	static std::atomic<bool> _trackingEnabled;

	//! The tracking mode "on", "off" or "auto"
	static ConfigVariable<std::string> _trackingMode;

	//! Whether should report NUMA information
	static ConfigVariable<bool> _reportEnabled;

public:
	static void initialize()
	{
		_trackingEnabled = false;
		std::string trackingMode = _trackingMode.getValue();
		if (trackingMode == "on") {
			// Mark tracking as enabled
			_trackingEnabled = true;
		} else if (trackingMode != "auto" && trackingMode != "off") {
			FatalErrorHandler::fail("Invalid data tracking mode: ", trackingMode);
		}

		// We always initialize everything, even in the "off" case. If "auto" we
		// enable the tracking in the first alloc/allocSentinels call

		// Initialize bitmasks to zero
		clearAll(&_bitmaskNumaAll);
		clearAll(&_bitmaskNumaAllActive);
		clearAll(&_bitmaskNumaAnyActive);

		size_t numNumaAll = 0;
		size_t numNumaAllActive = 0;
		size_t numNumaAnyActive = 0;

		numNumaAll = HardwareInfo::getMemoryPlaceCount(nanos6_host_device);

		// Currently, we are using uint64_t as type for the bitmasks. In case we have
		// more than 64 nodes, the bitmask cannot represent all the NUMA nodes
		FatalErrorHandler::failIf((numNumaAll > 64), "We cannot support such a high number of NUMA nodes.");
		FatalErrorHandler::failIf((numNumaAll <= 0), "There must be at least one NUMA node.");

		// The number of CPUs assigned to this process that each NUMA node contains
		std::vector<size_t> cpusPerNumaNode(numNumaAll, 0);

		// Get CPU list to check which CPUs we have in the process mask
		const std::vector<CPU *> &cpus = CPUManager::getCPUListReference();
		// Iterate over the CPU list to annotate CPUs per NUMA node
		for (CPU *cpu : cpus) {
			// In case DLB is enabled, we only want the CPUs we own
			if (cpu->isOwned()) {
				cpusPerNumaNode[cpu->getNumaNodeId()]++;
			}
		}

		// Enable corresponding bits in the bitmasks
		for (size_t numaNode = 0; numaNode < cpusPerNumaNode.size(); numaNode++) {
			// NUMA_ALL enables a bit per NUMA node available in the system
			BitManipulation::enableBit(&_bitmaskNumaAll, numaNode);

			// NUMA_ANY_ACTIVE enables a bit per NUMA node containing at least one CPU assigned to this process
			if (cpusPerNumaNode[numaNode] > 0) {
				BitManipulation::enableBit(&_bitmaskNumaAnyActive, numaNode);
				numNumaAnyActive++;

				// NUMA_ALL_ACTIVE enables a bit per NUMA node containing all the CPUs assigned to this process
				NUMAPlace *numaPlace = (NUMAPlace *) HardwareInfo::getMemoryPlace(nanos6_host_device, numaNode);
				assert(numaPlace != nullptr);
				if (cpusPerNumaNode[numaNode] == numaPlace->getNumLocalCores()) {
					BitManipulation::enableBit(&_bitmaskNumaAllActive, numaNode);
					numNumaAllActive++;
				}
			}
		}

		if (_reportEnabled) {
			FatalErrorHandler::print("---------- MANAGER NUMA REPORT ----------");
			FatalErrorHandler::print("NUMA_ALL:");
			FatalErrorHandler::print("  Number of NUMA nodes: ", numNumaAll);
			FatalErrorHandler::print("  bitmask: ", _bitmaskNumaAll);
			FatalErrorHandler::print("NUMA_ALL_ACTIVE:");
			FatalErrorHandler::print("  Number of NUMA nodes: ", numNumaAllActive);
			FatalErrorHandler::print("  bitmask: ", _bitmaskNumaAllActive);
			FatalErrorHandler::print("NUMA_ANY_ACTIVE:");
			FatalErrorHandler::print("  Number of NUMA nodes: ", numNumaAnyActive);
			FatalErrorHandler::print("  bitmask: ", _bitmaskNumaAnyActive);
		}
	}

	static void shutdown()
	{
		assert(_directory.empty());
		assert(_allocations.empty());
	}

	static void *alloc(size_t size, const bitmask_t *bitmask, size_t blockSize)
	{
		size_t pageSize = HardwareInfo::getPageSize();
		if (size < pageSize) {
			FatalErrorHandler::fail("Allocation size cannot be smaller than pagesize ", pageSize);
		}

		assert(bitmask != nullptr);
		assert(*bitmask != 0);
		assert(blockSize > 0);

		if (!enableTrackingIfAuto()) {
			void *res = malloc(size);
			FatalErrorHandler::failIf(res == nullptr, "Couldn't allocate memory.");
			return res;
		}

		bitmask_t bitmaskCopy = *bitmask;

		if (blockSize % pageSize != 0) {
			blockSize = MathSupport::closestMultiple(blockSize, pageSize);
		}

		void *res = nullptr;
		{
			// Allocate space using mmap
			int prot = PROT_READ | PROT_WRITE;
			int flags = MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE | MAP_NONBLOCK;
			int fd = -1;
			int offset = 0;
			void *addr = nullptr;
			res = mmap(addr, size, prot, flags, fd, offset);
			FatalErrorHandler::failIf(res == MAP_FAILED, "Couldn't allocate memory.");
		}

		_allocationsLock.lock();
		_allocations.emplace(res, size);
		_allocationsLock.unlock();

		struct bitmask *tmpBitmask = numa_bitmask_alloc(HardwareInfo::getMemoryPlaceCount(nanos6_host_device));

		for (size_t i = 0; i < size; i += blockSize) {
			uint8_t currentNodeIndex = BitManipulation::indexFirstEnabledBit(bitmaskCopy);
			BitManipulation::disableBit(&bitmaskCopy, currentNodeIndex);
			if (bitmaskCopy == 0) {
				bitmaskCopy = *bitmask;
			}

			// Place pages where they must be
			void *tmp = (void *) ((uintptr_t) res + i);
			numa_bitmask_clearall(tmpBitmask);
			numa_bitmask_setbit(tmpBitmask, currentNodeIndex);
			assert(numa_bitmask_isbitset(tmpBitmask, currentNodeIndex));
			size_t tmpSize = std::min(blockSize, size-i);
			numa_interleave_memory(tmp, tmpSize, tmpBitmask);

			// Insert into directory
			DirectoryInfo info(tmpSize, currentNodeIndex);
			_lock.writeLock();
			_directory.emplace(tmp, info);
			_lock.writeUnlock();
		}

		numa_bitmask_free(tmpBitmask);

#ifndef NDEBUG
		checkAllocationCorrectness(res, size, bitmask, blockSize);
#endif

		return res;
	}

	static void *allocSentinels(size_t size, const bitmask_t *bitmask, size_t blockSize)
	{
		assert(size > 0);

		if (!enableTrackingIfAuto()) {
			void *res = malloc(size);
			FatalErrorHandler::failIf(res == nullptr, "Couldn't allocate memory.");
			return res;
		}

		assert(*bitmask != 0);
		assert(blockSize > 0);

		bitmask_t bitmaskCopy = *bitmask;

		void *res = nullptr;
		size_t pageSize = HardwareInfo::getPageSize();

		if (size < pageSize) {
			// Use malloc for small allocations
			res = malloc(size);
			FatalErrorHandler::failIf(res == nullptr, "Couldn't allocate memory.");
		} else {
			// Allocate space using mmap
			int prot = PROT_READ | PROT_WRITE;
			int flags = MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE | MAP_NONBLOCK;
			int fd = -1;
			int offset = 0;
			void *addr = nullptr;
			res = mmap(addr, size, prot, flags, fd, offset);
			FatalErrorHandler::failIf(res == MAP_FAILED, "Couldn't allocate memory.");
		}

		_allocationsLock.lock();
		_allocations.emplace(res, size);
		_allocationsLock.unlock();

		// In this case, the whole allocation is inside the same page. However, it
		// is important for scheduling purposes to annotate in the directory as if
		// we could really split the allocation as requested
		for (size_t i = 0; i < size; i += blockSize) {
			uint8_t currentNodeIndex = BitManipulation::indexFirstEnabledBit(bitmaskCopy);
			BitManipulation::disableBit(&bitmaskCopy, currentNodeIndex);
			if (bitmaskCopy == 0) {
				bitmaskCopy = *bitmask;
			}

			// Insert into directory
			void *tmp = (void *) ((uintptr_t) res + i);
			size_t tmpSize = std::min(blockSize, size - i);
			DirectoryInfo info(tmpSize, currentNodeIndex);
			_lock.writeLock();
			_directory.emplace(tmp, info);
			_lock.writeUnlock();
		}


		return res;
	}

	static void free(void *ptr)
	{
		if (!isTrackingEnabled()) {
			std::free(ptr);
			return;
		}

		size_t pageSize = HardwareInfo::getPageSize();

		_allocationsLock.lock();
		// Find the allocation size and remove (one single map search)
		auto allocIt = _allocations.find(ptr);
		assert(allocIt != _allocations.end());
		size_t size = allocIt->second;
		_allocations.erase(allocIt);
		_allocationsLock.unlock();

		_lock.writeLock();
		// Find the initial element in the directory
		auto begin = _directory.find(ptr);
		assert(begin != _directory.end());

		// Find the next element after the allocation
		auto end = _directory.lower_bound((void *) ((uintptr_t) ptr + size));

		// Remove all elements in the range [begin, end)
		_directory.erase(begin, end);
		_lock.writeUnlock();

		// Release memory
		if (size < pageSize) {
			std::free(ptr);
		} else {
			__attribute__((unused)) int res = munmap(ptr, size);
			assert(res == 0);
		}
	}

	static uint8_t getHomeNode(void *ptr, size_t size)
	{
		if (!isTrackingEnabled()) {
			return (uint8_t) -1;
		}

		// Search in the directory
		_lock.readLock();
		auto it = _directory.lower_bound(ptr);

		// lower_bound returns the first element not considered to go before ptr
		// Thus, if ptr is exactly the start of the region, lower_bound will return
		// the desired region. Otherwise, if ptr belongs to the region but its start
		// address is greater than the region start, lower_bound returns the next
		// region. In consequence, we should apply a decrement to the iterator
		if (it == _directory.end() || ptr < it->first) {
			if (it == _directory.begin()) {
				_lock.readUnlock();
				return (uint8_t) -1;
			}
			it--;
		}

		// Not present
		if (it == _directory.end() || getContainedBytes(ptr, size, it->first, it->second._size) == 0) {
			_lock.readUnlock();
			return (uint8_t) -1;
		}

		// If the target region resides in several directory regions, we return as the
		// homeNode the one containing more bytes

		size_t numNumaAll = HardwareInfo::getMemoryPlaceCount(nanos6_host_device);
		assert(numNumaAll > 0);

		size_t *bytesInNUMA = (size_t *) alloca(numNumaAll * sizeof(size_t));
		std::memset(bytesInNUMA, 0, numNumaAll * sizeof(size_t));

		int idMax = 0;
		size_t foundBytes = 0;
		do {
			size_t containedBytes = getContainedBytes(it->first, it->second._size, ptr, size);

			// Break after we are out of the range [ptr, end)
			if (containedBytes == 0)
				break;

			uint8_t homeNode = it->second._homeNode;
			assert(homeNode != (uint8_t) -1);
			bytesInNUMA[homeNode] += containedBytes;

			if (bytesInNUMA[homeNode] > bytesInNUMA[idMax]) {
				idMax = homeNode;
			}

			// Cutoff: no other NUMA node can score better than this
			if (bytesInNUMA[homeNode] >= (size / 2)) {
				_lock.readUnlock();
				return homeNode;
			}

			foundBytes += containedBytes;
			it++;
		} while (foundBytes != size && it != _directory.end());
		_lock.readUnlock();

		assert(bytesInNUMA[idMax] > 0);

		return idMax;
	}

	static inline void clearAll(bitmask_t *bitmask)
	{
		*bitmask = 0;
	}

	static inline void clearBit(bitmask_t *bitmask, uint64_t bitIndex)
	{
		BitManipulation::disableBit(bitmask, bitIndex);
	}

	static inline void setAll(bitmask_t *bitmask)
	{
		*bitmask = _bitmaskNumaAll;
	}

	static inline void setAllActive(bitmask_t *bitmask)
	{
		*bitmask = _bitmaskNumaAllActive;
	}

	static inline void setAnyActive(bitmask_t *bitmask)
	{
		*bitmask = _bitmaskNumaAnyActive;
	}

	static inline void setWildcard(bitmask_t *bitmask, nanos6_bitmask_wildcard_t wildcard)
	{
		if (wildcard == NUMA_ALL) {
			setAll(bitmask);
		} else if (wildcard == NUMA_ALL_ACTIVE) {
			setAllActive(bitmask);
		} else if (wildcard == NUMA_ANY_ACTIVE) {
			setAnyActive(bitmask);
		} else {
			FatalErrorHandler::warnIf(true, "No valid wildcard provided. Bitmask is left unchangend.");
		}
	}

	static inline void setBit(bitmask_t *bitmask, uint64_t bitIndex)
	{
		BitManipulation::enableBit(bitmask, bitIndex);
	}

	static inline uint64_t isBitSet(const bitmask_t *bitmask, uint64_t bitIndex)
	{
		return (uint64_t) BitManipulation::checkBit(bitmask, bitIndex);
	}

	static inline uint64_t countEnabledBits(const bitmask_t *bitmask)
	{
		return BitManipulation::countEnabledBits(bitmask);
	}

	static bool isTrackingEnabled();

	static inline bool isValidNUMA(uint64_t bitIndex)
	{
		return BitManipulation::checkBit(&_bitmaskNumaAnyActive, bitIndex);
	}

	static uint64_t getTrackingNodes();

private:
	static inline size_t getContainedBytes(void *ptr1, size_t size1, void *ptr2, size_t size2)
	{
		uintptr_t start1 = (uintptr_t) ptr1;
		uintptr_t start2 = (uintptr_t) ptr2;
		uintptr_t end1 = start1 + size1;
		uintptr_t end2 = start2 + size2;
		uintptr_t start = std::max(start1, start2);
		uintptr_t end = std::min(end1, end2);

		if (start < end)
			return end - start;

		return 0;
	}

	static bool enableTrackingIfAuto()
	{
		if (isTrackingEnabled()) {
			return true;
		}

		std::string trackingMode = _trackingMode.getValue();
		if (trackingMode == "auto" && getValidTrackingNodes() > 1) {
			_trackingEnabled.store(true, std::memory_order_release);
			return true;
		}

		return false;
	}

	static inline uint64_t getValidTrackingNodes()
	{
		std::string trackingMode = _trackingMode.getValue();
		if (trackingMode == "off") {
			return 1;
		} else {
			return BitManipulation::countEnabledBits(&_bitmaskNumaAnyActive);
		}
	}

#ifndef NDEBUG
	static void checkAllocationCorrectness(
		void *res, size_t size,
		const bitmask_t *bitmask,
		size_t blockSize
	);
#endif
};

#endif //MANAGER_NUMA_HPP
