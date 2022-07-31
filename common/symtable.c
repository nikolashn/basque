// See LICENSE for copyright/license information

#include "symtable.h"
#include "exitmsg.h"

struct ba_SymTable* ba_NewSymTable() {
	struct ba_SymTable* st = malloc(sizeof(*st));
	if (!st) {
		ba_ErrorMallocNoMem();
	}

	st->ht = ba_NewHashTable();
	if (!st->ht) {
		ba_ErrorMallocNoMem();
	}

	st->parent = 0;
	st->children = 0;
	st->childCnt = 0;
	st->childCap = 0;
	st->dataSize = 0;

	return st;
}

void ba_DelSymTable(struct ba_SymTable* st) {
	ba_DelHashTable(st->ht);
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

// stFoundInPtr must be a real pointer otherwise null pointer dereferencing
struct ba_STVal* ba_STParentFind(struct ba_SymTable* st, 
	struct ba_SymTable** stFoundInPtr, char* key)
{
	while (st) {
		struct ba_STVal* get = ba_HTGet(st->ht, key);
		if (get) {
			*stFoundInPtr = st;
			return get;
		}
		st = st->parent;
	}
	return 0;
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
			"descendant of its own scope\n");
		exit(-1);
	}
	return stackStart - id->address;
}
