/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef INSTRUMENT_STATS_HPP
#define INSTRUMENT_STATS_HPP

#include <list>
#include <map>
#include <vector>

#include <nanos6.h>

#include "Timer.hpp"
#include "lowlevel/RWTicketSpinLock.hpp"
#include "lowlevel/SpinLock.hpp"

#define NANOS6_DEPENDENCY_STATE_MACRO					\
	HELPER(NANOS_OUTSIDE_DEPENDENCYSUBSYSTEM)			\
	HELPER(NANOS_REGISTERTASKDATAACCESSES)				\
	HELPER(NANOS_UNREGISTERTASKDATAACCESSES)			\
	HELPER(NANOS_PROPAGATESATISFIABILITY)				\
	HELPER(NANOS_RELEASEACCESSREGION)					\
	HELPER(NANOS_HANDLEENTERTASKWAIT)					\
	HELPER(NANOS_HANDLEEXITTASKWAIT)					\
	HELPER(NANOS_UNREGISTERTASKDATAACCESSESCALLBACK)	\
	HELPER(NANOS_UNREGISTERTASKDATAACCESSES2)			\
	HELPER(NANOS_HANDLECOMPLETEDTASKWAITS)				\
	HELPER(NANOS_SETUPTASKWAITWORKFLOW)					\
	HELPER(NANOS_RELEASETASKWAITFRAGMENT)				\
	HELPER(NANOS_CREATEDATACOPYSTEP_TASK)				\
	HELPER(NANOS_CREATEDATACOPYSTEP_TASKWAIT)


namespace Instrument {
	namespace Stats {
		extern RWTicketSpinLock _phasesSpinLock;
		extern int _currentPhase;
		extern std::vector<Timer> _phaseTimes;

		typedef enum {
#define HELPER(arg) arg,
			NANOS6_DEPENDENCY_STATE_MACRO
			NANOS_DEPENDENCY_STATE_TYPES
#undef HELPER
		} nanos6_dependency_state_t;

		extern std::array<std::atomic<size_t>, NANOS_DEPENDENCY_STATE_TYPES> nanos6_dependency_state_stats;

		void show_dependency_state_stats(std::ostream &output);

		struct TaskTimes {
			Timer _instantiationTime;
			Timer _pendingTime;
			Timer _readyTime;
			Timer _executionTime;
			Timer _blockedTime;
			Timer _zombieTime;

			TaskTimes(bool summary)
				: _instantiationTime(!summary),
				_pendingTime(false),
				_readyTime(false),
				_executionTime(false),
				_blockedTime(false),
				_zombieTime(false)
			{
			}

			TaskTimes &operator+=(TaskTimes const &instanceTimes)
			{
				_instantiationTime += instanceTimes._instantiationTime;
				_pendingTime += instanceTimes._pendingTime;
				_readyTime += instanceTimes._readyTime;
				_executionTime += instanceTimes._executionTime;
				_blockedTime += instanceTimes._blockedTime;
				_zombieTime += instanceTimes._zombieTime;

				return *this;
			}

			template <typename T>
			TaskTimes operator/(T divisor) const
			{
				TaskTimes result(*this);

				result._instantiationTime /= divisor;
				result._pendingTime /= divisor;
				result._readyTime /= divisor;
				result._executionTime /= divisor;
				result._blockedTime /= divisor;
				result._zombieTime /= divisor;

				return result;
			}

			Timer getTotal() const
			{
				Timer result;

				result += _instantiationTime;
				result += _pendingTime;
				result += _readyTime;
				result += _executionTime;
				result += _blockedTime;
				result += _zombieTime;

				return result;
			}
		};


		struct TaskInfo {
			long _numInstances;
			TaskTimes _times;

			TaskInfo()
				: _numInstances(0), _times(true)
			{
			}

			TaskInfo &operator+=(TaskTimes const &instanceTimes)
			{
				_numInstances++;
				_times += instanceTimes;

				return *this;
			}

			TaskInfo &operator+=(TaskInfo const &other)
			{
				_numInstances += other._numInstances;
				_times += other._times;

				return *this;
			}

			TaskTimes getMean() const
			{
				return _times / _numInstances;
			}
		};

		struct TaskTypeAndTimes {
			nanos6_task_info_t const *_type;
			SpinLock _lock;
			TaskTimes _times;
			bool _hasParent;
			Timer *_currentTimer;

			TaskTypeAndTimes(nanos6_task_info_t const *type, bool hasParent)
				: _type(type), _times(false), _hasParent(hasParent), _currentTimer(&_times._instantiationTime)
			{
			}
		};


		struct PhaseInfo {
			std::map<nanos6_task_info_t const *, TaskInfo> _perTask;
			Timer _runningTime;
			Timer _blockedTime;

			PhaseInfo(bool active=true)
				: _perTask(),
				_runningTime(active),
				_blockedTime(false)
			{
			}

			PhaseInfo &operator+=(PhaseInfo const &other)
			{
				for (auto &perTaskEntry : other._perTask) {
					_perTask[perTaskEntry.first] += perTaskEntry.second;
				}

				_runningTime += other._runningTime;
				_blockedTime += other._blockedTime;

				return *this;
			}

			void stopTimers()
			{
				if (_runningTime.isRunning()) {
					assert(!_blockedTime.isRunning());
					_runningTime.stop();
				} else {
					assert(_blockedTime.isRunning());
					_blockedTime.stop();
				}
			}

			void stoppedAt(Timer const &reference)
			{
				if (!_runningTime.empty()) {
					_runningTime.fixStopTimeFrom(reference);
				}

				if (!_blockedTime.empty()) {
					_blockedTime.fixStopTimeFrom(reference);
				}
			}

			bool isRunning() const
			{
				return _runningTime.isRunning();
			}
		};


		struct ThreadInfo {
			std::list<PhaseInfo> _phaseInfo;

			ThreadInfo(bool active=true)
				: _phaseInfo()
			{
				_phaseInfo.emplace_back(active);
			}

			ThreadInfo &operator+=(ThreadInfo const &other)
			{
				unsigned int phases = other._phaseInfo.size();

				while (_phaseInfo.size() < phases) {
					_phaseInfo.emplace_back(false);
				}

				auto it = _phaseInfo.begin();
				auto otherIt = other._phaseInfo.begin();
				while (otherIt != other._phaseInfo.end()) {
					(*it) += (*otherIt);

					it++;
					otherIt++;
				}

				return *this;
			}

			PhaseInfo &getCurrentPhaseRef()
			{
				Instrument::Stats::_phasesSpinLock.readLock();

				assert(_currentPhase == (int)(_phaseTimes.size() - 1));
				int lastStartedPhase = _phaseInfo.size() - 1;

				if (lastStartedPhase == -1) {
					// Add the previous phases as empty
					for (int phase = 0; phase < _currentPhase-1; phase++) {
						_phaseInfo.emplace_back(false);
					}

					// Start the new phase
					_phaseInfo.emplace_back(true);
				} else if (lastStartedPhase < _currentPhase) {
					// Fix the stopping time of the last phase
					bool isRunning = _phaseInfo.back().isRunning();
					_phaseInfo.back().stoppedAt(_phaseTimes[lastStartedPhase]);

					// Mark any already finished phase that is missing and the current phase as blocked
					for (int phase = lastStartedPhase+1; phase <= _currentPhase; phase++) {
						_phaseInfo.emplace_back(false);

						if (isRunning) {
							_phaseInfo.back()._runningTime = _phaseTimes[phase];
						} else {
							_phaseInfo.back()._blockedTime = _phaseTimes[phase];
						}
					}
				}

				Instrument::Stats::_phasesSpinLock.readUnlock();

				return _phaseInfo.back();
			}
		};

		extern SpinLock _threadInfoListSpinLock;
		extern std::list<ThreadInfo *> _threadInfoList;
		extern Timer _totalTime;
	}
}


#endif // INSTRUMENT_STATS_HPP
