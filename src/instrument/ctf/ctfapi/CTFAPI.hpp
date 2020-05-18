/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef CTFAPI_HPP
#define CTFAPI_HPP

#include <cstdint>
#include <inttypes.h>
#include <time.h>
#include <string.h>

#include "InstrumentCPULocalData.hpp"
#include "instrument/support/InstrumentCPULocalDataSupport.hpp"
#include "lowlevel/FatalErrorHandler.hpp"

#include "CTFTypes.hpp"
#include "CTFEvent.hpp"
#include "stream/CTFStream.hpp"

namespace CTFAPI {

	struct __attribute__((__packed__)) event_header {
		uint8_t  id;
		uint64_t timestamp;
	};

	void greetings(void);
	void addStreamHeader(CTFStream *stream);
	int mk_event_header(char **buf, uint8_t id);


	template <typename T>
	static inline size_t sizeOfVariadic(T arg)
	{
		size_t value = sizeof(arg);
		return value;
	}

	template <>
	inline size_t sizeOfVariadic(const char *arg)
	{
		size_t i = 0;
		for (; arg[i]; ++i)
			;
		size_t value = i * sizeof(char);
		return value + 1; // adding 1 to count for the null character
	}

	template<typename First, typename... Rest>
	static inline size_t sizeOfVariadic(First arg, Rest... rest)
	{
		size_t total = sizeOfVariadic(arg) + sizeOfVariadic(rest...);
		return total;
	}

	template<typename... ARGS>
	inline void tp_write_args(void **buf, ARGS... args)
	{
	}

	template<typename T>
	static inline void tp_write_args(void **buf, T arg)
	{
		T **p = reinterpret_cast<T**>(buf);
		**p = arg;
		(*p)++;
	}

	template<>
	inline void tp_write_args(void **buf, const char *arg)
	{
		char **pbuf = reinterpret_cast<char**>(buf);
		char *parg = (char *) arg;

		while (*parg != '\0') {
			**pbuf = *parg;
			parg++;
			(*pbuf)++;
		}

		**pbuf = '\0';
		(*pbuf)++;
	}

	template<typename T, typename... ARGS>
	static inline void tp_write_args(void **buf, T arg, ARGS... args)
	{
		tp_write_args(buf, arg);
		tp_write_args(buf, args...);
	}

	// TODO update instructions
	//
	// To add a new user-space tracepoint into Nanos6:
	//   1) add a new TP_NANOS6_* macro with your tracepoint id.
	//   2) add the corresponding metadata entry under CTFAPI.cpp with the
	//      previous ID. Define arguments as needed.
	//   3) call this function with the tracepoint ID as first argument and
	//      the correspnding arguments as defined in the metadata file,
	//      in the same order.
	// When calling this function, allways cast each variadic argument to
	// the type specified in the metadata file. Otherwhise an incorrect
	// number of bytes might be written.

	template<typename... ARGS>
	static void tracepoint(CTFEvent *event, ARGS... args)
	{
		CTFStream *stream = Instrument::getCPULocalData()->userStream;
		const size_t size = sizeof(struct event_header) + sizeOfVariadic(args...) + event->getContextSize() + stream->getContextSize();
		const uint8_t tracepointId = event->getEventId();
		void *buf;

		stream->lock();

		// TODO checkFreeSpace should not perform flushing, move
		// it here.
		// TODO add flushing tracepoints if possible
		if (!stream->checkFreeSpace(size)) {
			stream->unlock();
			return;
		}

		buf = stream->buffer + (stream->head & stream->mask);

		mk_event_header((char **) &buf, tracepointId);
		stream->writeContext(&buf);
		event->writeContext(&buf);
		tp_write_args(&buf, args...);

		stream->head += size;

		stream->unlock();
	}
}

#endif // CTFAPI_HPP
