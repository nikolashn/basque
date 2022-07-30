// See LICENSE for copyright/license information

#include "elf.h"

u64 pageSize = 0;

void ba_SetPageSize(u64 sz) {
	pageSize = sz;
}

u64 ba_GetPageSize() {
	return pageSize;
}

