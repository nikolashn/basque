// See LICENSE for copyright/license information

#include "ctr.h"
#include "lexeme.h"
#include "dynarr.h"
#include "symtable.h"

struct ba_Ctr* ba_NewCtr() {
	struct ba_Ctr* ctr = ba_MAlloc(sizeof(struct ba_Ctr));

	ctr->startLex = ba_NewLexeme();
	ctr->lex = ctr->startLex;
	ctr->dir = 0;
	ctr->currPath = 0;

	ctr->pTkStk = ba_NewStk();
	ctr->pOpStk = ba_NewStk();
	ctr->pBreakStk = ba_NewStk();
	ctr->shortCircLblStk = ba_NewStk();
	ctr->cmpLblStk = ba_NewStk();

	ctr->cmpRegStk = ba_NewStk();
	ctr->cmpRegStk->count = 1;
	ctr->cmpRegStk->items[0] = (void*)0;

	ctr->funcFrameStk = ba_NewStk();
	ctr->expCoercedTypeStk = ba_NewStk();

	ctr->startIM = ba_NewIM();
	ctr->im = ctr->startIM;
	ctr->entryIM = ctr->startIM;

	ctr->globalST = ba_NewSymTable();
	ctr->globalST->frameScope = ctr->globalST;
	ctr->currScope = ctr->globalST;

	ctr->labelTable = ba_NewHashTable();
	ctr->inclInodes = ba_NewDynArr64(0x400);
	ctr->usedRegisters = 0;
	ctr->imStackSize = 0;
	ctr->statics = ba_NewDynArr64(0x400);
	ctr->labelCnt = 1; // Starts at 1 since label 0 means no label found
	ctr->isPermitArrLit = 0;

	ctr->paren = 0;
	ctr->bracket = 0;

	ctr->genImStk = ba_NewStk();
	ctr->genImStk->count = 1;
	ctr->genImStk->items[0] = (void*)1;

	return ctr;
}

void ba_DelCtr(struct ba_Ctr* ctr) {
	ba_DelLexeme(ctr->startLex);
	ba_DelLexeme(ctr->lex);

	for (u64 i = 0; i < ctr->pTkStk->cap; i++) {
		free(ctr->pTkStk->items[i]);
	}
	ba_DelStk(ctr->pTkStk);
	for (u64 i = 0; i < ctr->pOpStk->cap; i++) {
		free(ctr->pOpStk->items[i]);
	}
	ba_DelStk(ctr->pOpStk);
	for (u64 i = 0; i < ctr->pBreakStk->cap; i++) {
		free(ctr->pBreakStk->items[i]);
	}
	ba_DelStk(ctr->pBreakStk);
	for (u64 i = 0; i < ctr->shortCircLblStk->cap; i++) {
		free(ctr->shortCircLblStk->items[i]);
	}
	ba_DelStk(ctr->shortCircLblStk);

	ba_DelIM(ctr->startIM);
	ba_DelIM(ctr->im);

	ba_DelSymTable(ctr->globalST);
	ba_DelHashTable(ctr->labelTable);
	ba_DelDynArr64(ctr->inclInodes);
	ba_DelDynArr64(ctr->statics);

	free(ctr);
}

void ba_AddIM(struct ba_Ctr* ctr, u64 count, ...) {
	if (!ba_StkTop(ctr->genImStk)) {
		return;
	}

	ctr->im->vals = ba_MAlloc(sizeof(u64) * count);
	
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

