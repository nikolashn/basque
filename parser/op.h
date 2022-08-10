// See LICENSE for copyright/license information

#ifndef BA__PARSER_OP_H
#define BA__PARSER_OP_H

#include "../common/ctr.h"
#include "../common/parser.h"

bool ba_POpIsHandler(struct ba_POpStkItem* op);
u8 ba_POpPrecedence(struct ba_POpStkItem* op);
void ba_POpAsgnRegOrStack(struct ba_Ctr* ctr, u64 lexType, u64* reg, 
	u64* stackPos);
void ba_POpMovIdToReg(struct ba_Ctr* ctr, struct ba_STVal* id, u64 argSize, 
	u64 reg, bool isLea);
bool ba_POpMovArgToReg(struct ba_Ctr* ctr, struct ba_PTkStkItem* arg, u64 reg, 
	bool isLiteral);
bool ba_POpMovArgToRegDPTR(struct ba_Ctr* ctr, struct ba_PTkStkItem* arg,
	u64 size, u64 reg, u64 testReg, u64 tmpRegDef, u64 tmpRegBackup);
void ba_POpSetArg(struct ba_Ctr* ctr, struct ba_PTkStkItem* arg, u64 reg, 
	u64 stackPos);
void ba_POpNonLitUnary(u64 opLexType, struct ba_PTkStkItem* arg,
	struct ba_Ctr* ctr);
void ba_POpNonLitBinary(struct ba_Ctr* ctr, u64 imOp, struct ba_PTkStkItem* lhs,
	struct ba_PTkStkItem* rhs, struct ba_PTkStkItem* arg, bool isLhsLiteral, 
	bool isRhsLiteral, bool isShortCirc);
void ba_POpNonLitBitShift(struct ba_Ctr* ctr, u64 imOp, 
	struct ba_PTkStkItem* lhs, struct ba_PTkStkItem* rhs, 
	struct ba_PTkStkItem* arg, bool isRhsLiteral);
u8 ba_POpIsRightAssoc(struct ba_POpStkItem* op);
u8 ba_PCorrectDPtr(struct ba_Ctr* ctr, struct ba_PTkStkItem* item);
u8 ba_POpAssignChecks(struct ba_Ctr* ctr, struct ba_Type lhsType, 
	struct ba_PTkStkItem* rhs, u64 line, u64 col);
void ba_POpFuncCallPushArgReg(struct ba_Ctr* ctr, u64 reg, u64 size);
struct ba_StaticAddr* 
ba_AllocStrLitStatic(struct ba_Ctr* ctr, struct ba_Str* str);
void ba_PAssignArr(struct ba_Ctr* ctr, struct ba_PTkStkItem* destItem, 
	struct ba_PTkStkItem* srcItem, u64 size);
void ba_POpNonLitDivMod(struct ba_Ctr* ctr, struct ba_PTkStkItem* lhs, 
	struct ba_PTkStkItem* rhs, struct ba_PTkStkItem* arg, u64 regL, u64 regR,
	u64 realReg, u64 lhsStackPos, struct ba_Type lhsType, 
	bool isDiv, bool isAssign);

#endif
