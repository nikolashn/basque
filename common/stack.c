// See LICENSE for copyright/license information

#include "stack.h"
#include "exitmsg.h"

struct ba_Stk* ba_NewStk() {
	struct ba_Stk* stk = ba_MAlloc(sizeof(*stk));
	stk->cap = 64;
	stk->count = 0;
	stk->items = ba_MAlloc(stk->cap * sizeof(*stk->items));
	return stk;
}

void ba_DelStk(struct ba_Stk* stk) {
	free(stk->items);
	free(stk);
}

u8 ba_ResizeStk(struct ba_Stk* stk) {
	stk->cap <<= 1;
	stk->items = ba_Realloc(stk->items, stk->cap * sizeof(*stk->items));
	return 1;
}

void* ba_StkPush(struct ba_Stk* stk, void* item) {
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

