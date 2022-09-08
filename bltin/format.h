// See LICENSE for copyright/license information

#ifndef BA__BLTIN_FORMAT_H
#define BA__BLTIN_FORMAT_H

#include "../common/ctr.h"

void ba_BltinU64ToStr(struct ba_Ctr* ctr);
void ba_BltinI64ToStr(struct ba_Ctr* ctr);
void ba_BltinHexToStr(struct ba_Ctr* ctr);
void ba_BltinOctToStr(struct ba_Ctr* ctr);
void ba_BltinBinToStr(struct ba_Ctr* ctr);

#endif
