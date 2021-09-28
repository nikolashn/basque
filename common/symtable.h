// See LICENSE for copyright/license information

#ifndef BA__SYMTABLE_H
#define BA__SYMTABLE_H

struct ba_STVal {
	u64 type;
	u64 address; // relative to global memory start address
	void* initVal;
	u8 isInited;
};

struct ba_STEntry {
	char* key;
	struct ba_STVal* val;
};

struct ba_SymTable {
	struct ba_SymTable* parent;
	struct ba_SymTable** children;
	u64 childCount;

	struct ba_STEntry* entries;
	u64 capacity;
	u64 count;
};

struct ba_SymTable* ba_NewSymTable() {
	struct ba_SymTable* st = malloc(sizeof(struct ba_SymTable));
	st->parent = 0;
	st->children = 0;
	st->childCount = 0;

	st->entries = calloc(BA_SYMTABLE_CAPACITY, sizeof(struct ba_STEntry));
	st->capacity = BA_SYMTABLE_CAPACITY;
	st->count = 0;
	return st;
}

void ba_DelSymTable(struct ba_SymTable* st) {
	free(st->entries);
	free(st);
}

u64 ba_Hash(char* str) {
	u64 hash = 0;
	u64 c;
	while ((c = *str)) {
		hash = c + (hash << 6) + (hash << 16) - hash;
		++str;
	}
	return hash;
}

struct ba_STVal* ba_STGet(struct ba_SymTable* st, char* key) {
	u64 hash = ba_Hash(key);
	u64 index = (u64)(hash & (u64)(st->capacity - 1));

	while (st->entries[index].key) {
		if (!strcmp(key, st->entries[index].key)) {
			return st->entries[index].val;
		}
		++index;
		if (index >= st->capacity) {
			index = 0;
		}
	}

	return 0;
}

char* ba_STSetNoExpand(struct ba_SymTable* st, char* key, struct ba_STVal* val) {
	u64 hash = ba_Hash(key);
	u64 index = (u64)(hash & (u64)(st->capacity - 1));

	while (st->entries[index].key) {
		if (!strcmp(key, st->entries[index].key)) {
			st->entries[index].val = val;
			return st->entries[index].key;
		}

		++index;
		if (index >= st->capacity) {
			index = 0;
		}
	}

	key = strdup(key);
	if (!key) {
		return 0;
	}
	++(st->count);

	st->entries[index].key = key;
	st->entries[index].val = val;

	return key;
}

int ba_STExpand(struct ba_SymTable* st) {
	if (st->capacity >= (1llu << 62)) {
		return 0;
	}
	
	u64 newCapacity = st->capacity << 1;
	
	struct ba_STEntry* newEntries = calloc(newCapacity, sizeof(struct ba_STEntry));
	if (!newEntries) {
		return 0;
	}
	
	for (u64 i = 0; i < st->capacity; i++) {
		ba_STSetNoExpand(st, st->entries[i].key, st->entries[i].val);
	}

	free(st->entries);
	st->entries = newEntries;
	st->capacity = newCapacity;
	return 1;
}

char* ba_STSet(struct ba_SymTable* st, char* key, struct ba_STVal* val) {
	if (!val) {
		return 0;
	}

	if (st->count >= (st->capacity >> 1)) {
		if (!ba_STExpand(st)) {
			return 0;
		}
	}
	
	ba_STSetNoExpand(st, key, val);
	return key;
}

#endif
