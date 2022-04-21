// See LICENSE for copyright/license information

#ifndef BA__EXP_H
#define BA__EXP_H

// ----- Forward declarations -----
void ba_POpAsgnRegOrStack(struct ba_Controller* ctr, u64 lexType, u64* reg, 
	u64* stackPos);
u8 ba_POpMovArgToReg(struct ba_Controller* ctr, struct ba_PTkStkItem* arg, 
	u64 reg, bool isLiteral);
u8 ba_PAtom(struct ba_Controller* ctr);
// --------------------------------

// Dereferencing list early handling
u8 ba_PDerefListMake(struct ba_Controller* ctr, u64 line, u64 col) {
	struct ba_Type type;
	struct ba_Stk* originalOpStk = ctr->pOpStk;
	u64 derefArgsCnt = 0;
	u64 deStackPos = 0;
	u64 deReg = 0;
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
				type = *(struct ba_Type*)type.extraInfo;
			}
			if (type.type != BA_TYPE_PTR && type.type != BA_TYPE_ARR) {
				return ba_ErrorDerefNonPtr(line, col, ctr->currPath);
			}
			
			u64 pntdTypeSize = 
				ba_GetSizeOfType(*(struct ba_Type*)type.extraInfo);
			if (pntdTypeSize != 1 && pntdTypeSize != 2 && 
				pntdTypeSize != 4 && pntdTypeSize != 8)
			{
				fprintf(stderr, "Error: dereferencing pointer pointing to type "
					"with size %llu not implemented, line %llu:%llu in %s\n", 
					pntdTypeSize, line, col, ctr->currPath);
				exit(-1);
			}

			if (ba_IsLexemeLiteral(item->lexemeType)) {
				bool isNeg = (i64)item->val < 0;
				u64 ofst = pntdTypeSize * 
					(isNeg ? (u64)item->val : -(u64)item->val);
				ba_AddIM(ctr, 5, isLastArg ? BA_IM_LEA : BA_IM_MOV, 
					isLastArg ? deReg : ba_AdjRegSize(deReg, pntdTypeSize),
					isNeg ? BA_IM_ADRADD : BA_IM_ADRSUB, deReg, ofst);
				goto BA_LBL_PDEREFMAKE_LOOPEND;
			}

			u64 addReg = (u64)item->val;
			if (item->lexemeType != BA_TK_IMREGISTER) {
				addReg = ba_NextIMRegister(ctr);
			}

			u64 effAddReg = addReg;
			if (!addReg) {
				if (!ctr->imStackSize) {
					ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
				}
				// Only preserve rcx/rdx
				effAddReg = deReg == BA_IM_RCX ? BA_IM_RDX : BA_IM_RCX;
				ba_AddIM(ctr, 2, BA_IM_PUSH, effAddReg);
				ctr->imStackSize += 8;
			}

			ba_POpMovArgToReg(ctr, item, effAddReg, /* isLiteral = */ 0);

			ba_AddIM(ctr, 6, isLastArg ? BA_IM_LEA : BA_IM_MOV,
				isLastArg ? deReg : ba_AdjRegSize(deReg, pntdTypeSize),
				BA_IM_ADRADDREGMUL, deReg, pntdTypeSize, addReg);

			if (!addReg) {
				ba_AddIM(ctr, 2, BA_IM_POP, effAddReg);
				ctr->imStackSize -= 8;
			}
		}
		
		BA_LBL_PDEREFMAKE_LOOPEND:
		if (isLastArg) {
			break;
		}
	}

	struct ba_PTkStkItem* result = malloc(sizeof(*result));
	if (!result) {
		return ba_ErrorMallocNoMem();
	}

	if (deReg) {
		result->lexemeType = BA_TK_IMREGISTER;
		result->val = (void*)deReg;
	}
	else {
		ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRSUB, 
			BA_IM_RBP, deStackPos, BA_IM_RAX);
		ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
		ctr->imStackSize -= 8;
		result->lexemeType = BA_TK_IMRBPSUB;
		result->val = (void*)ctr->imStackSize;
	}

	result->typeInfo.type = BA_TYPE_DPTR;
	result->typeInfo.extraInfo = type.extraInfo;
	result->isLValue = 1;
	ba_StkPush(ctr->pTkStk, result);
	
	ctr->pOpStk = originalOpStk;
	return 1;
}

// Any type of expression
u8 ba_PExp(struct ba_Controller* ctr) {
	// Parse as if following an operator or parse as if following an atom?
	bool isAfterAtom = 0;

	// Initial level of parenthesization etc
	i64 initParen = ctr->paren;
	i64 initBracket = ctr->bracket;
	/* goto BA_LBL_PEXP_END due to a parenthesis (1) or bracket (2)
	 * or other (0) */
	u8 endDueTo = 0;

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
				ba_PAccept(BA_TK_INC, ctr) || ba_PAccept(BA_TK_DEC, ctr))
			{
				// Left grouping parenthesis / dereferencing bracket
				// Enters a new expression frame (reset whether is cmp chain)
				if (lexType == '(') {
					++ctr->paren;
					ba_StkPush(ctr->cmpRegStk, (void*)0);
				}
				else if (lexType == '[') {
					++ctr->bracket;
					ba_StkPush(ctr->cmpRegStk, (void*)0);
					if (!ba_PDerefListMake(ctr, line, col)) {
						return 0;
					}
					isAfterAtom = 1;
				}
				ba_POpStkPush(ctr->pOpStk, line, col, lexType, BA_OP_PREFIX);
			}
			// Note: ba_PAtom pushes the atom to pTkStk
			else if (ba_PAtom(ctr)) {
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
			struct ba_POpStkItem* op = malloc(sizeof(*op));
			if (!op) {
				return ba_ErrorMallocNoMem();
			}

			op->line = line;
			op->col = col;
			op->lexemeType = lexType;
			op->syntax = 0;

			u64 nextLexType = ctr->lex->type;

			// Set syntax type
			if (ctr->lex->type == ')') {
				if (ctr->paren <= initParen) {
					endDueTo = 1;
					goto BA_LBL_PEXP_END;
				}
				op->syntax = BA_OP_POSTFIX;
				ba_PAccept(')', ctr);
			}
			else if (ctr->lex->type == ']') {
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

				u64 funcArgsCnt = 0;
				while (1) {
					ctr->pOpStk = ba_NewStk();
					bool isExpFound = ba_PExp(ctr);
					ba_DelStk(ctr->pOpStk);
					bool isComma = ba_PAccept(',', ctr);

					if (funcArgsCnt || isExpFound || isComma) {
						++funcArgsCnt;
					}

					if (funcArgsCnt && !isExpFound) {
						// Represents no argument (default argument used)
						ba_StkPush(ctr->pTkStk, (void*)0);
					}

					if (!isComma) {
						break;
					}
				}
				// 1 is added since arg cant be 0
				ba_StkPush(ctr->pTkStk, (void*)(1 + funcArgsCnt));
				
				ctr->pOpStk = originalOpStk;
			}
			else if (ba_PAccept('~', ctr)) {
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
				ba_PAccept(BA_TK_LOGOR, ctr) || ba_PAccept('=', ctr) || 
				(ba_IsLexemeCompoundAssign(nextLexType) && 
					ba_PAccept(nextLexType, ctr)) || 
				(ba_IsLexemeCompare(nextLexType) && 
					ba_PAccept(nextLexType, ctr)))
			{
				op->syntax = BA_OP_INFIX;
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
		ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_RSP, BA_IM_IMM, 
			ctr->imStackSize);
	}
	ctr->imStackSize = 0;

	ctr->cmpLblStk->count = 0;
	ctr->cmpRegStk->count = 1;
	ctr->cmpRegStk->items[0] = (void*)0;
	ctr->paren = initParen;
	ctr->bracket = initBracket;

	return 1;
}

#endif
