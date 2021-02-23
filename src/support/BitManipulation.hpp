/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020 Barcelona Supercomputing Center (BSC)
*/


#ifndef BIT_MANIPULATION_HPP
#define BIT_MANIPULATION_HPP

#include <cstdint>


class BitManipulation {
public:
	static inline int indexFirstEnabledBit(uint64_t x)
	{
		// The ffs function returns the least signficant enabled bit starting
		// from one, while returning zero means that has no enabled bits
		return __builtin_ffsll(x) - 1;
	}

	static inline void disableBit(uint64_t *x, uint64_t bitIndex)
	{
		*x &= ~((uint64_t) 1 << bitIndex);
	}

	static inline void enableBit(uint64_t *x, uint64_t bitIndex)
	{
		*x |= ((uint64_t) 1 << bitIndex);
	}

	static inline uint8_t checkBit(const uint64_t *x, uint64_t bitIndex)
	{
		uint8_t bit = ((*x >> bitIndex) & (uint64_t) 1);
		return bit;
	}

	static inline uint64_t countEnabledBits(const uint64_t *x)
	{
		return __builtin_popcountll(*x);
	}
};


#endif // BIT_MANIPULATION_HPP

