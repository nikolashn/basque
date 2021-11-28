// See LICENSE for copyright/license information

#ifndef BA__STACK_H
#define BA__STACK_H

// General stack structure
// The type of items should be known based on which stack is used
struct ba_Stk {
	u64 cap;
	u64 count;
	void** items;
};

struct ba_Stk* ba_NewStk() {
	struct ba_Stk* stk = malloc(sizeof(*stk));
	if (!stk) {
		ba_ErrorMallocNoMem();
	}
	stk->cap = 64;
	stk->count = 0;
	stk->items = malloc(stk->cap * sizeof(*stk->items));
	if (!stk->items) {
		ba_ErrorMallocNoMem();
	}
	return stk;
}

void ba_DelStk(struct ba_Stk* stk) {
	free(stk->items);
	free(stk);
}

u8 ba_ResizeStk(struct ba_Stk* stk) {
	stk->cap <<= 1;
	stk->items = realloc(stk->items, stk->cap * sizeof(*stk->items));
	if (!stk->items) {
		return ba_ErrorMallocNoMem();
	}
	return 1;
}

void* ba_StkPush(void* item, struct ba_Stk* stk) {
	(stk->count >= stk->cap) && ba_ResizeStk(stk);
	stk->items[stk->count] = item;
	++stk->count;
	return item;
}

void* ba_StkPop(struct ba_Stk* stk) {
	if (!stk->count) {
		return 0;
	}
	return stk->items[--stk->count];
}

void* ba_StkTop(struct ba_Stk* stk) {
	if (!stk->count) {
		return 0;
	}
	return stk->items[stk->count-1];
}

// The parser pushes data from read lexemes onto the stack for optimized 
// calculation and intermediate code generation. Data is stored in ctr->pTkStk 
// whereas operators are stored in ctr->pOpStk.

struct ba_PTkStkItem {
	void* val;
	u64 type;
	u64 lexemeType;
};

struct ba_POpStkItem {
	u64 line;
	u64 col;
	u64 lexemeType;
	u8 syntax; // i.e. infix, prefix, postfix
};

enum {
	BA_OP_PREFIX = 1,
	BA_OP_INFIX = 2,
	BA_OP_POSTFIX = 3,
};

#endif
