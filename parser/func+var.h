// See LICENSE for copyright/license information

#ifndef BA__PARSER_FUNCVAR_H
#define BA__PARSER_FUNCVAR_H

#include "../lexer.h"
#include "../common/parser_stacks.h"

// ----- Forward declarations -----
u8 ba_PAccept(u64 type, struct ba_Controller* ctr);
u8 ba_PExpect(u64 type, struct ba_Controller* ctr);
u8 ba_PBaseType(struct ba_Controller* ctr, bool isInclVoid, bool isInclIndefArr);
u8 ba_PExp(struct ba_Controller* ctr);
u8 ba_PStmt(struct ba_Controller* ctr);
// --------------------------------

u8 ba_PFuncDef(struct ba_Controller* ctr, char* funcName,
	u64 line, u64 col, struct ba_Type retType)
{
	// TODO: once named scopes are added, change this
	if (ctr->currScope != ctr->globalST) {
		return ba_ExitMsg(BA_EXIT_ERR, "func can only be defined in "
			"the outer scope,", line, col, ctr->currPath);
	}

	struct ba_STVal* prevFuncIdVal = ba_HTGet(ctr->currScope->ht, funcName);
	if (prevFuncIdVal && (prevFuncIdVal->type.type != BA_TYPE_FUNC || 
		prevFuncIdVal->isInited))
	{
		return ba_ErrorVarRedef(funcName, line, col, ctr->currPath);
	}

	if (retType.type == BA_TYPE_ARR && 
		!((struct ba_ArrExtraInfo*)retType.extraInfo)->cnt) 
	{
		return ba_ExitMsg(BA_EXIT_ERR, "func return type cannot be indefinite "
			"size array on", line, col, ctr->currPath);
	}

	struct ba_STVal* funcIdVal = malloc(sizeof(struct ba_STVal));
	if (!funcIdVal) {
		return ba_ErrorMallocNoMem();
	}
	ba_HTSet(ctr->currScope->ht, funcName, (void*)funcIdVal);

	funcIdVal->scope = ctr->currScope;
	funcIdVal->type.type = BA_TYPE_FUNC;

	struct ba_Func* func = ba_NewFunc();
	funcIdVal->initVal = func;

	func->retType = retType;
	if (!func->childScope) {
		func->childScope = ba_SymTableAddChild(ctr->currScope);
	}

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

	funcIdVal->isInited = 1;

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
				if (!ba_PBaseType(ctr, /* isInclVoid = */ 0, 
					/* isInclIndefArr = */ 0)) 
				{
					return 0;
				}
				
				++func->paramCnt;
				param->type = *(struct ba_Type*)((struct ba_PTkStkItem*)
					ba_StkPop(ctr->pTkStk))->typeInfo.extraInfo;

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
					if (stmtType == TP_FULLDEC) {
						return 0;
					}
					stmtType = TP_FWDDEC;
					state = ST_PARAM;
					break;
				}

				if (ba_HTGet(func->childScope->ht, paramName)) {
					return ba_ErrorVarRedef(paramName, line, col, ctr->currPath);
				}

				struct ba_STVal* paramVal = malloc(sizeof(struct ba_STVal));
				if (!paramVal) {
					return ba_ErrorMallocNoMem();
				}

				paramVal->scope = func->childScope;
				paramVal->type = param->type;

				u64 paramSize = ba_GetSizeOfType(paramVal->type);
				func->childScope->dataSize += paramSize;
				func->paramStackSize += paramSize;
				paramVal->address = func->childScope->dataSize;

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
					return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", line, col,
						ctr->currPath);
				}
				
				if (!ba_IsLexemeLiteral(expItem->lexemeType)) {
					return ba_ExitMsg(BA_EXIT_ERR, "default func parameter "
						"cannot be resolved at compile time on", line, col,
						ctr->currPath);
				}

				if ((ba_IsTypeNumeric(param->type.type) && 
					!ba_IsTypeNumeric(expItem->typeInfo.type)) ||
					(!ba_IsTypeNumeric(param->type.type) && 
					!ba_AreTypesEqual(param->type, expItem->typeInfo)))
				{
					return ba_ErrorAssignTypes(
						ba_GetTypeStr(expItem->typeInfo), paramName, 
						ba_GetTypeStr(param->type), line, col, ctr->currPath);
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
				goto BA_LBL_FUNCDEF_LOOPEND;
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

	ba_AddIM(ctr, 2, BA_IM_LABEL, func->lblStart);
	// TODO: preserve registers
	func->childScope->dataSize += 8; // For the return location
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);

	if (!stmtType && ba_PAccept(';', ctr)) {
		funcIdVal->isInited = 0;
		stmtType = TP_FWDDEC;
	}
	else if (stmtType == TP_FWDDEC) {
		funcIdVal->isInited = 0;
		ba_PExpect(';', ctr);
	}
	else if (ba_PAccept(',', ctr)) {
		stmtType = TP_FULLDEC;
		if (!ba_PStmt(ctr)) {
			return 0;
		}
	}
	else if (ba_PAccept('{', ctr)) {
		stmtType = TP_FULLDEC;
		while (ba_PStmt(ctr));
		ba_PExpect('}', ctr);
	}
	else {
		return 0;
	}

	// Error for mismatch with forward declaration types
	if (prevFuncIdVal) {
		struct ba_Func* prevFunc = prevFuncIdVal->initVal;
		struct ba_FuncParam* fwdDecParam = prevFunc->firstParam;
		param = func->firstParam;
		while (fwdDecParam && param) {
			if (!ba_AreTypesEqual(fwdDecParam->type, param->type)) {
				break;
			}
			fwdDecParam = fwdDecParam->next;
			param = param->next;
		}
		if (fwdDecParam || param) {
			fprintf(stderr, "Error: parameters of func %s declared on line "
				"%llu:%llu in %s incompatible with previously forward declared "
				"definition\n", funcName, line, col, ctr->currPath);
			exit(-1);
		}
		ba_DelFunc(prevFunc);
		free(prevFuncIdVal);
	}
	
	ba_AddIM(ctr, 2, BA_IM_LABEL, func->lblEnd);
	// TODO: restore registers
	// Fix stack
	if (func->childScope->dataSize - func->paramStackSize - 8) {
		ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_RSP, BA_IM_IMM, 
			func->childScope->dataSize - func->paramStackSize - 8);
	}
	ba_AddIM(ctr, 1, BA_IM_RET);

	func->imEnd = ctr->im;
	ctr->im = oldIM;
	ctr->currFunc = 0;
	ctr->currScope = func->childScope->parent;

	if (stmtType == TP_FULLDEC && retType.type != BA_TYPE_VOID && 
		!func->doesReturn) 
	{
		fprintf(stderr, "Error: func '%s' (defined on line %llu:%llu in %s "
			"with return type %s) does not return a value\n", funcName, line, 
			col, ctr->currPath, ba_GetTypeStr(retType));
		exit(-1);
	}

	return 1;
}

u8 ba_PVarDef(struct ba_Controller* ctr, char* idName, 
	u64 line, u64 col, struct ba_Type type)
{
	if (ba_HTGet(ctr->currScope->ht, idName)) {
		return ba_ErrorVarRedef(idName, line, col, ctr->currPath);
	}

	struct ba_SymTable* foundIn = 0;
	if (ba_STParentFind(ctr->currScope, &foundIn, idName)) {
		if (!ba_IsSilenceWarnings) {
			fprintf(stderr, "Warning: shadowing variable '%s' on "
				"line %llu:%llu in %s\n", idName, line, col, ctr->currPath);
		}
		if (ba_IsWarningsAsErrors) {
			exit(-1);
		}
	}

	struct ba_STVal* idVal = malloc(sizeof(struct ba_STVal));
	if (!idVal) {
		return ba_ErrorMallocNoMem();
	}

	idVal->scope = ctr->currScope;
	idVal->type = type;

	if (idVal->type.type == BA_TYPE_VOID) {
		return ba_ErrorVarVoid(line, col, ctr->currPath);
	}
	
	u64 dataSize = ba_GetSizeOfType(idVal->type);
	idVal->address = ctr->currScope->dataSize + dataSize;

	ba_HTSet(ctr->currScope->ht, idName, (void*)idVal);

	line = ctr->lex->line;
	col = ctr->lex->colStart;
	
	if (!ba_PAccept('=', ctr)) {
		if (ba_IsTypeNumeric(idVal->type.type)) {
			if (dataSize == 8) {
				ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
			}
			else if (dataSize == 1) {
				ba_AddIM(ctr, 2, BA_IM_DEC, BA_IM_RSP);
			}
			else {
				ba_AddIM(ctr, 3, BA_IM_SUB, BA_IM_RSP, dataSize);
			}
		}
		else if (idVal->type.type == BA_TYPE_ARR) {
			if (!((struct ba_ArrExtraInfo*)idVal->type.extraInfo)->cnt) {
				return ba_ExitMsg(BA_EXIT_ERR, "uninitialized array with "
					"undefined size on", line, col, ctr->currPath);
			}
			ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RSP, BA_IM_IMM, dataSize);
		}
		idVal->isInited = 0;
		ctr->currScope->dataSize += dataSize;
		return ba_PExpect(';', ctr);
	}

	if (!ba_PExp(ctr)) {
		return 0;
	}
	
	struct ba_PTkStkItem* expItem = ba_StkPop(ctr->pTkStk);
	ba_POpAssignChecks(ctr, idVal->type, expItem, line, col);
	
	if (ba_IsTypeNumeric(idVal->type.type)) {
		if (!ba_IsTypeNumeric(expItem->typeInfo.type)) {
			char* expTypeStr = ba_GetTypeStr(expItem->typeInfo);
			char* varTypeStr = ba_GetTypeStr(idVal->type);
			return ba_ErrorAssignTypes(expTypeStr, idName, 
				varTypeStr, line, col, ctr->currPath);
		}

		if (expItem->lexemeType == BA_TK_IDENTIFIER) {
			ba_AddIM(ctr, 5, BA_IM_MOV, ba_AdjRegSize(BA_IM_RAX, dataSize), 
				BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
				ba_CalcSTValOffset(ctr->currScope, expItem->val));
		}

		if (ba_IsLexemeLiteral(expItem->lexemeType)) {
			ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, (u64)expItem->val);
		}
		
		u64 reg = expItem->lexemeType == BA_TK_IMREGISTER 
			? (u64)expItem->val : BA_IM_RAX;

		if (dataSize == 8) {
			ba_AddIM(ctr, 2, BA_IM_PUSH, reg);
		}
		else {
			if (dataSize == 1) {
				ba_AddIM(ctr, 2, BA_IM_DEC, BA_IM_RSP);
			}
			else {
				ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RSP, BA_IM_IMM, dataSize);
			}
			ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RSP, 
				ba_AdjRegSize(reg, dataSize));
		}
	}
	else if (idVal->type.type == BA_TYPE_ARR) {
		ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RSP, BA_IM_IMM, dataSize);
		// TODO
		// remember, for uninitialized arrays, their size needs to be set here,
		// changing their address, and also the scope's data size
	}
	
	ctr->currScope->dataSize += dataSize;
	return ba_PExpect(';', ctr);
}

#endif
