// See LICENSE for copyright/license information

#ifndef BA__PARSER_FUNCVAR_H
#define BA__PARSER_FUNCVAR_H

#include "../lexer.h"

// ----- Forward declarations -----
u8 ba_PAccept(u64 type, struct ba_Controller* ctr);
u8 ba_PExpect(u64 type, struct ba_Controller* ctr);
u8 ba_PBaseType(struct ba_Controller* ctr);
u8 ba_PExp(struct ba_Controller* ctr);
u8 ba_PCommaStmt(struct ba_Controller* ctr);
u8 ba_PScope(struct ba_Controller* ctr);
// --------------------------------

u8 ba_PFuncDef(struct ba_Controller* ctr, char* funcName,
	u64 line, u64 col, u64 retType)
{
	// TODO: once named scopes are added, change this
	if (ctr->currScope != ctr->globalST) {
		return ba_ExitMsg(BA_EXIT_ERR, "func can only be defined in "
			"the outer scope,", line, col);
	}

	struct ba_STVal* funcIdVal = ba_HTGet(ctr->currScope->ht, funcName);
	if (funcIdVal && (funcIdVal->type != BA_TYPE_FUNC || funcIdVal->isInited)) {
		return ba_ErrorVarRedef(funcName, line, col);
	}

	// TODO: error for mismatch with forward declaration types
	
	!funcIdVal && (funcIdVal = malloc(sizeof(struct ba_STVal)));
	if (!funcIdVal) {
		return ba_ErrorMallocNoMem();
	}
	ba_HTSet(ctr->currScope->ht, funcName, (void*)funcIdVal);

	funcIdVal->scope = ctr->currScope;
	funcIdVal->type = BA_TYPE_FUNC;

	struct ba_Func* func = ba_NewFunc();
	funcIdVal->initVal = func;

	func->retType = retType;
	func->childScope = ba_SymTableAddChild(ctr->currScope);

	struct ba_FuncParam* param = ba_NewFuncParam();
	char* paramName = 0;
	func->firstParam = param;

	ctr->currScope = func->childScope;
	ctr->currFunc = func;

	struct ba_IM* oldIM = ctr->im;
	ctr->im = func->imBegin;

	enum {
		TP_NONE = 0,
		TP_FWDDEC, // Forward declaration
		TP_FULLDEC
	}
	stmtType = 0;

	enum {
		ST_NONE = 0,
		ST_RPAREN,
		ST_PARAM,
		ST_COMMA,
		ST_DEFAULTVAL
	}
	state = 0;

	while (state != ST_RPAREN) {
		switch (state) {
			case ST_RPAREN: return 0;

			case 0:
			{
				// ... ")" ...
				if (ba_PAccept(')', ctr)) {
					state = ST_RPAREN;
					break;
				}
				// Fallthrough into ST_COMMA
			}

			case ST_COMMA:
			{
				// ... base_type ...
				if (!ba_PBaseType(ctr)) {
					return 0;
				}
				
				++func->paramCnt;
				param->type = ba_GetTypeFromKeyword(
					((struct ba_PTkStkItem*)ba_StkPop(ctr->pTkStk))->lexemeType);

				if (ctr->lex->val) {
					paramName = malloc(ctr->lex->valLen+1);
					if (!paramName) {
						return ba_ErrorMallocNoMem();
					}
					strcpy(paramName, ctr->lex->val);
				}

				line = ctr->lex->line;
				col = ctr->lex->colStart;

				// ... identifier ...
				if (!ba_PAccept(BA_TK_IDENTIFIER, ctr)) {
					stmtType = TP_FWDDEC;
					state = ST_PARAM;
					break;
				}

				stmtType = TP_FULLDEC;
				
				if (ba_HTGet(func->childScope->ht, paramName)) {
					return ba_ErrorVarRedef(paramName, line, col);
				}

				struct ba_STVal* paramVal = malloc(sizeof(struct ba_STVal));
				if (!paramVal) {
					return ba_ErrorMallocNoMem();
				}

				paramVal->scope = func->childScope;
				paramVal->type = param->type;

				u64 paramSize = ba_GetSizeOfType(paramVal->type);
				func->childScope->dataSize += paramSize;
				paramVal->address = func->childScope->dataSize;

				paramVal->isInited = 1;
				
				ba_HTSet(func->childScope->ht, paramName, (void*)paramVal);

				state = ST_PARAM;
			}

			case ST_PARAM:
			{
				// ... ")" ...
				if (ba_PAccept(')', ctr)) {
					state = ST_RPAREN;
					break;
				}

				// ... "," ...
				if (ba_PAccept(',', ctr)) {
					state = ST_COMMA;
					goto BA_LBL_PFUNCDEF_ENDPARAM;
				}

				// ... "=" exp ...
				line = ctr->lex->line;
				col = ctr->lex->colStart;
				if (stmtType == TP_FWDDEC || !ba_PExpect('=', ctr) ||
					!ba_PExp(ctr))
				{
					return 0;
				}
				
				struct ba_PTkStkItem* expItem = ba_StkPop(ctr->pTkStk);
				if (!expItem) {
					return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", line, col);
				}
				
				if (!ba_IsLexemeLiteral(expItem->lexemeType)) {
					return ba_ExitMsg(BA_EXIT_ERR, "default func parameter "
						"cannot be resolved at compile time on", line, col);
				}

				if ((ba_IsTypeNumeric(param->type) && 
					!ba_IsTypeNumeric(expItem->type)) ||
					(!ba_IsTypeNumeric(param->type) && 
					param->type != expItem->type))
				{
					return ba_ErrorAssignTypes(ba_GetTypeStr(expItem->type), 
						paramName, ba_GetTypeStr(param->type), line, col);
				}

				param->defaultVal = expItem->val;
				param->hasDefaultVal = 1;
				state = ST_DEFAULTVAL;
				goto BA_LBL_PFUNCDEF_ENDPARAM;
			}

			case ST_DEFAULTVAL:
			{
				// ... ")" ...
				if (ba_PAccept(')', ctr)) {
					state = ST_RPAREN;
					break;
				}
				// ... "," ...
				if (!ba_PExpect(',', ctr)) {
					return 0;
				}
				state = ST_COMMA;
				param->next = ba_NewFuncParam();
				param = param->next;
				goto BA_LBL_PFUNCDEF_ENDPARAM;
			}
		}

		goto BA_LBL_FUNCDEF_LOOPEND;

		BA_LBL_PFUNCDEF_ENDPARAM:
		param->next = ba_NewFuncParam();
		param = param->next;

		BA_LBL_FUNCDEF_LOOPEND:
	}

	func->lblStart = ctr->labelCnt++;
	func->lblEnd = ctr->labelCnt++;
	ba_AddIM(&ctr->im, 2, BA_IM_LABEL, func->lblStart);
	// Store return location in RBX
	ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RBX);
	// TODO: preserve registers
	ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);

	if (!stmtType && ba_PAccept(';', ctr)) {
		stmtType = TP_FWDDEC;
	}
	else if (stmtType == TP_FWDDEC) {
		ba_PExpect(';', ctr);
	}
	else if (ba_PCommaStmt(ctr) || ba_PScope(ctr)) {
		stmtType = TP_FULLDEC;
		funcIdVal->isInited = 1;
	}
	else {
		return 0;
	}

	ba_AddIM(&ctr->im, 2, BA_IM_LABEL, func->lblEnd);
	// Fix stack
	if (func->childScope->dataSize) {
		ba_AddIM(&ctr->im, 4, BA_IM_ADD, BA_IM_RSP, 
			BA_IM_IMM, func->childScope->dataSize);
	}
	// TODO: restore registers
	// Push return location
	ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RBX);
	ba_AddIM(&ctr->im, 1, BA_IM_RET);

	func->imEnd = ctr->im;
	ctr->im = oldIM;
	ctr->currFunc = 0;
	ctr->currScope = func->childScope->parent;
	return 1;
}

u8 ba_PVarDef(struct ba_Controller* ctr, char* idName, 
	u64 line, u64 col, u64 type)
{
	if (ba_HTGet(ctr->currScope->ht, idName)) {
		return ba_ErrorVarRedef(idName, line, col);
	}

	struct ba_STVal* idVal = malloc(sizeof(struct ba_STVal));
	if (!idVal) {
		return ba_ErrorMallocNoMem();
	}

	idVal->scope = ctr->currScope;
	idVal->type = type;

	if (idVal->type == BA_TYPE_VOID) {
		return ba_ErrorVarVoid(line, col);
	}
	
	u64 idDataSize = ba_GetSizeOfType(idVal->type);
	/* For global identifiers, the address of the start is used 
	 * For non global identifiers, the address of the end is used */
	idVal->address = ctr->currScope->dataSize + 
		(ctr->currScope != ctr->globalST) * idDataSize;

	idVal->initVal = 0;
	idVal->isInited = 0;

	ba_HTSet(ctr->currScope->ht, idName, (void*)idVal);

	if (ba_PAccept('=', ctr)) {
		idVal->isInited = 1;

		line = ctr->lex->line;
		col = ctr->lex->colStart;
		
		if (!ba_PExp(ctr)) {
			return 0;
		}
		struct ba_PTkStkItem* expItem = ba_StkPop(ctr->pTkStk);
		
		if (ba_IsTypeNumeric(idVal->type)) {
			if (!ba_IsTypeNumeric(expItem->type)) {
				char* expTypeStr = ba_GetTypeStr(expItem->type);
				char* varTypeStr = ba_GetTypeStr(idVal->type);
				return ba_ErrorAssignTypes(expTypeStr, idName, 
					varTypeStr, line, col);
			}

			if (expItem->lexemeType == BA_TK_GLOBALID) {
				ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_DATASGMT, 
					((struct ba_STVal*)expItem->val)->address);
			}
			else if (expItem->lexemeType == BA_TK_LOCALID) {
				ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_RAX, 
					BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
					ba_CalcSTValOffset(ctr->currScope, expItem->val));
			}

			if (ctr->currScope == ctr->globalST) {
				if (ba_IsLexemeLiteral(expItem->lexemeType)) {
					idVal->initVal = expItem->val;
				}
				else {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_DATASGMT, 
						idVal->address, 
						expItem->lexemeType == BA_TK_IMREGISTER ? 
							(u64)expItem->val : BA_IM_RAX);
				}
			}
			else {
				if (ba_IsLexemeLiteral(expItem->lexemeType)) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, 
						BA_IM_IMM, (u64)expItem->val);
				}
				ba_AddIM(&ctr->im, 2, BA_IM_PUSH,
					expItem->lexemeType == BA_TK_IMREGISTER ? 
						(u64)expItem->val : BA_IM_RAX);
			}
		}
	}
	
	ctr->currScope->dataSize += idDataSize;

	if (ctr->currScope != ctr->globalST && !idVal->isInited) {
		ba_AddIM(&ctr->im, 3, BA_IM_PUSH, BA_IM_IMM, 0);
	}

	return ba_PExpect(';', ctr);
}

#endif
