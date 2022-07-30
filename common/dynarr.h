// See LICENSE for copyright/license information

#ifndef BA__COMMON_DYNARR_H
#define BA__COMMON_DYNARR_H

#include "common.h"

struct ba_DynArr8 {
	u8* arr;
	u64 cap;
	u64 cnt;
};

struct ba_DynArr64 {
	u64* arr;
	u64 cap;
	u64 cnt;
};

struct ba_DynArr8* ba_NewDynArr8(u64 initCap);
u8 ba_ResizeDynArr8(struct ba_DynArr8* da);
u8* ba_TopDynArr8(struct ba_DynArr8* da);
void ba_DelDynArr8(struct ba_DynArr8* da);
struct ba_DynArr64* ba_NewDynArr64(u64 initCap);
u8 ba_ResizeDynArr64(struct ba_DynArr64* da);
u64* ba_TopDynArr64(struct ba_DynArr64* da);
void ba_DelDynArr64(struct ba_DynArr64* da);

#endif
