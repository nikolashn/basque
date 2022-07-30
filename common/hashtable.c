// See LICENSE for copyright/license information

#include "hashtable.h"
#include "exitmsg.h"

u64 ba_Hash(char* str) {
	u64 hash = 0;
	u64 c;
	while ((c = *str)) {
		hash = c + (hash << 6) + (hash << 16) - hash;
		++str;
	}
	return hash;
}

struct ba_HashTable* ba_NewHashTable() {
	struct ba_HashTable* ht = malloc(sizeof(*ht));
	if (!ht) {
		ba_ErrorMallocNoMem();
	}
	
	ht->entries = calloc(BA_HASHTABLE_CAPACITY, sizeof(*ht->entries));
	if (!ht->entries) {
		ba_ErrorMallocNoMem();
	}

	ht->capacity = BA_HASHTABLE_CAPACITY;
	ht->count = 0;

	return ht;
}

void ba_DelHashTable(struct ba_HashTable* ht) {
	free(ht->entries);
	free(ht);
}

void* ba_HTGet(struct ba_HashTable* ht, char* key) {
	u64 hash = ba_Hash(key);
	u64 index = (u64)(hash & (u64)(ht->capacity - 1));

	while (ht->entries[index].key) {
		if (!strcmp(key, ht->entries[index].key)) {
			return ht->entries[index].val;
		}
		++index;
		if (index >= ht->capacity) {
			index = 0;
		}
	}

	return 0;
}

char* ba_HTSetNoExpand(struct ba_HashTable* ht, char* key, void* val) {
	u64 hash = ba_Hash(key);
	u64 index = (u64)(hash & (u64)(ht->capacity - 1));

	while (ht->entries[index].key) {
		if (!strcmp(key, ht->entries[index].key)) {
			ht->entries[index].val = val;
			return ht->entries[index].key;
		}

		++index;
		if (index >= ht->capacity) {
			index = 0;
		}
	}

	key = strdup(key);
	if (!key) {
		return 0;
	}
	++(ht->count);

	ht->entries[index].key = key;
	ht->entries[index].val = val;

	return key;
}

bool ba_HTExpand(struct ba_HashTable* ht) {
	if (ht->capacity >= (1llu << 62)) {
		return 0;
	}
	
	u64 newCapacity = ht->capacity << 1;
	
	struct ba_HTEntry* newEntries = calloc(newCapacity, sizeof(*newEntries));
	if (!newEntries) {
		return ba_ErrorMallocNoMem();
	}
	
	for (u64 i = 0; i < ht->capacity; i++) {
		ba_HTSetNoExpand(ht, ht->entries[i].key, ht->entries[i].val);
	}

	free(ht->entries);
	ht->entries = newEntries;
	ht->capacity = newCapacity;
	return 1;
}

char* ba_HTSet(struct ba_HashTable* ht, char* key, void* val) {
	if (!val) {
		return 0;
	}

	if (ht->count >= (ht->capacity >> 1)) {
		if (!ba_HTExpand(ht)) {
			return 0;
		}
	}
	
	ba_HTSetNoExpand(ht, key, val);
	return key;
}

