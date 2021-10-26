// See LICENSE for copyright/license information

#ifndef BA__CTR_H
#define BA__CTR_H

#include "stack.h"

struct ba_Controller {
	// Lexer
	struct ba_Lexeme* startLex;
	struct ba_Lexeme* lex;

	// Parser
	struct ba_Stk* pTkStk; // Takes ba_PTkStkItem as items
	struct ba_Stk* pOpStk; // Takes ba_POpStkItem as items
	struct ba_SymTable* globalST;
	struct ba_SymTable* currScope;

	// Code generation
	struct ba_IM* startIM;
	struct ba_IM* im;
	struct ba_IM* entryIM;
	u64 usedRegisters;
	u64 imStackCnt;
	u64 labelCnt;
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
	ctr->entryIM = ctr->startIM;
	ctr->globalST = ba_NewSymTable();
	ctr->currScope = ctr->globalST;
	ctr->usedRegisters = 0;
	ctr->imStackCnt = 0;
	ctr->labelCnt = 0;
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
	BA_CTR_RAX = 0x1,
	BA_CTR_RCX = 0x2,
	BA_CTR_RDX = 0x4,
	BA_CTR_RSI = 0x8,
	BA_CTR_RDI = 0x10,
};

u64 ba_NextIMRegister(struct ba_Controller* ctr) {
	if (!(ctr->usedRegisters & BA_CTR_RAX)) {
		ctr->usedRegisters |= BA_CTR_RAX;
		return BA_IM_RAX;
	}
	if (!(ctr->usedRegisters & BA_CTR_RCX)) {
		ctr->usedRegisters |= BA_CTR_RCX;
		return BA_IM_RCX;
	}
	if (!(ctr->usedRegisters & BA_CTR_RDX)) {
		ctr->usedRegisters |= BA_CTR_RDX;
		return BA_IM_RDX;
	}
	if (!(ctr->usedRegisters & BA_CTR_RSI)) {
		ctr->usedRegisters |= BA_CTR_RSI;
		return BA_IM_RSI;
	}
	if (!(ctr->usedRegisters & BA_CTR_RDI)) {
		ctr->usedRegisters |= BA_CTR_RDI;
		return BA_IM_RDI;
	}

	// Stack
	++ctr->imStackCnt;
	return 0;
}

void ba_PrevIMRegister(struct ba_Controller* ctr) {
	if (ctr->imStackCnt) {
		--ctr->imStackCnt;
	}
	else if (ctr->usedRegisters & BA_CTR_RDI) {
		ctr->usedRegisters &= ~BA_CTR_RDI;
	}
	else if (ctr->usedRegisters & BA_CTR_RSI) {
		ctr->usedRegisters &= ~BA_CTR_RSI;
	}
	else if (ctr->usedRegisters & BA_CTR_RDX) {
		ctr->usedRegisters &= ~BA_CTR_RDX;
	}
	else if (ctr->usedRegisters & BA_CTR_RCX) {
		ctr->usedRegisters &= ~BA_CTR_RCX;
	}
	else if (ctr->usedRegisters & BA_CTR_RAX) {
		ctr->usedRegisters &= ~BA_CTR_RAX;
	}
}

#endif
