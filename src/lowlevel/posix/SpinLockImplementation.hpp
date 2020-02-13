/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef POSIX_SPIN_LOCK_IMPLEMENTATION_HPP
#define POSIX_SPIN_LOCK_IMPLEMENTATION_HPP


#ifndef SPIN_LOCK_HPP
	#error Include SpinLock.h instead
#endif


template <class DEBUG_KIND>
inline CustomizableSpinLock<DEBUG_KIND>::CustomizableSpinLock()
{
	pthread_spin_init(&_lock, PTHREAD_PROCESS_PRIVATE);
}

template <class DEBUG_KIND>
inline CustomizableSpinLock<DEBUG_KIND>::~CustomizableSpinLock()
{
	DEBUG_KIND::assertUnowned();
	pthread_spin_destroy(&_lock);
}

template <class DEBUG_KIND>
inline void CustomizableSpinLock<DEBUG_KIND>::lock()
{
	DEBUG_KIND::assertNotCurrentOwner();
	DEBUG_KIND::willLock();
	pthread_spin_lock(&_lock);
	DEBUG_KIND::assertUnowned();
	DEBUG_KIND::setOwner();
}

template <class DEBUG_KIND>
inline bool CustomizableSpinLock<DEBUG_KIND>::tryLock()
{
	DEBUG_KIND::assertNotCurrentOwner();

	bool success = (pthread_spin_trylock(&_lock) == 0);

	if (success) {
		DEBUG_KIND::assertUnowned();
		DEBUG_KIND::setOwner();
	}

	return success;
}

template <class DEBUG_KIND>
inline void CustomizableSpinLock<DEBUG_KIND>::unlock(bool ignoreOwner)
{
	DEBUG_KIND::assertCurrentOwner(ignoreOwner);
	DEBUG_KIND::unsetOwner();
	pthread_spin_unlock(&_lock);
}


#endif // POSIX_SPIN_LOCK_IMPLEMENTATION_HPP
