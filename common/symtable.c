// See LICENSE for copyright/license information

#include "symtable.h"
#include "exitmsg.h"
#include "func.h"

struct ba_SymTable* ba_NewSymTable() {
	struct ba_SymTable* st = ba_MAlloc(sizeof(*st));
	st->ht = ba_NewHashTable();
	st->parent = 0;
	st->frameScope = 0;
	st->children = 0;
	st->childCnt = 0;
	st->childCap = 0;
	st->depth = 0;
	st->dataSize = 0;
	st->func = 0;
	st->hasFramePtrLink = 0;
	return st;
}

void ba_DelSymTable(struct ba_SymTable* st) {
	ba_DelHashTable(st->ht);
	free(st);
}

struct ba_SymTable* ba_SymTableAddChild(struct ba_SymTable* parent) {
	struct ba_SymTable* child = ba_NewSymTable();
	child->parent = parent;
	child->depth = parent->depth + 1;
	child->func = parent->func;
	child->frameScope = parent->frameScope;
	if (!parent->childCap) {
		parent->childCap = 0x20;
		parent->children = ba_MAlloc(0x20 * sizeof(*parent->children));
	}
	else if (parent->childCap >= parent->childCnt) {
		parent->childCap <<= 1;
		parent->children = ba_Realloc(parent->children, 
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
			if (stFoundInPtr) {
				*stFoundInPtr = st;
			}
			return get;
		}
		st = st->parent;
	}
	return 0;
}

i64 ba_CalcVarOffset(struct ba_Ctr* ctr, struct ba_STVal* id, bool* isPushRbp) {
	*isPushRbp = 0;
	i64 address = -id->address;
	struct ba_SymTable* lastScope = 0;
	struct ba_SymTable* scope = ctr->currScope;
	i64 linksPassed = 0;

	while (scope && scope != id->scope) {
		lastScope = scope;
		scope = scope->parent;
		if (!scope) {
			fprintf(stderr, "Error: identifier used in scope that is not a "
				"descendant of its own scope\n");
			exit(1);
		}

		if (!lastScope->func || lastScope != lastScope->func->childScope) {
			if (!linksPassed && lastScope->hasFramePtrLink) {
				++linksPassed;
			}
			if (linksPassed) {
				address += scope->dataSize + 8 * lastScope->hasFramePtrLink;
			}
		}
		else { // outermost scope of a func
			if (!*isPushRbp) {
				*isPushRbp = 1;
				ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
			}
			++linksPassed;
			while (linksPassed) {
				ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RBP, BA_IM_ADR, BA_IM_RBP);
				--linksPassed;
			}
			address = -id->address;
		}
	}

	if (!linksPassed) {
		while (scope && !scope->hasFramePtrLink) {
			scope = scope->parent;
			address -= scope ? scope->dataSize : 0;
		}
	}

	return address;
}

