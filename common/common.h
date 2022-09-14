// See LICENSE for copyright/license information

#ifndef BA__COMMON_H
#define BA__COMMON_H

// ----- C Includes -----

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdarg.h"
#include "unistd.h"
#include "libgen.h"
#include "sys/stat.h"

// ----- Defines -----

#define BA_VERSION "1.0.0"
// Must be at least 4 kiB
#define BA_FILE_BUF_SIZE 64 * 1024
#define BA_PATH_BUF_SIZE 4095

#define BA_HASHTABLE_CAPACITY 1024
#define BA_STACK_SIZE 2 * 1024 * 1024

// Don't include the final '\0'
#define BA_IDENTIFIER_SIZE 255
#define BA_LITERAL_SIZE 4095

// ----- Typedefs/structs -----

typedef unsigned long long u64;
typedef signed long long i64;
typedef _Bool bool;
typedef unsigned char u8;
typedef signed char i8;
typedef double f64;
typedef float f32;

// ----- Funcs -----

void* ba_MAlloc(u64 size);
void* ba_Realloc(void* ptr, u64 size);
void* ba_CAlloc(u64 itemCnt, u64 itemSize);

#endif
