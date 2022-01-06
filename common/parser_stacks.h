// See LICENSE for copyright/license information

#ifndef BA__PARSER_STACKS_H
#define BA__PARSER_STACKS_H

// The parser pushes data from read lexemes onto the stack for optimized 
// calculation and intermediate code generation. Data is stored in ctr->pTkStk 
// whereas operators are stored in ctr->pOpStk.

struct ba_PTkStkItem {
	void* val;
	u64 type;
	u64 lexemeType;
	bool isLValue;
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

#endif
