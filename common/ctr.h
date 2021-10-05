// See LICENSE for copyright/license information

#ifndef BA__CTR_H
#define BA__CTR_H

#include "stack.h"

struct ba_Controller {
	struct ba_Lexeme* startLex;
	struct ba_Lexeme* lex;
	struct ba_Stk* pTkStk; // Takes ba_PTkStkItem as items
	struct ba_Stk* pOpStk; // Takes ba_POpStkItem as items
	struct ba_IM* startIM;
	struct ba_IM* im;
	struct ba_SymTable* globalST;
	struct ba_SymTable* currScope;
	
	u64 dataSgmtSize;
};

void ba_PTkStkPush(struct ba_Controller* ctr, void* val, 
	u64 type, u64 lexemeType)
{
	struct ba_PTkStkItem* stkItem = malloc(sizeof(struct ba_PTkStkItem));
	if (!stkItem) {
		ba_ErrorMallocNoMem();
	}
	stkItem->val = val;
	stkItem->type = type;
	stkItem->lexemeType = lexemeType;
	ba_StkPush((void*)stkItem, ctr->pTkStk);
}

void ba_POpStkPush(struct ba_Controller* ctr, u64 line, u64 col, 
	u64 lexemeType, u8 syntax)
{
	struct ba_POpStkItem* stkItem = malloc(sizeof(struct ba_POpStkItem));
	if (!stkItem) {
		ba_ErrorMallocNoMem();
	}
	stkItem->line = line;
	stkItem->col = col;
	stkItem->lexemeType = lexemeType;
	stkItem->syntax = syntax;
	ba_StkPush((void*)stkItem, ctr->pOpStk);
}

struct ba_Controller* ba_NewController() {
	struct ba_Controller* ctr = malloc(sizeof(struct ba_Controller));
	if (!ctr) {
		ba_ErrorMallocNoMem();
	}
	ctr->startLex = ba_NewLexeme();
	ctr->lex = ctr->startLex;
	ctr->pTkStk = ba_NewStk();
	ctr->pOpStk = ba_NewStk();
	ctr->startIM = ba_NewIM();
	ctr->im = ctr->startIM;
	ctr->globalST = ba_NewSymTable();
	ctr->currScope = ctr->globalST;
	ctr->dataSgmtSize = 0;
	return ctr;
}

void ba_DelController(struct ba_Controller* ctr) {
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

	ba_DelIM(ctr->startIM);
	ba_DelIM(ctr->im);

	ba_DelSymTable(ctr->globalST);

	free(ctr);
}

void ba_AddIM(struct ba_Controller* ctr, u64 count, ...) {
	ctr->im->vals = malloc(sizeof(u64) * count);
	if (!ctr->im->vals) {
		ba_ErrorMallocNoMem();
	}
	
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
