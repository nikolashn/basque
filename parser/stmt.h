// See LICENSE for copyright/license information

#ifndef BA__PARSER_STMT_H
#define BA__PARSER_STMT_H

#include "../common/ctr.h"
#include "../common/symtable.h"

u8 ba_PStmt(struct ba_Ctr* ctr);
u8 ba_PScope(struct ba_Ctr* ctr, struct ba_SymTable* scope);

#endif
