// See LICENSE for copyright/license information

#ifndef BA__CTR_H
#define BA__CTR_H

#include "stack.h"
#include "func.h"

struct ba_Controller {
	// Lexer
	struct ba_Lexeme* startLex;
	struct ba_Lexeme* lex;

	// Parser
	struct ba_Stk* pTkStk; // Takes ba_PTkStkItem as items
	struct ba_Stk* pOpStk; // Takes ba_POpStkItem as items
	struct ba_SymTable* globalST;
	struct ba_SymTable* currScope;
	struct ba_HashTable* labelTable;
	struct ba_DynArr64* inclInodes;
	char* dir;
	char* currPath;

	// Stores labels used in short circuiting (&& and || operators)
	struct ba_Stk* shortCircLblStk; // Takes u64 (label IDs) as items
	// Used in comparison chains
	struct ba_Stk* cmpLblStk; // Takes u64 (label IDs) as items
	struct ba_Stk* cmpRegStk; // Takes u64 (im enum for registers) as items
	// Used in breaking out of loops and returning from funcs
	struct ba_Stk* breakLblStk; // Takes u64 (label IDs) as items
	// Func stack frame
	struct ba_Stk* funcFrameStk; // Takes u64 (usedRegisters) as items

	// Used with return statements
	struct ba_Func* currFunc;

	// Code generation
	struct ba_IM* startIM;
	struct ba_IM* im;
	struct ba_IM* entryIM;
	
	// Parser
	u64 usedRegisters;
	u64 imStackSize;
	u64 staticSize;
	u64 labelCnt;
	// Counts expression parentheses etc. to make sure they are balanced
	i64 paren;
	i64 bracket;
};

struct ba_Controller* ba_NewController() {
	struct ba_Controller* ctr = malloc(sizeof(struct ba_Controller));
	if (!ctr) {
		ba_ErrorMallocNoMem();
	}
	ctr->startLex = ba_NewLexeme();
	ctr->lex = ctr->startLex;
	ctr->dir = 0;
	ctr->currPath = 0;
	ctr->pTkStk = ba_NewStk();
	ctr->pOpStk = ba_NewStk();
	ctr->shortCircLblStk = ba_NewStk();
	ctr->cmpLblStk = ba_NewStk();
	ctr->cmpRegStk = ba_NewStk();
	ctr->cmpRegStk->count = 1;
	ctr->cmpRegStk->items[0] = (void*)0;
	ctr->breakLblStk = ba_NewStk();
	ctr->funcFrameStk = ba_NewStk();
	ctr->startIM = ba_NewIM();
	ctr->im = ctr->startIM;
	ctr->entryIM = ctr->startIM;
	ctr->globalST = ba_NewSymTable();
	ctr->currScope = ctr->globalST;
	ctr->labelTable = ba_NewHashTable();
	ctr->inclInodes = ba_NewDynArr64(0x400);
	ctr->usedRegisters = 0;
	ctr->imStackSize = 0;
	ctr->staticSize = 0;
	ctr->labelCnt = 1; // Starts at 1 since label 0 means no label found
	ctr->currFunc = 0;
	ctr->paren = 0;
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
	ba_DelStk(ctr->shortCircLblStk);
	for (u64 i = 0; i < ctr->shortCircLblStk->cap; i++) {
		free(ctr->shortCircLblStk->items[i]);
	}
	ba_DelStk(ctr->pOpStk);

	ba_DelIM(ctr->startIM);
	ba_DelIM(ctr->im);

	ba_DelSymTable(ctr->globalST);
	ba_DelHashTable(ctr->labelTable);
	ba_DelDynArr64(ctr->inclInodes);

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
