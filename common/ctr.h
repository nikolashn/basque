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

	// Stores labels used in short circuiting (&& and || operators)
	struct ba_Stk* shortCircLblStk; // Takes u64 (label IDs) as items
	// Used in comparison chains
	struct ba_Stk* cmpLblStk; // Takes u64 (label IDs) as items
	struct ba_Stk* cmpRegStk; // Takes u64 (im enum for registers) as items
	// Used in breaking out of loops and returning from funcs
	struct ba_Stk* breakLblStk; // Takes u64 (label IDs) as items

	// Used with return statements
	struct ba_Func* currFunc;

	// Code generation
	struct ba_IM* startIM;
	struct ba_IM* im;
	struct ba_IM* entryIM;
	
	// Parser
	u64 usedRegisters;
	u64 imStackSize;
	u64 labelCnt;
	// Counts expression parentheses to make sure they are balanced
	i64 paren;
};

void ba_PTkStkPush(struct ba_Stk* stk, void* val, 
	u64 type, u64 lexemeType, bool isLValue)
{
	struct ba_PTkStkItem* stkItem = malloc(sizeof(*stkItem));
	if (!stkItem) {
		ba_ErrorMallocNoMem();
	}
	stkItem->val = val;
	stkItem->type = type;
	stkItem->lexemeType = lexemeType;
	stkItem->isLValue = isLValue;
	ba_StkPush(stk, (void*)stkItem);
}

void ba_POpStkPush(struct ba_Stk* stk, u64 line, u64 col, 
	u64 lexemeType, u8 syntax)
{
	struct ba_POpStkItem* stkItem = malloc(sizeof(*stkItem));
	if (!stkItem) {
		ba_ErrorMallocNoMem();
	}
	stkItem->line = line;
	stkItem->col = col;
	stkItem->lexemeType = lexemeType;
	stkItem->syntax = syntax;
	ba_StkPush(stk, (void*)stkItem);
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
	ctr->shortCircLblStk = ba_NewStk();
	ctr->cmpLblStk = ba_NewStk();
	ctr->cmpRegStk = ba_NewStk();
	ctr->cmpRegStk->count = 1;
	ctr->cmpRegStk->items[0] = (void*)0;
	ctr->breakLblStk = ba_NewStk();
	ctr->startIM = ba_NewIM();
	ctr->im = ctr->startIM;
	ctr->entryIM = ctr->startIM;
	ctr->globalST = ba_NewSymTable();
	ctr->currScope = ctr->globalST;
	ctr->labelTable = ba_NewHashTable();
	ctr->usedRegisters = 0;
	ctr->imStackSize = 0;
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

	free(ctr);
}

void ba_AddIM(struct ba_IM** imPtr, u64 count, ...) {
	struct ba_IM* im = *imPtr;
	im->vals = malloc(sizeof(u64) * count);
	if (!im->vals) {
		ba_ErrorMallocNoMem();
	}
	
	va_list vals;
	va_start(vals, count);
	for (u64 i = 0; i < count; i++) {
		im->vals[i] = va_arg(vals, u64);
	}
	va_end(vals);

	im->count = count;
	im->next = ba_NewIM();
	
	//printf("%s\n", ba_IMToStr(im)); // DEBUG

	*imPtr = im->next;
}

enum {
	BA_CTRREG_RAX = 0x1,
	BA_CTRREG_RCX = 0x2,
	BA_CTRREG_RDX = 0x4,
	BA_CTRREG_RSI = 0x8,
	BA_CTRREG_RDI = 0x10,
};

u64 ba_IMToCtrRegister(u64 imReg) {
	switch (imReg) {
		case BA_IM_RAX:
			return BA_CTRREG_RAX;
		case BA_IM_RCX:
			return BA_CTRREG_RCX;
		case BA_IM_RDX:
			return BA_CTRREG_RDX;
		case BA_IM_RSI:
			return BA_CTRREG_RSI;
		case BA_IM_RDI:
			return BA_CTRREG_RDI;
		default:
			return 0;
	}
}

u64 ba_NextIMRegister(struct ba_Controller* ctr) {
	if (!(ctr->usedRegisters & BA_CTRREG_RAX)) {
		ctr->usedRegisters |= BA_CTRREG_RAX;
		return BA_IM_RAX;
	}
	if (!(ctr->usedRegisters & BA_CTRREG_RCX)) {
		ctr->usedRegisters |= BA_CTRREG_RCX;
		return BA_IM_RCX;
	}
	if (!(ctr->usedRegisters & BA_CTRREG_RDX)) {
		ctr->usedRegisters |= BA_CTRREG_RDX;
		return BA_IM_RDX;
	}
	if (!(ctr->usedRegisters & BA_CTRREG_RSI)) {
		ctr->usedRegisters |= BA_CTRREG_RSI;
		return BA_IM_RSI;
	}
	if (!(ctr->usedRegisters & BA_CTRREG_RDI)) {
		ctr->usedRegisters |= BA_CTRREG_RDI;
		return BA_IM_RDI;
	}

	// Stack
	return 0;
}

u64 ba_GetIMRegisterFlag(u64 reg) {
	switch (reg) {
		case BA_IM_RAX:
			return BA_CTRREG_RAX;
		case BA_IM_RCX:
			return BA_CTRREG_RCX;
		case BA_IM_RDX:
			return BA_CTRREG_RDX;
		case BA_IM_RSI:
			return BA_CTRREG_RSI;
		case BA_IM_RDI:
			return BA_CTRREG_RDI;
		default:
			return 0;
	}
}

#endif
