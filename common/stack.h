// See LICENSE for copyright/license information

#ifndef BA__COMMON_STACK_H
#define BA__COMMON_STACK_H

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

#endif
