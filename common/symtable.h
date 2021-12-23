// See LICENSE for copyright/license information

#ifndef BA__SYMTABLE_H
#define BA__SYMTABLE_H

struct ba_SymTable;

struct ba_STVal {
	struct ba_SymTable* scope;
	
	/* If a label, the label ID
	 * If global, relative to global memory start address
	 * If in a scope, relative to start of scope's stack */
	u64 address;

	u64 type;
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
	u64 childCnt;
	u64 childCap;

	struct ba_STEntry* entries;
	u64 capacity;
	u64 count;
	
	/* If global, size of the data segment
	 * If in a scope, size of stack */
	u64 dataSize;
};

struct ba_SymTable* ba_NewSymTable() {
	struct ba_SymTable* st = malloc(sizeof(struct ba_SymTable));
	if (!st) {
		ba_ErrorMallocNoMem();
	}
	st->parent = 0;
	st->children = 0;
	st->childCnt = 0;
	st->childCap = 0;
	
	st->entries = calloc(BA_SYMTABLE_CAPACITY, sizeof(struct ba_STEntry));
	if (!st->entries) {
		ba_ErrorMallocNoMem();
	}

	st->capacity = BA_SYMTABLE_CAPACITY;
	st->count = 0;

	st->dataSize = 0;

	return st;
}

void ba_DelSymTable(struct ba_SymTable* st) {
	free(st->entries);
	free(st);
}

struct ba_SymTable* ba_SymTableAddChild(struct ba_SymTable* parent) {
	struct ba_SymTable* child = ba_NewSymTable();
	child->parent = parent;
	if (!parent->childCap) {
		parent->childCap = 0x20;
		parent->children = malloc(0x20 * sizeof(*parent->children));
	}
	else if (parent->childCap >= parent->childCnt) {
		parent->childCap <<= 1;
		parent->children = realloc(parent->children, 
			parent->childCap * sizeof(*parent->children));
	}
	parent->children[parent->childCnt++] = child;
	return child;
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

struct ba_STVal* ba_SymTableSearchChildren(struct ba_SymTable* st, char* key) {
	struct ba_STVal* idVal = ba_STGet(st, key);
	if (idVal) {
		return idVal;
	}
	for (u64 i = 0; i < st->childCnt; i++) {
		idVal = ba_SymTableSearchChildren(st->children[i], key);
		if (idVal) {
			return idVal;
		}
	}
	return 0;
}

// stFoundIn must be a real pointer otherwise null pointer dereferencing
struct ba_STVal* ba_STParentFind(struct ba_SymTable* st, 
	struct ba_SymTable** stFoundIn, char* key)
{
	while (st) {
		struct ba_STVal* get = ba_STGet(st, key);
		if (get) {
			*stFoundIn = st;
			return get;
		}
		st = st->parent;
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
		return ba_ErrorMallocNoMem();
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

// Offset from RSP if ctr->imStackSize == 0, otherwise RBP
u64 ba_CalcSTValOffset(struct ba_SymTable* currScope, struct ba_STVal* id) {
	u64 stackStart = id->scope->dataSize;
	while (currScope && currScope != id->scope) {
		stackStart += currScope->dataSize;
		currScope = currScope->parent;
	}
	if (!currScope) {
		fprintf(stderr, "Error: identifier used in scope that is not a "
			"descendant of its own scope");
		exit(-1);
	}
	return stackStart - id->address;
}

#endif
