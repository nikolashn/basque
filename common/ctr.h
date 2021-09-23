// See LICENSE for copyright/license information

#ifndef BA__CTR_H
#define BA__CTR_H

// ----- Stack -----

struct ba_Stk {
	u64 cap;
	u64 count;
	void** items;
};

struct ba_Stk* ba_NewStk() {
	struct ba_Stk* stk = malloc(sizeof(struct ba_Stk));
	stk->cap = 64;
	stk->count = 0;
	stk->items = malloc(stk->cap * sizeof(void*));
	return stk;
}

void ba_DelStk(struct ba_Stk* stk) {
	free(stk->items);
	free(stk);
}

void* ba_StkPush(void* item, struct ba_Stk* stk) {
	if (stk->count >= stk->cap) {
		if (stk->cap >= (1llu << 32)) {
			printf("Error: overflow in compiler controller stack\n");
			exit(-1);
			return 0;
		}
		stk->items = realloc(stk->items, stk->cap * 2);
	}
	else {
		stk->items[stk->count] = item;
		++stk->count;
	}
	return item;
}

void* ba_StkPop(struct ba_Stk* stk) {
	if (!stk->count) {
		return 0;
	}
	return stk->items[--stk->count];
}

// ----- Controller -----

struct ba_Controller {
	struct ba_Lexeme* startLex;
	struct ba_Lexeme* lex;
	struct ba_Stk* stk;
	struct ba_IM* startIM;
	struct ba_IM* im;
	struct ba_SymTable* globalST;
	
	// Must be a type.h enum
	u64 lastType;
};

struct ba_Controller* ba_NewController() {
	struct ba_Controller* ctr = malloc(sizeof(struct ba_Controller));
	ctr->startLex = ba_NewLexeme();
	ctr->lex = ctr->startLex;
	ctr->stk = ba_NewStk();
	ctr->startIM = ba_NewIM();
	ctr->im = ctr->startIM;
	ctr->globalST = ba_NewSymTable();
	return ctr;
}

void ba_DelController(struct ba_Controller* ctr) {
	ba_DelLexeme(ctr->startLex);
	ba_DelLexeme(ctr->lex);
	ba_DelStk(ctr->stk);
	ba_DelIM(ctr->startIM);
	ba_DelIM(ctr->im);
	ba_DelSymTable(ctr->globalST);
	free(ctr);
}

void ba_AddIM(struct ba_Controller* ctr, u64 count, ...) {
	ctr->im->vals = malloc(sizeof(u64) * count);
	
	va_list vals;
	va_start(vals, count);
	for (u64 i = 0; i < count; i++) {
		ctr->im->vals[i] = va_arg(vals, u64);
	}
	va_end(vals);

	ctr->im->count = count;
	ctr->im->next = ba_NewIM();
	
	//printf("%s\n", ba_IMToStr(ctr->im)); // DEBUG

	ctr->im = ctr->im->next;
}

#endif
