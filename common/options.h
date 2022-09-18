// See LICENSE for copyright/license information

#ifndef BA__COMMON_OPTIONS_H
#define BA__COMMON_OPTIONS_H

#include "common.h"

void ba_SetPageSize(u64 sz);
u64 ba_GetPageSize();
void ba_SetIncludePath(char* path, bool isOverrideDefaults);
char* ba_GetIncludePath();
void ba_SetAssertions(bool has);
bool ba_HasAssertions();

#endif
