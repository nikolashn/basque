// See LICENSE for copyright/license information

#ifndef BA__COMMON_U64TOSTR_H
#define BA__COMMON_U64TOSTR_H

#include "common.h"
#include "exitmsg.h"

u64 ba_StrToU64(char* str, u64 line, u64 col, char* path);
char* ba_U64ToStr(u64 num);

#endif
