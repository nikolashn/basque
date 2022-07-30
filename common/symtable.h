// See LICENSE for copyright/license information

#ifndef BA__COMMON_SYMTABLE_H
#define BA__COMMON_SYMTABLE_H

#include "types.h"
#include "hashtable.h"

struct ba_SymTable {
	struct ba_HashTable* ht;
	struct ba_SymTable* parent;
	struct ba_SymTable** children;
	u64 childCnt;
	u64 childCap;
	u64 dataSize; // Size of the stack
};

struct ba_STVal {
	struct ba_Type type;
	struct ba_SymTable* scope;
	
	/* If a label, the label ID
	 * If a variable, relative to start of scope's stack */
	u64 address;
	
	bool isInited;
//	void* initVal; // A relic of ancient code. TODO: use this for consts
};

struct ba_SymTable* ba_NewSymTable();
void ba_DelSymTable(struct ba_SymTable* st);
struct ba_SymTable* ba_SymTableAddChild(struct ba_SymTable* parent);
struct ba_STVal* ba_STParentFind(struct ba_SymTable* st, 
	struct ba_SymTable** stFoundInPtr, char* key);
u64 ba_CalcSTValOffset(struct ba_SymTable* currScope, struct ba_STVal* id);

#endif
