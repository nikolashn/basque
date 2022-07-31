// See LICENSE for copyright/license information

#ifndef BA__PARSER_STMT_H
#define BA__PARSER_STMT_H

#include "../common/ctr.h"

u8 ba_PStmt(struct ba_Ctr* ctr);
u8 ba_PFuncDef(struct ba_Ctr* ctr, char* funcName, u64 line, u64 col, 
	struct ba_Type retType);
u8 ba_PVarDef(struct ba_Ctr* ctr, char* idName, u64 line, u64 col, 
	struct ba_Type type);
void ba_PStmtWrite(struct ba_Ctr* ctr, u64 len, char* str);
u8 ba_PCommaStmt(struct ba_Ctr* ctr);
u8 ba_PScope(struct ba_Ctr* ctr);

#endif
