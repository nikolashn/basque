// See LICENSE for copyright/license information

#include "common.h"
#include "../common/types.h"

/* atom = lit_str { lit_str } | lit_int | lit_char | identifier 
 *      | "{" exp { "," exp } [ "," ] "}" */
u8 ba_PAtom(struct ba_Ctr* ctr) {
	u64 lexLine = ctr->lex->line;
	u64 lexColStart = ctr->lex->colStart;
	u64 lexValLen = ctr->lex->valLen;
	char* lexVal = 0;
	if (ctr->lex->val) {
		lexVal = ba_MAlloc(lexValLen+1);
		strcpy(lexVal, ctr->lex->val);
	}

	// lit_str { lit_str }
	if (ba_PAccept(BA_TK_LITSTR, ctr)) {
		u64 len = lexValLen;
		char* chars = ba_MAlloc(len + 1);
		strcpy(chars, lexVal);
		
		// do-while prevents 1 more str literal from being consumed than needed
		do {
			if (ctr->lex->type != BA_TK_LITSTR) {
				break;
			}
			u64 oldLen = len;
			len += ctr->lex->valLen;
			
			if (len > BA_STACK_SIZE) {
				return ba_ExitMsg2(BA_EXIT_ERR, "string at", ctr->lex->line, 
					ctr->lex->colStart, ctr->currPath, 
					" too large to fit on the stack");
			}

			chars = ba_Realloc(chars, len+1);
			strcpy(chars+oldLen, ctr->lex->val);
			chars[len] = 0;
		}
		while (ba_PAccept(BA_TK_LITSTR, ctr));

		struct ba_Str* str = ba_MAlloc(sizeof(*str));
		*str = (struct ba_Str){ .str = chars, .len = len, .staticAddr = 0 };
		str->str = chars;
		str->len = len;

		struct ba_ArrExtraInfo* extraInfo = ba_MAlloc(sizeof(*extraInfo));
		extraInfo->type = (struct ba_Type){ BA_TYPE_U8, 0 };
		extraInfo->cnt = len + 1;
		struct ba_Type strType = { BA_TYPE_ARR, extraInfo };
		
		ba_PTkStkPush(ctr->pTkStk, (void*)str, strType, 
			BA_TK_LITSTR, /* isLValue = */ 0, /* isConst = */ 1);
	}
	// lit_int
	else if (ba_PAccept(BA_TK_LITINT, ctr)) {
		u64 num;
		struct ba_Type type = { BA_TYPE_U64, 0 };
		if (lexVal[lexValLen-1] == 'u') {
			// ba_StrToU64 cannot parse the 'u' suffix so it must be removed
			lexVal[--lexValLen] = 0;
			num = ba_StrToU64(lexVal, lexLine, lexColStart, ctr->currPath);
		}
		else {
			num = ba_StrToU64(lexVal, lexLine, lexColStart, ctr->currPath);
			if (num < (1llu << 63)) {
				type = (struct ba_Type){ BA_TYPE_I64, 0 };
			}
		}
		ba_PTkStkPush(ctr->pTkStk, (void*)num, type, BA_TK_LITINT, 
			/* isLValue = */ 0, /* isConst = */ 1);
	}
	// lit_char
	else if (ba_PAccept(BA_TK_LITCHAR, ctr)) {
		struct ba_Type type = { BA_TYPE_U8, 0 };
		ba_PTkStkPush(ctr->pTkStk, (void*)(u64)lexVal[0], type, BA_TK_LITCHAR,
			/* isLValue = */ 0, /* isConst = */ 1);
	}
	// identifier
	else if (ba_PAccept(BA_TK_IDENTIFIER, ctr)) {
		struct ba_SymTable* stFoundIn = 0;
		struct ba_STVal* id = ba_STParentFind(ctr->currScope, &stFoundIn, lexVal);
		if (!id) {
			return ba_ErrorIdUndef(lexVal, lexLine, lexColStart, ctr->currPath);
		}
		ba_PTkStkPush(ctr->pTkStk, (void*)id, id->type, BA_TK_IDENTIFIER, 
			/* isLValue = */ 1, /* isConst = */ 0);
	}
	// "{" exp { "," exp } [ "," ] "}"
	else if (ba_PAccept('{', ctr)) {
		// TODO: add nesting arrays
		!ctr->isPermitArrLit &&
			ba_ExitMsg(BA_EXIT_ERR, "used array literal in context where it is "
				"not permitted", lexLine, lexColStart, ctr->currPath);
		
		struct ba_Type* coercedType = ba_StkTop(ctr->expCoercedTypeStk);
		
		if (coercedType->type != BA_TYPE_ARR) {
			fprintf(stderr, "Error: cannot coerce array literal to type '%s' "
				"on line %llu:%llu in %s\n", ba_GetTypeStr(*coercedType), 
				lexLine, lexColStart, ctr->currPath);
			exit(-1);
		}
	
		struct ba_ArrExtraInfo* extraInfo = coercedType->extraInfo;
		u64 fundamentalSz = ba_GetSizeOfType(extraInfo->type);
		bool isArrNum = ba_IsTypeNum(extraInfo->type);

		u64 cnt = 0;
		bool isConst = 1;
		
		struct ba_Static* statObj = ba_MAlloc(sizeof(*statObj));
		*statObj = (struct ba_Static)
			{ .arr = ba_NewDynArr8(0x1000), .offset = 0, .isUsed = 1 };
		bool hasTruncationWarning = 0;

		// Enter a new stack so that operators are not handled improperly
		struct ba_Stk* oldPOpStk = ctr->pOpStk;
		ctr->pOpStk = ba_NewStk();
		i64 oldParen = ctr->paren;
		ctr->paren = 0;
		i64 oldBracket = ctr->bracket;
		ctr->bracket = 0;

		struct ba_PTkStkItem* stkItem = 0;
		while (ba_PExp(ctr)) {
			stkItem = ba_StkPop(ctr->pTkStk);
			u64 itemSz = ba_GetSizeOfType(stkItem->typeInfo);

			if ((extraInfo->type.type == BA_TYPE_ARR &&
					(stkItem->typeInfo.type != BA_TYPE_ARR || 
						fundamentalSz != itemSz)) || 
				(isArrNum && !ba_IsTypeNum(stkItem->typeInfo)) ||
				 (!isArrNum && 
				  !ba_AreTypesEqual(extraInfo->type, stkItem->typeInfo)))
			{
				ba_ErrorConvertTypes(lexLine, lexColStart, ctr->currPath, 
					stkItem->typeInfo, extraInfo->type);
			}

			if (hasTruncationWarning) {
				goto BA_LBL_PATOM_ARRLIT_LOOPEND;
			}

			if (extraInfo->cnt && cnt > extraInfo->cnt) {
				ba_ExitMsg2(BA_EXIT_WARN, "number of elements in array literal "
					"is greater than of expected type on", lexLine, lexColStart,
					ctr->currPath, 
					ba_IsWarnsAsErrs() ? "" : ", array literal truncated");
				hasTruncationWarning = 1;
			}

			u64 addr = statObj->arr->cnt;
			u8* memStart = statObj->arr->arr + addr;
			
			++cnt;
			statObj->arr->cnt += itemSz;
			(statObj->arr->cnt > statObj->arr->cap) && ba_ResizeDynArr8(statObj->arr);
			
			if (stkItem->isConst) {
				if (stkItem->lexemeType == BA_TK_LITINT) {
					memcpy(memStart, &stkItem->val, itemSz);
				}
				else if (stkItem->lexemeType == BA_TK_LITSTR) {
					struct ba_Str* str = stkItem->val;
					str->staticAddr = ba_MAlloc(sizeof(*str->staticAddr));
					*str->staticAddr = (struct ba_StaticAddr){ statObj, addr };
					memcpy(memStart, str->str, str->len + 1);
				}
				else if (stkItem->lexemeType == BA_TK_IMSTATIC) {
					// TODO: nesting arrays
					exit(-1);
				}
			}
			else {
				// TODO: handle types that cannot be put in a register
				isConst = 0;
				memset(memStart, 0, itemSz);
				if (stkItem->lexemeType != BA_TK_IMREGISTER || 
					(u64)stkItem->val != BA_IM_RAX) 
				{
					ba_POpMovArgToReg(ctr, stkItem, BA_IM_RAX, /* isLiteral = */ 0);
				}
				struct ba_StaticAddr* staticAddr = ba_MAlloc(sizeof(*staticAddr));
				*staticAddr = (struct ba_StaticAddr){ statObj, addr };
				ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RCX, BA_IM_STATIC, (u64)staticAddr);
				ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RCX, BA_IM_RAX);
			}
			
			BA_LBL_PATOM_ARRLIT_LOOPEND:
			ctr->isPermitArrLit = 1; // Array literals can nest
			if (!ba_PAccept(',', ctr)) {
				break;
			}
		}
		
		++ctr->statics->cnt;
		(ctr->statics->cnt > ctr->statics->cap) && ba_ResizeDynArr64(ctr->statics);
		ctr->statics->arr[ctr->statics->cnt-1] = (u64)statObj;

		// Restore original state
		ba_DelStk(ctr->pOpStk);
		ctr->pOpStk = oldPOpStk;
		ctr->paren = oldParen;
		ctr->bracket = oldBracket;

		!extraInfo->cnt && (extraInfo->cnt = cnt); // Indefinite arrays
		if (!stkItem) {
			return ba_ExitMsg(BA_EXIT_ERR, "expected expression on", lexLine, 
				lexColStart, ctr->currPath);
		}
		ba_PExpect('}', ctr);

		struct ba_StaticAddr* staticAddr = ba_MAlloc(sizeof(*staticAddr));
		*staticAddr = (struct ba_StaticAddr){ statObj, 0 };
		ba_PTkStkPush(ctr->pTkStk, staticAddr, *coercedType, BA_TK_IMSTATIC, 
				/* isLValue = */ 0, isConst);
	}
	// Other
	else {
		return 0;
	}

	free(lexVal);
	return 1;
}

// Dereferencing list early handling
u8 ba_PDerefListMake(struct ba_Ctr* ctr, u64 line, u64 col) {
	struct ba_Type type = {0};
	struct ba_Stk* originalOpStk = ctr->pOpStk;

	u64 derefArgsCnt = 0;
	u64 deStackPos = 0;
	u64 deReg = 0;

	bool isArrLeaOfst = 0;
	i64 arrLeaOfst = 0;

	while (1) {
		ctr->pOpStk = ba_NewStk();
		if (!ba_PExp(ctr)) {
			return 0;
		}
		ba_DelStk(ctr->pOpStk);
		
		++derefArgsCnt;
		struct ba_PTkStkItem* item = ba_StkPop(ctr->pTkStk);

		bool isLastArg = !ba_PAccept(',', ctr);

		if (derefArgsCnt == 1) {
			type = item->typeInfo;

			if (type.type != BA_TYPE_PTR && type.type != BA_TYPE_ARR) {
				return ba_ErrorDerefNonPtr(line, col, ctr->currPath);
			}

			deReg = (u64)item->val;
			ba_POpAsgnRegOrStack(ctr, item->lexemeType, &deReg, &deStackPos);

			ba_POpMovArgToReg(ctr, item, deReg ? deReg : BA_IM_RAX, 
				ba_IsLexemeLiteral(item->lexemeType));
		}
		else {
			if (derefArgsCnt > 2) {
				type = type.type == BA_TYPE_ARR
					? ((struct ba_ArrExtraInfo*)type.extraInfo)->type
					: *(struct ba_Type*)type.extraInfo;
			}

			if (type.type != BA_TYPE_PTR && type.type != BA_TYPE_ARR) {
				return ba_ErrorDerefNonPtr(line, col, ctr->currPath);
			}

			u64 pntdSz = ba_GetSizeOfType(*(struct ba_Type*)type.extraInfo);

			if (ba_IsLexemeLiteral(item->lexemeType)) {
				// Handle literal array indices by simple multiplication
				if (type.type == BA_TYPE_ARR) {
					arrLeaOfst += (i64)item->val * ba_GetSizeOfType(
						((struct ba_ArrExtraInfo*)type.extraInfo)->type);
					isArrLeaOfst = 1;
				}
				// Literal dereferencing list items for pointers use lea/mov
				else {
					bool isNeg = (i64)item->val < 0;
					u64 ofst = pntdSz * 
						(isNeg ? -(u64)item->val : (u64)item->val);
					ba_AddIM(ctr, 5, isLastArg ? BA_IM_LEA : BA_IM_MOV, 
						isLastArg ? deReg : ba_AdjRegSize(deReg, pntdSz),
						isNeg ? BA_IM_ADRSUB : BA_IM_ADRADD, deReg, ofst);
				}
				goto BA_LBL_PDEREFMAKE_LOOPEND;
			}

			// Non-literal dereferencing list item

			u64 addReg = item->lexemeType == BA_TK_IMREGISTER
				? (u64)item->val
				: ba_NextIMRegister(ctr);

			u64 effAddReg = addReg;
			if (!addReg) {
				// Only preserve rcx/rdx
				effAddReg = deReg == BA_IM_RCX ? BA_IM_RDX : BA_IM_RCX;
				ba_AddIM(ctr, 2, BA_IM_PUSH, effAddReg);
				ctr->imStackSize += 8;
			}

			ba_POpMovArgToReg(ctr, item, effAddReg, /* isLiteral = */ 0);

			if (type.type == BA_TYPE_ARR) {
				u64 tmpReg = ba_NextIMRegister(ctr);
				if (!tmpReg) {
					ba_AddIM(ctr, 2, BA_IM_PUSH, deReg);
				}

				ba_AddIM(ctr, 4, BA_IM_MOV, tmpReg ? tmpReg : deReg, 
					BA_IM_IMM, pntdSz);
				// effAddReg can't be RSP, but if it was there would be an issue
				ba_AddIM(ctr, 3, BA_IM_IMUL, effAddReg, tmpReg ? tmpReg : deReg);

				if (!tmpReg) {
					ba_AddIM(ctr, 2, BA_IM_POP, deReg);
				}

				ba_AddIM(ctr, 6, BA_IM_LEA, deReg, BA_IM_ADRADDREGMUL,
					deReg, 1, effAddReg);

				tmpReg && (ctr->usedRegisters &= ~ba_IMToCtrReg(tmpReg));
			}
			else if (pntdSz == 1 || pntdSz == 2 || pntdSz == 4 || pntdSz == 8) {
				ba_AddIM(ctr, 6, isLastArg ? BA_IM_LEA : BA_IM_MOV,
					isLastArg ? deReg : ba_AdjRegSize(deReg, pntdSz),
					BA_IM_ADRADDREGMUL, deReg, pntdSz, effAddReg);
			}
			else {
				fprintf(stderr, "Error: Dereferencing non-arrays with "
					"nonstandard pntdSz not implemented yet\n");
			}

			if (!addReg) {
				ba_AddIM(ctr, 2, BA_IM_POP, effAddReg);
				ctr->imStackSize -= 8;
			}
		}
		
		BA_LBL_PDEREFMAKE_LOOPEND:

		// Array lea offset: calculated array offset handling
		if (type.type == BA_TYPE_ARR) {
			struct ba_Type tmpType = 
				((struct ba_ArrExtraInfo*)type.extraInfo)->type;
			if (isArrLeaOfst && (tmpType.type != BA_TYPE_ARR || isLastArg)) {
				bool isNeg = (i64)arrLeaOfst < 0;
				ba_AddIM(ctr, 5, BA_IM_LEA, deReg,
					isNeg ? BA_IM_ADRSUB : BA_IM_ADRADD, deReg, 
					isNeg ? -arrLeaOfst : arrLeaOfst);
				isArrLeaOfst = 0;
				arrLeaOfst = 0;
			}
		}
		
		// Leave if last item in the dereferencing list
		if (isLastArg) {
			break;
		}
	}

	struct ba_PTkStkItem* result = ba_MAlloc(sizeof(*result));

	if (deReg) {
		result->lexemeType = BA_TK_IMREGISTER;
		result->val = (void*)deReg;
	}
	else {
		ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP, 
			ctr->currScope->dataSize + deStackPos, BA_IM_RAX);
		ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
		ctr->imStackSize -= 8;
		result->lexemeType = BA_TK_IMSTACK;
		result->val = (void*)ctr->imStackSize;
	}

	result->typeInfo.type = BA_TYPE_DPTR;
	result->typeInfo.extraInfo = type.extraInfo;
	result->isLValue = 1;
	result->isConst = 0;
	ba_StkPush(ctr->pTkStk, result);
	
	ctr->pOpStk = originalOpStk;
	return 1;
}

// Any type of expression
u8 ba_PExp(struct ba_Ctr* ctr) {
	// Parse as if following an operator or parse as if following an atom?
	bool isAfterAtom = 0;

	// Initial level of parenthesization etc
	i64 initParen = ctr->paren;
	i64 initBracket = ctr->bracket;
	/* goto BA_LBL_PEXP_END due to a parenthesis (1) or bracket (2)
	 * or other (0) */
	u8 endDueTo = 0;
	u64 lastPraefix = 0;

	while (1) {
		u64 lexType = ctr->lex->type;
		u64 line = ctr->lex->line;
		u64 col = ctr->lex->colStart;

		// Start of an expression or after a binary operator
		if (!isAfterAtom) {
			// Prefix operator
			if (ba_PAccept('+', ctr) || ba_PAccept('-', ctr) || 
				ba_PAccept('!', ctr) || ba_PAccept('~', ctr) || 
				ba_PAccept('(', ctr) || ba_PAccept('[', ctr) || 
				ba_PAccept('$', ctr) || ba_PAccept('&', ctr) || 
				ba_PAccept(BA_TK_KW_LENGTHOF, ctr) || 
				ba_PAccept(BA_TK_INC, ctr) || ba_PAccept(BA_TK_DEC, ctr))
			{
				lastPraefix = lexType;
				// Left grouping parenthesis / dereferencing bracket
				// Enters a new expression frame (reset whether is cmp chain)
				if (lexType == '(') {
					++ctr->paren;
					ba_StkPush(ctr->cmpRegStk, (void*)0);
				}
				else {
					ctr->isPermitArrLit = 0;
					if (lexType == '[') {
						++ctr->bracket;
						ba_StkPush(ctr->cmpRegStk, (void*)0);
						if (!ba_PDerefListMake(ctr, line, col)) {
							return 0;
						}
						isAfterAtom = 1;
					}
				}
				ba_POpStkPush(ctr->pOpStk, line, col, lexType, BA_OP_PREFIX);
			}
			// Note: ba_PAtom pushes the atom to pTkStk
			else if (ba_PAtom(ctr) || (lastPraefix == '$' && 
				ba_PBaseType(ctr, /* isInclVoid = */ 0, 
					/* isInclIndefArr = */ 0)))
			{
				isAfterAtom = 1;
			}
			else if (ctr->pOpStk->count) {
				return ba_ExitMsg(BA_EXIT_ERR, "expected expression on", 
					line, col, ctr->currPath);
			}
			else {
				return 0;
			}
		}
		// After an atom, postfix or grouped expression
		else {
			struct ba_POpStkItem* op = ba_MAlloc(sizeof(*op));
			op->line = line;
			op->col = col;
			op->lexemeType = lexType;
			op->syntax = 0;

			u64 nextLexType = ctr->lex->type;

			// Set syntax type
			if (ctr->lex->type == ')') {
				ctr->isPermitArrLit = 0;
				if (ctr->paren <= initParen) {
					endDueTo = 1;
					goto BA_LBL_PEXP_END;
				}
				op->syntax = BA_OP_POSTFIX;
				ba_PAccept(')', ctr);
			}
			else if (ctr->lex->type == ']') {
				ctr->isPermitArrLit = 0;
				if (ctr->bracket <= initBracket) {
					endDueTo = 2;
					goto BA_LBL_PEXP_END;
				}
				op->syntax = BA_OP_POSTFIX;
				ba_PAccept(']', ctr);
			}
			else if (ba_PAccept('(', ctr)) {
				op->syntax = BA_OP_POSTFIX;
				++ctr->paren;
				ba_StkPush(ctr->cmpRegStk, (void*)0);
				struct ba_Stk* originalOpStk = ctr->pOpStk;
				
				struct ba_PTkStkItem* funcTk = ba_StkTop(ctr->pTkStk);
				if (!funcTk || funcTk->typeInfo.type != BA_TYPE_FUNC) {
					return ba_ExitMsg(BA_EXIT_ERR, "attempt to call "
						"non-func on", op->line, op->col, ctr->currPath);
				}
				struct ba_Func* func = 
					((struct ba_STVal*)funcTk->val)->type.extraInfo;
				
				u64 funcArgsCnt = 0;
				struct ba_FuncParam* param = func->firstParam;
				while (1) {
					ctr->pOpStk = ba_NewStk();
					if (funcArgsCnt >= func->paramCnt) {
						break;
					}
					ba_StkPush(ctr->expCoercedTypeStk, &param->type);
					ctr->isPermitArrLit = 1;
					bool isExpFound = ba_PExp(ctr);
					ctr->isPermitArrLit = 0;
					ba_StkPop(ctr->expCoercedTypeStk);
					param = param->next;
					
					ba_DelStk(ctr->pOpStk);
					bool isComma = ba_PAccept(',', ctr);

					if (funcArgsCnt || isExpFound || isComma) {
						++funcArgsCnt;
					}

					if (!isExpFound) {
						// Represents no argument (default argument used)
						ba_StkPush(ctr->pTkStk, (void*)0);
					}

					if (!isComma) {
						break;
					}
				}

				if (!funcArgsCnt && func->paramCnt && 
					func->firstParam->hasDefaultVal) 
				{
					++funcArgsCnt;
				}
				else if (funcArgsCnt != func->paramCnt) {
					while (param && param->hasDefaultVal) {
						ba_StkPush(ctr->pTkStk, (void*)0);
						++funcArgsCnt;
						param = param->next;
					}
					if (funcArgsCnt != func->paramCnt) {
						fprintf(stderr, "Error: func on line %llu:%llu in %s "
							"takes %llu parameter%s, but %llu argument%s passed\n", 
							op->line, op->col, ctr->currPath, func->paramCnt, 
							func->paramCnt == 1 ? "" : "s", 
							funcArgsCnt, funcArgsCnt == 1 ? " was" : "s were");
						exit(-1);
					}
				}
				
				// 1 is added since arg cant be 0
				ba_StkPush(ctr->pTkStk, (void*)(1 + funcArgsCnt));
				
				ctr->pOpStk = originalOpStk;
			}
			else if (ba_PAccept('~', ctr)) {
				ctr->isPermitArrLit = 0;
				op->syntax = BA_OP_POSTFIX;
				if (!ba_PBaseType(ctr, /* isInclVoid = */ 0, 
					/* isInclIndefArr = */ 0)) 
				{
					return ba_ExitMsg(BA_EXIT_ERR, "cast to expression that is "
						"not a valid type on", op->line, op->col, ctr->currPath);
				}
			}
			else if (ba_PAccept(BA_TK_LSHIFT, ctr) || 
				ba_PAccept(BA_TK_RSHIFT, ctr) || ba_PAccept('*', ctr) || 
				ba_PAccept(BA_TK_IDIV, ctr) || ba_PAccept('%', ctr) || 
				ba_PAccept('&', ctr) || ba_PAccept('^', ctr) || 
				ba_PAccept('|', ctr) || ba_PAccept('+', ctr) || 
				ba_PAccept('-', ctr) || ba_PAccept(BA_TK_LOGAND, ctr) || 
				ba_PAccept(BA_TK_LOGOR, ctr) || 
				(ba_IsLexemeCompare(nextLexType) && 
					ba_PAccept(nextLexType, ctr)))
			{
				ctr->isPermitArrLit = 0;
				op->syntax = BA_OP_INFIX;
			}
			else if (ba_PAccept('=', ctr) || 
				(ba_IsLexemeCompoundAssign(nextLexType) && 
					ba_PAccept(nextLexType, ctr))) 
			{
				ctr->isPermitArrLit = 1;
				op->syntax = BA_OP_INFIX;
				ba_StkPush(ctr->expCoercedTypeStk, 
					&((struct ba_PTkStkItem*)ba_StkTop(ctr->pTkStk))->typeInfo);
			}
			else {
				goto BA_LBL_PEXP_END;
			}

			// Right parenthesis/bracket
			(lexType == ')') && --ctr->paren;
			(lexType == ']') && --ctr->bracket;

			if (ctr->pOpStk->count && ba_POpIsHandler(op)) {
				do {
					struct ba_POpStkItem* stkTop = ba_StkTop(ctr->pOpStk);
					u8 stkTopPrec = ba_POpPrecedence(stkTop);
					u8 opPrec = ba_POpPrecedence(op);
					bool willHandle = ba_POpIsRightAssoc(op)
						? stkTopPrec < opPrec : stkTopPrec <= opPrec;

					if (willHandle) {
						u8 handleResult = ba_POpHandle(ctr, op);
						if (!handleResult) {
							return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", 
								line, col, ctr->currPath);
						}
						// Left parenthesis
						else if (handleResult == 2) {
							// Return to previous expression frame
							ba_StkPop(ctr->cmpRegStk);
							goto BA_LBL_PEXP_LOOPEND;
						}
					}
					else {
						break;
					}
				}
				while (ctr->pOpStk->count);
			}

			// Short circuiting: jmp ahead if right-hand side operand is unnecessary
			if (lexType == BA_TK_LOGAND || lexType == BA_TK_LOGOR) {
				struct ba_PTkStkItem* lhs = ba_StkPop(ctr->pTkStk);
				ba_StkPush(ctr->shortCircLblStk, (void*)ctr->labelCnt);

				u64 reg = (u64)lhs->val; // Kept only if lhs is a register
				
				if (lhs->lexemeType != BA_TK_IMREGISTER) {
					reg = ba_NextIMRegister(ctr);
				}

				if (!reg) { // Preserve rax
					ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
					ctr->imStackSize += 8;
				}
				
				u64 realReg = reg ? reg : BA_IM_RAX;
				ba_POpMovArgToReg(ctr, lhs, realReg, 
					/* isLiteral = */ lhs->lexemeType != BA_TK_IMREGISTER);

				ba_AddIM(ctr, 3, BA_IM_TEST, realReg, realReg);
				if (!reg) {
					ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
					ctr->imStackSize -= 8;
				}
				ba_AddIM(ctr, 2, 
					lexType == BA_TK_LOGAND ? BA_IM_LABELJZ : BA_IM_LABELJNZ,
						ctr->labelCnt);

				lhs->lexemeType = BA_TK_IMREGISTER;
				lhs->val = (void*)reg;
				ba_StkPush(ctr->pTkStk, lhs);

				++ctr->labelCnt;
			}

			ba_StkPush(ctr->pOpStk, op);

			if (op->syntax == BA_OP_INFIX) {
				isAfterAtom = 0;
			}
		}

		BA_LBL_PEXP_LOOPEND:;
		
		if (ctr->paren < 0) {
			return ba_ExitMsg(BA_EXIT_ERR, "unmatched ')' on", line, col, 
				ctr->currPath);
		}
		if (ctr->bracket < 0) {
			return ba_ExitMsg(BA_EXIT_ERR, "unmatched ']' on", line, col, 
				ctr->currPath);
		}

		line = ctr->lex->line;
		col = ctr->lex->colStart;
	}

	BA_LBL_PEXP_END:;
	
	if ((endDueTo == 1 && ctr->paren != initParen) || 
		(endDueTo == 2 && ctr->bracket != initBracket))
	{
		return 0;
	}
	
	while (ctr->pOpStk->count) {
		if (!ba_POpHandle(ctr, 0)) {
			return 0;
		}
	}

	!ba_PCorrectDPtr(ctr, ba_StkTop(ctr->pTkStk)) &&
		ba_ErrorDerefInvalid(ctr->lex->line, ctr->lex->colStart, ctr->currPath);

	if (ctr->paren || ctr->bracket) {
		return 1;
	}

	ctr->usedRegisters = 0;
	if (ctr->imStackSize) {
		ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_RSP, BA_IM_IMM, ctr->imStackSize);
	}
	ctr->imStackSize = 0;

	ctr->cmpLblStk->count = 0;
	ctr->cmpRegStk->count = 1;
	ctr->cmpRegStk->items[0] = (void*)0;
	ctr->paren = initParen;
	ctr->bracket = initBracket;

	return 1;
}

