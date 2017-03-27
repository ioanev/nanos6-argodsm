#include <atomic>
#include <cassert>

#include "InstrumentDependenciesByAccessLinks.hpp"
#include "InstrumentVerbose.hpp"


using namespace Instrument::Verbose;


namespace Instrument {
	static std::atomic<data_access_id_t::inner_type_t> _nextDataAccessId(1);
	
	data_access_id_t createdDataAccess(
		data_access_id_t superAccessId,
		DataAccessType accessType, bool weak, DataAccessRange range,
		bool readSatisfied, bool writeSatisfied, bool globallySatisfied,
		task_id_t originatorTaskId,
		InstrumentationContext const &context
	) {
		if (!_verboseDependenciesByAccessLinks) {
			return data_access_id_t();
		}
		
		LogEntry *logEntry = getLogEntry();
		assert(logEntry != nullptr);
		
		data_access_id_t id = _nextDataAccessId++;
		
		logEntry->appendLocation(context);
		logEntry->_contents << " <-> CreateDataAccess " << id << " superaccess:" << superAccessId << " ";
		
		if (weak) {
			logEntry->_contents << "weak";
		}
		switch (accessType) {
			case READ_ACCESS_TYPE:
				logEntry->_contents << " input";
				break;
			case READWRITE_ACCESS_TYPE:
				logEntry->_contents << " inout";
				break;
			case WRITE_ACCESS_TYPE:
				logEntry->_contents << " output";
				break;
			default:
				logEntry->_contents << " unknown_access_type";
				break;
		}
		
		logEntry->_contents << " " << range;
		
		if (readSatisfied) {
			logEntry->_contents << " read_safistied";
		}
		if (writeSatisfied) {
			logEntry->_contents << " write_safistied";
		}
		if (globallySatisfied) {
			logEntry->_contents << " safistied";
		}
		if (!readSatisfied && !writeSatisfied && !globallySatisfied) {
			logEntry->_contents << " unsatisfied";
		}
		
		logEntry->_contents << " originator:" << originatorTaskId;
		
		addLogEntry(logEntry);
		
		return id;
	}
	
	
	void upgradedDataAccess(
		data_access_id_t dataAccessId,
		DataAccessType previousAccessType,
		bool previousWeakness,
		DataAccessType newAccessType,
		bool newWeakness,
		bool becomesUnsatisfied,
		InstrumentationContext const &context
	) {
		if (!_verboseDependenciesByAccessLinks) {
			return;
		}
		
		LogEntry *logEntry = getLogEntry();
		assert(logEntry != nullptr);
		
		logEntry->appendLocation(context);
		logEntry->_contents << " <-> UpgradeDataAccess " << dataAccessId;
		
		logEntry->_contents << " ";
		if (previousWeakness) {
			logEntry->_contents << "weak";
		} else {
			logEntry->_contents << "noweak";
		}
		logEntry->_contents << "->";
		if (newWeakness) {
			logEntry->_contents << "weak";
		} else {
			logEntry->_contents << "noweak";
		}
		
		logEntry->_contents << " ";
		switch (previousAccessType) {
			case READ_ACCESS_TYPE:
				logEntry->_contents << "input";
				break;
			case READWRITE_ACCESS_TYPE:
				logEntry->_contents << "inout";
				break;
			case WRITE_ACCESS_TYPE:
				logEntry->_contents << "output";
				break;
			default:
				logEntry->_contents << "unknown_access_type";
				break;
		}
		logEntry->_contents << "->";
		switch (newAccessType) {
			case READ_ACCESS_TYPE:
				logEntry->_contents << "input";
				break;
			case READWRITE_ACCESS_TYPE:
				logEntry->_contents << "inout";
				break;
			case WRITE_ACCESS_TYPE:
				logEntry->_contents << "output";
				break;
			default:
				logEntry->_contents << "unknown_access_type";
				break;
		}
		
		if (becomesUnsatisfied) {
			logEntry->_contents << " satisfied->unsatisfied";
		}
		
		logEntry->_contents << " triggererTask:" << context._taskId;
		
		addLogEntry(logEntry);
	}
	
	
	void dataAccessBecomesSatisfied(
		data_access_id_t dataAccessId,
		bool readSatisfied, bool writeSatisfied, bool globallySatisfied,
		task_id_t targetTaskId,
		InstrumentationContext const &context
	) {
		if (!_verboseDependenciesByAccessLinks) {
			return;
		}
		
		LogEntry *logEntry = getLogEntry();
		assert(logEntry != nullptr);
		
		logEntry->appendLocation(context);
		logEntry->_contents << " <-> DataAccessBecomesSatisfied " << dataAccessId << " triggererTask:" << context._taskId << " targetTask:" << targetTaskId;
		
		if (readSatisfied) {
			logEntry->_contents << " +read_safistied";
		}
		if (writeSatisfied) {
			logEntry->_contents << " +write_safistied";
		}
		if (globallySatisfied) {
			logEntry->_contents << " +safistied";
		}
		if (!readSatisfied && !writeSatisfied && !globallySatisfied) {
			logEntry->_contents << " remains_unsatisfied";
		}
		
		addLogEntry(logEntry);
	}
	
	
	void modifiedDataAccessRange(
		data_access_id_t dataAccessId,
		DataAccessRange newRange,
		InstrumentationContext const &context
	) {
		if (!_verboseDependenciesByAccessLinks) {
			return;
		}
		
		LogEntry *logEntry = getLogEntry();
		assert(logEntry != nullptr);
		
		logEntry->appendLocation(context);
		logEntry->_contents << " <-> ModifiedDataAccessRange " << dataAccessId << " newRange:" << newRange << " triggererTask:" << context._taskId;
		
		addLogEntry(logEntry);
	}
	
	
	data_access_id_t fragmentedDataAccess(
		data_access_id_t dataAccessId,
		DataAccessRange newRange,
		InstrumentationContext const &context
	) {
		if (!_verboseDependenciesByAccessLinks) {
			return data_access_id_t();
		}
		
		LogEntry *logEntry = getLogEntry();
		assert(logEntry != nullptr);
		
		data_access_id_t id = _nextDataAccessId++;
		
		logEntry->appendLocation(context);
		logEntry->_contents << " <-> FragmentedDataAccess " << dataAccessId << " newFragment:" << id << " newRange:" << newRange << " triggererTask:" << context._taskId;
		
		addLogEntry(logEntry);
		
		return id;
	}
	
	
	data_access_id_t createdDataSubaccessFragment(
		data_access_id_t dataAccessId,
		InstrumentationContext const &context
	) {
		if (!_verboseDependenciesByAccessLinks) {
			return data_access_id_t();
		}
		
		LogEntry *logEntry = getLogEntry();
		assert(logEntry != nullptr);
		
		data_access_id_t id = _nextDataAccessId++;
		
		logEntry->appendLocation(context);
		logEntry->_contents << " <-> CreatedDataSubaccessFragment " << dataAccessId << " newSubaccessFragment:" << id << " triggererTask:" << context._taskId;
		
		addLogEntry(logEntry);
		
		return id;
	}
	
	
	void completedDataAccess(
		data_access_id_t dataAccessId,
		InstrumentationContext const &context
	) {
		if (!_verboseDependenciesByAccessLinks) {
			return;
		}
		
		LogEntry *logEntry = getLogEntry();
		assert(logEntry != nullptr);
		
		logEntry->appendLocation(context);
		logEntry->_contents << " <-> CompletedDataAccess " << dataAccessId << " triggererTask:" << context._taskId;
		
		addLogEntry(logEntry);
	}
	
	
	void dataAccessBecomesRemovable(
		data_access_id_t dataAccessId,
		InstrumentationContext const &context
	) {
		if (!_verboseDependenciesByAccessLinks) {
			return;
		}
		
		LogEntry *logEntry = getLogEntry();
		assert(logEntry != nullptr);
		
		logEntry->appendLocation(context);
		logEntry->_contents << " <-> DataAccessBecomesRemovable " << dataAccessId << " triggererTask:" << context._taskId;
		
		addLogEntry(logEntry);
	}
	
	
	void removedDataAccess(
		data_access_id_t dataAccessId,
		InstrumentationContext const &context
	) {
		if (!_verboseDependenciesByAccessLinks) {
			return;
		}
		
		LogEntry *logEntry = getLogEntry();
		assert(logEntry != nullptr);
		
		logEntry->appendLocation(context);
		logEntry->_contents << " <-> RemoveDataAccess " << dataAccessId << " triggererTask:" << context._taskId;
		
		addLogEntry(logEntry);
	}
	
	
	void linkedDataAccesses(
		data_access_id_t sourceAccessId,
		task_id_t sinkTaskId,
		DataAccessRange range,
		bool direct,
		__attribute__((unused)) bool bidirectional,
		InstrumentationContext const &context
	) {
		if (!_verboseDependenciesByAccessLinks) {
			return;
		}
		
		LogEntry *logEntry = getLogEntry();
		assert(logEntry != nullptr);
		
		logEntry->appendLocation(context);
		logEntry->_contents << " <-> LinkDataAccesses " << sourceAccessId << " -> Task:" << sinkTaskId << " [" << range << "]" << (direct ? " direct" : "indirect") << " triggererTask:" << context._taskId;
		
		addLogEntry(logEntry);
	}
	
	
	void unlinkedDataAccesses(
		data_access_id_t sourceAccessId,
		task_id_t sinkTaskId,
		bool direct,
		InstrumentationContext const &context
	) {
		if (!_verboseDependenciesByAccessLinks) {
			return;
		}
		
		LogEntry *logEntry = getLogEntry();
		assert(logEntry != nullptr);
		
		logEntry->appendLocation(context);
		logEntry->_contents << " <-> UnlinkDataAccesses " << sourceAccessId << " -> Task:" << sinkTaskId << (direct ? " direct" : "indirect") << " triggererTask:" << context._taskId;
		
		addLogEntry(logEntry);
	}
	
	
	void reparentedDataAccess(
		data_access_id_t oldSuperAccessId,
		data_access_id_t newSuperAccessId,
		data_access_id_t dataAccessId,
		InstrumentationContext const &context
	) {
		if (!_verboseDependenciesByAccessLinks) {
			return;
		}
		
		LogEntry *logEntry = getLogEntry();
		assert(logEntry != nullptr);
		
		logEntry->appendLocation(context);
		logEntry->_contents << " <-> ReplaceSuperAccess " << dataAccessId << " " << oldSuperAccessId << "->" << newSuperAccessId << " triggererTask:" << context._taskId;
		
		addLogEntry(logEntry);
	}
	
	
	void newDataAccessProperty(
		data_access_id_t dataAccessId,
		char const *shortPropertyName,
		char const *longPropertyName,
		InstrumentationContext const &context
	) {
		if (!_verboseDependenciesByAccessLinks) {
			return;
		}
		
		LogEntry *logEntry = getLogEntry();
		assert(logEntry != nullptr);
		
		logEntry->appendLocation(context);
		logEntry->_contents << " <-> DataAccessNewProperty " << dataAccessId << " " << longPropertyName << " (" << shortPropertyName << ") triggererTask:" << context._taskId;
		
		addLogEntry(logEntry);
	}
	
}
