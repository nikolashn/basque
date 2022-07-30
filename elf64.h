// See LICENSE for copyright/license information

#ifndef BA__BINARY_H
#define BA__BINARY_H

#include "common/ctr.h"
#include "common/elf.h"

u8 ba_WriteBinary(char* fileName, struct ba_Ctr* ctr);
u8 ba_PessimalInstrSize(struct ba_IM* im);

#endif
