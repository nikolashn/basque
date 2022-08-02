// See LICENSE for copyright/license information

#ifndef BA__COMMON_PARSER_H
#define BA__COMMON_PARSER_H

#include "types.h"
#include "stack.h"
#include "symtable.h"

struct ba_PTkStkItem {
	struct ba_Type typeInfo;
	void* val;
	u64 lexemeType;
	bool isLValue;
	bool isConst;
};

struct ba_POpStkItem {
	u64 line;
	u64 col;
	u64 lexemeType;
	u8 syntax; // i.e. infix, prefix, postfix
};

struct ba_PLabel {
	u64 id;
	struct ba_SymTable* scope;
};

enum {
	BA_OP_PREFIX = 1,
	BA_OP_INFIX = 2,
	BA_OP_POSTFIX = 3,
};

void ba_PTkStkPush(struct ba_Stk* stk, void* val, struct ba_Type type, 
	u64 lexemeType, bool isLValue, bool isConst);
void ba_POpStkPush(struct ba_Stk* stk, u64 line, u64 col, 
	u64 lexemeType, u8 syntax);
void ba_PBreakStkPush(struct ba_Stk* stk, u64 id, struct ba_SymTable* scope);

#endif
