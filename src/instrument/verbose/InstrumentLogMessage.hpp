#ifndef INSTRUMENT_VERBOSE_LOG_MESSAGE_HPP
#define INSTRUMENT_VERBOSE_LOG_MESSAGE_HPP


#include <cassert>

#include "../api/InstrumentLogMessage.hpp"

#include "InstrumentVerbose.hpp"


using namespace Instrument::Verbose;


namespace Instrument {
	namespace Verbose {
		template<typename T>
		inline void fillLogEntry(LogEntry *logEntry, T contents)
		{
			logEntry->_contents << contents;
		}
		
		template<typename T, typename... TS>
		inline void fillLogEntry(LogEntry *logEntry, T content1, TS... contents)
		{
			logEntry->_contents << content1;
			fillLogEntry(logEntry, contents...);
		}
	}
	
	
	template<typename... TS>
	void logMessage(InstrumentationContext const &context, TS... contents)
	{
		LogEntry *logEntry = getLogEntry();
		assert(logEntry != nullptr);
		
		logEntry->appendLocation(context);
		
		if (context._taskId != task_id_t()) {
			logEntry->_contents << " Task:" << context._taskId;
		}
		
		logEntry->_contents << " ";
		
		fillLogEntry(logEntry, contents...);
		
		addLogEntry(logEntry);
	}
}


#endif // INSTRUMENT_VERBOSE_LOG_MESSAGE_HPP
