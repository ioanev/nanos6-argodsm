/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020 Barcelona Supercomputing Center (BSC)
*/

#include "resolve.h"


RESOLVE_API_FUNCTION(nanos6_numa_alloc_block_interleave, "numa", NULL);
RESOLVE_API_FUNCTION(nanos6_numa_alloc_sentinels, "numa", NULL);
RESOLVE_API_FUNCTION(nanos6_numa_free, "numa", NULL);
RESOLVE_API_FUNCTION(nanos6_bitmask_clearall, "numa", NULL);
RESOLVE_API_FUNCTION(nanos6_bitmask_clearbit, "numa", NULL);
RESOLVE_API_FUNCTION(nanos6_bitmask_setall, "numa", NULL);
RESOLVE_API_FUNCTION(nanos6_bitmask_setbit, "numa", NULL);
RESOLVE_API_FUNCTION(nanos6_bitmask_set_wildcard, "numa", NULL);
RESOLVE_API_FUNCTION(nanos6_bitmask_isbitset, "numa", NULL);
RESOLVE_API_FUNCTION(nanos6_count_setbits, "numa", NULL);
