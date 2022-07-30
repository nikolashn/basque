// See LICENSE for copyright/license information

#ifndef BA__COMMON_CTR_H
#define BA__COMMON_CTR_H

#include "exitmsg.h"
#include "stack.h"
#include "func.h"

struct ba_Ctr { // Controller
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
	// For type coercion in array literals
	struct ba_Stk* expCoercedTypeStk; // Takes struct ba_Type* as items

	// Used with return statements
	struct ba_Func* currFunc;

	// Code generation
	struct ba_IM* startIM;
	struct ba_IM* im;
	struct ba_IM* entryIM;
	
	// Parser
	u64 usedRegisters;
	u64 imStackSize;
	struct ba_DynArr8* staticSeg;
	u64 labelCnt;
	bool isPermitArrLit;
	// Counts expression parentheses etc. to make sure they are balanced
	i64 paren;
	i64 bracket;
};

struct ba_Ctr* ba_NewCtr();
void ba_DelCtr(struct ba_Ctr* ctr);
void ba_AddIM(struct ba_Ctr* ctr, u64 count, ...);

#endif
