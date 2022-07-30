// See LICENSE for copyright/license information

#ifndef BA__PARSER_EXP_H
#define BA__PARSER_EXP_H

#include "../common/ctr.h"

u8 ba_PAtom(struct ba_Ctr* ctr);
u8 ba_PDerefListMake(struct ba_Ctr* ctr, u64 line, u64 col);
u8 ba_PExp(struct ba_Ctr* ctr);

#endif
