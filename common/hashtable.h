// See LICENSE for copyright/license information

#ifndef BA__COMMON_HASHTABLE_H
#define BA__COMMON_HASHTABLE_H

#include "common.h"

struct ba_HTEntry {
	char* key;
	void* val;
};

struct ba_HashTable {
	struct ba_HTEntry* entries;
	u64 capacity;
	u64 count;
};

u64 ba_Hash(char* str);
struct ba_HashTable* ba_NewHashTable();
void ba_DelHashTable(struct ba_HashTable* ht);
void* ba_HTGet(struct ba_HashTable* ht, char* key);
char* ba_HTSetNoExpand(struct ba_HashTable* ht, char* key, void* val);
bool ba_HTExpand(struct ba_HashTable* ht);
char* ba_HTSet(struct ba_HashTable* ht, char* key, void* val);

#endif
