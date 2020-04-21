/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019-2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef CPU_USAGE_PREDICTOR_HPP
#define CPU_USAGE_PREDICTOR_HPP

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>

#include "nanos6/polling.h"

#include "CPUMonitor.hpp"
#include "support/config/ConfigVariable.hpp"


class CPUUsagePredictor {

private:

	typedef boost::accumulators::accumulator_set<double, boost::accumulators::stats<boost::accumulators::tag::sum, boost::accumulators::tag::mean> > accumulator_t;

	//! How often (microseconds) CPU utilization predictions are made
	static ConfigVariable<size_t> _predictionRate;

	//! The current prediction
	double _prediction;

	//! Tells whether the first prediciton has been done
	bool _predictionAvailable;

	//! Accumulator of prediction accuracies
	accumulator_t _accuracies;

public:

	inline CPUUsagePredictor() :
		_predictionRate("monitoring.cpuusage_prediction_rate", 100 /* µs */)
	{
	}

	inline ~CPUUsagePredictor()
	{
	}

	//! \brief Retreive CPU usage prediction statistics
	//!
	//! \param[in,out] stream The output stream
	inline void displayStatistics(std::stringstream &stream)
	{
		stream << std::left << std::fixed << std::setprecision(2) << "\n";
		stream << "+-----------------------------+\n";
		stream << "|    CPU Usage Predictions    |\n";
		stream << "+-----------------------------+\n";
		if (_predictionAvailable) {
			stream << "  MEAN ACCURACY: " << boost::accumulators::mean(_accuracies) << "%\n";
		} else {
			stream << "  MEAN ACCURACY: NA\n";
		}
		stream << "+-----------------------------+\n\n";
	}

	//! \brief Get a CPU Usage prediction over an amount of time
	//!
	//! \param[in] time The amount of time in microseconds to predict usage for
	//! (i.e. in the next 'time' microseconds, the amount of CPUs to be used)
	//!
	//! \return The expected CPU Usage for the next 'time' microseconds
	double getCPUUsagePrediction(size_t time);

};

#endif // CPU_USAGE_PREDICTOR_HPP

