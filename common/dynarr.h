// See LICENSE for copyright/license information

#ifndef BA__DYNARR_H
#define BA__DYNARR_H

// 8 bit

struct ba_DynArr8 {
	u8* arr;
	u64 cap;
	u64 cnt;
};

struct ba_DynArr8* ba_NewDynArr8(u64 initCap) {
	struct ba_DynArr8* da = calloc(1, sizeof(*da));
	if (!da) {
		ba_ErrorMallocNoMem();
	}
	da->cap = initCap;
	da->arr = malloc(initCap);
	if (!da->arr) {
		ba_ErrorMallocNoMem();
	}
	return da;
}

u8 ba_ResizeDynArr8(struct ba_DynArr8* da) {
	da->cap <<= 1;
	da->arr = realloc(da->arr, da->cap);
	if (!da->arr) {
		return ba_ErrorMallocNoMem();
	}
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

struct ba_DynArr64 {
	u64* arr;
	u64 cap;
	u64 cnt;
};

struct ba_DynArr64* ba_NewDynArr64(u64 initCap) {
	struct ba_DynArr64* da = calloc(1, sizeof(*da));
	if (!da) {
		ba_ErrorMallocNoMem();
	}
	da->cap = initCap;
	da->arr = malloc(initCap * sizeof(u64));
	if (!da->arr) {
		ba_ErrorMallocNoMem();
	}
	return da;
}

u8 ba_ResizeDynArr64(struct ba_DynArr64* da) {
	da->cap <<= 1;
	da->arr = realloc(da->arr, da->cap * sizeof(u64));
	if (!da->arr) {
		return ba_ErrorMallocNoMem();
	}
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

#endif
