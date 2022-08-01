// See LICENSE for copyright/license information

#include "common.h"

void* ba_MAlloc(u64 size) {
	void* ptr = malloc(size);
	if (!ptr) {
		fprintf(stderr, "Error: insufficient memory\n");
		exit(-1);
		return 0;
	}
	return ptr;
}

void* ba_Realloc(void* ptr, u64 size) {
	ptr = realloc(ptr, size);
	if (!ptr) {
		fprintf(stderr, "Error: insufficient memory\n");
		exit(-1);
		return 0;
	}
	return ptr;
}

void* ba_CAlloc(u64 itemCnt, u64 itemSize) {
	void* ptr = calloc(itemCnt, itemSize);
	if (!ptr) {
		fprintf(stderr, "Error: insufficient memory\n");
		exit(-1);
		return 0;
	}
	return ptr;
}

