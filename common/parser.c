// See LICENSE for copyright/license information

#include "parser.h"
#include "exitmsg.h"

/* The parser pushes data from read lexemes onto the stack for optimized 
 * calculation and intermediate code generation. Data is stored in ctr->pTkStk 
 * whereas operators are stored in ctr->pOpStk. */

void ba_PTkStkPush(struct ba_Stk* stk, void* val, struct ba_Type type, 
	u64 lexemeType, bool isLValue, bool isConst)
{
	struct ba_PTkStkItem* stkItem = malloc(sizeof(*stkItem));
	if (!stkItem) {
		ba_ErrorMallocNoMem();
	}
	stkItem->val = val;
	stkItem->typeInfo = type;
	stkItem->lexemeType = lexemeType;
	stkItem->isLValue = isLValue;
	stkItem->isConst = isConst;
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
