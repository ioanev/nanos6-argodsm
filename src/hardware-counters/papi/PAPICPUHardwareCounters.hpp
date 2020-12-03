/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef PAPI_CPU_HARDWARE_COUNTERS_HPP
#define PAPI_CPU_HARDWARE_COUNTERS_HPP

#include <cstring>
#include <papi.h>

#include "PAPIHardwareCounters.hpp"
#include "hardware-counters/CPUHardwareCountersInterface.hpp"
#include "hardware-counters/SupportedHardwareCounters.hpp"
#include "lowlevel/FatalErrorHandler.hpp"


class PAPICPUHardwareCounters : public CPUHardwareCountersInterface {

private:

	//! Arrays of regular HW counter deltas
	long long _counters[HWCounters::HWC_PAPI_NUM_EVENTS];

public:

	inline PAPICPUHardwareCounters()
	{
		memset(_counters, 0, sizeof(_counters));
	}

	//! \brief Read counters from an event set
	//!
	//! \param[in] eventSet The event set specified
	inline void readCounters(int eventSet)
	{
		assert(eventSet != PAPI_NULL);

		int ret = PAPI_read(eventSet, _counters);
		if (ret != PAPI_OK) {
			FatalErrorHandler::fail(ret, " when reading a PAPI event set - ", PAPI_strerror(ret));
		}

		ret = PAPI_reset(eventSet);
		if (ret != PAPI_OK) {
			FatalErrorHandler::fail(ret, " when resetting a PAPI event set - ", PAPI_strerror(ret));
		}
	}

	//! \brief Get the delta value of a HW counter
	//!
	//! \param[in] counterType The type of counter to get the delta from
	inline uint64_t getDelta(HWCounters::counters_t counterType) const override
	{
		assert(PAPIHardwareCounters::isCounterEnabled(counterType));

		int innerId = PAPIHardwareCounters::getInnerIdentifier(counterType);
		assert(innerId >= 0 && (size_t) innerId < PAPIHardwareCounters::getNumEnabledCounters());

		return (uint64_t) _counters[innerId];
	}

};

#endif // PAPI_CPU_HARDWARE_COUNTERS_HPP
