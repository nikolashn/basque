// See LICENSE for copyright/license information

#include "options.h"

u64 pageSize = 0;
char* includePath = 0;

void ba_SetPageSize(u64 sz) {
	pageSize = sz;
}

u64 ba_GetPageSize() {
	return pageSize;
}

void ba_SetIncludePath(char* path, bool isOverrideDefaults) {
	u64 len = strlen(path);
	includePath = malloc(len+1);
	strcpy(includePath, path);
}

char* ba_GetIncludePath() {
	return includePath;
}

