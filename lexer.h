// See LICENSE for copyright/license information

#ifndef BA__LEXER_H
#define BA__LEXER_H

#include "common/common.h"
#include "common/lexeme.h"
#include "common/ctr.h"

u8 ba_Tokenize(FILE* srcFile, struct ba_Ctr* ctr);

#endif
