// See LICENSE for copyright/license information

#include "dynarr.h"
#include "exitmsg.h"

// 8 bit

struct ba_DynArr8* ba_NewDynArr8(u64 initCap) {
	struct ba_DynArr8* da = ba_CAlloc(1, sizeof(*da));
	da->cap = initCap;
	da->arr = ba_MAlloc(initCap);
	return da;
}

u8 ba_ResizeDynArr8(struct ba_DynArr8* da) {
	da->cap <<= 1;
	da->arr = ba_Realloc(da->arr, da->cap);
	return 1;
}

u8* ba_TopDynArr8(struct ba_DynArr8* da) {
	if (!da->cnt) {
		return 0;
	}
	return da->arr + da->cnt - 1;
}

void ba_DelDynArr8(struct ba_DynArr8* da) {
	free(da->arr);
	free(da);
}

// 64 bit

struct ba_DynArr64* ba_NewDynArr64(u64 initCap) {
	struct ba_DynArr64* da = ba_CAlloc(1, sizeof(*da));
	da->cap = initCap;
	da->arr = ba_MAlloc(initCap * sizeof(u64));
	return da;
}

u8 ba_ResizeDynArr64(struct ba_DynArr64* da) {
	da->cap <<= 1;
	da->arr = ba_Realloc(da->arr, da->cap * sizeof(u64));
	return 1;
}

u64* ba_TopDynArr64(struct ba_DynArr64* da) {
	if (!da->cnt) {
		return 0;
	}
	return da->arr + da->cnt - 1;
}

void ba_DelDynArr64(struct ba_DynArr64* da) {
	free(da->arr);
	free(da);
}

