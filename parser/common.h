// See LICENSE for copyright/license information

#ifndef BA__PARSER_COMMON_H
#define BA__PARSER_COMMON_H

#include "parse.h"
#include "stmt.h"
#include "exp.h"
#include "op.h"
#include "handle.h"
#include "../lexer.h"
#include "../common/dynarr.h"
#include "../common/lexeme.h"
#include "../common/parser.h"
#include "../common/reg.h"
#include "../common/format.h"
#include "../bltin/bltin.h"

u8 ba_PAccept(u64 type, struct ba_Ctr* ctr);
u8 ba_PExpect(u64 type, struct ba_Ctr* ctr);
u8 ba_PBaseType(struct ba_Ctr* ctr, bool isInclVoid, bool isInclIndefArr);

#endif
