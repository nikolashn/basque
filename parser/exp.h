// See LICENSE for copyright/license information

#ifndef BA__EXP_H
#define BA__EXP_H

// ----- Forward declarations -----
u8 ba_PAtom(struct ba_Controller* ctr);
// --------------------------------

// Dereferencing list early handling
u8 ba_PDerefListMake(struct ba_Controller* ctr, u64 line, u64 col) {
	struct ba_Type type;
	struct ba_Stk* originalOpStk = ctr->pOpStk;
	u64 derefArgsCnt = 0;
	u64 deStackPos = 0;
	u64 deReg = 0;
	bool isUsedAsLValue = 0;
	while (1) {
		ctr->pOpStk = ba_NewStk();
		if (!ba_PExp(ctr)) {
			return 0;
		}
		ba_DelStk(ctr->pOpStk);
		
		++derefArgsCnt;
		struct ba_PTkStkItem* item = ba_StkPop(ctr->pTkStk);

		bool isLastArg = !ba_PAccept(',', ctr);

		// Determine if the deref is used as an l-value
		if (isLastArg) {
			/* There are only 2 ways for it to be an l-value: 
			   assignment and the '&' operator. */

			u64 lkBkPos = originalOpStk->count;
			while (lkBkPos) {
				--lkBkPos;
				struct ba_POpStkItem* op = originalOpStk->items[lkBkPos];
				if (op->lexemeType == '&' && op->syntax == BA_OP_PREFIX) {
					isUsedAsLValue = 1;
					goto BA_LBL_PDEREFMAKE_CODEGEN;
				}
				if (op->lexemeType != '(' || op->syntax != BA_OP_PREFIX) {
					break;
				}
			}
			
			struct ba_Lexeme* nextLex = ctr->lex;
			if (!nextLex || nextLex->type != ']') {
				return ba_PExpect(']', ctr);
			}
			nextLex = nextLex->next;
			while (nextLex) {
				if (nextLex->type == '=' || 
					ba_IsLexemeCompoundAssign(nextLex->type))
				{
					isUsedAsLValue = 1;
					goto BA_LBL_PDEREFMAKE_CODEGEN;
				}
				if (nextLex->type != ')') {
					break;
				}
				nextLex = nextLex->next;
			}
		}

		BA_LBL_PDEREFMAKE_CODEGEN:
		if (derefArgsCnt == 1) {
			type = item->typeInfo;
			if (type.type != BA_TYPE_PTR) {
				return ba_ErrorDerefNonPtr(line, col, ctr->currPath);
			}

			deReg = (u64)item->val;
			(item->lexemeType != BA_TK_IMREGISTER) && 
				(deReg = ba_NextIMRegister(ctr));
			if (!deReg) {
				if (!ctr->imStackSize) {
					ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
				}
				deStackPos = ctr->imStackSize + 8;
				// First: result location, second: preserve rax
				ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
				ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
				ctr->imStackSize += 16;
			}

			if (item->lexemeType == BA_TK_IDENTIFIER) {
				ba_AddIM(ctr, 5, BA_IM_MOV, 
					deReg ? deReg : BA_IM_RAX, BA_IM_ADRADD, 
					ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
					ba_CalcSTValOffset(ctr->currScope, item->val));
			}
			else if (item->lexemeType == BA_TK_IMRBPSUB) {
				ba_AddIM(ctr, 5, BA_IM_MOV, deReg ? deReg : BA_IM_RAX, 
					BA_IM_ADRSUB, BA_IM_RBP, (u64)item->val);
			}
			else if (ba_IsLexemeLiteral(item->lexemeType)) {
				ba_AddIM(ctr, 4, BA_IM_MOV, deReg ? deReg : BA_IM_RAX,
					BA_IM_IMM, (u64)item->val);
			}
		}
		else {
			if (type.type != BA_TYPE_PTR) {
				return ba_ErrorDerefNonPtr(line, col, ctr->currPath);
			}
			
			u64 pointedTypeSize = ba_GetSizeOfType(type);
			if (pointedTypeSize != 1 && pointedTypeSize != 2 && 
				pointedTypeSize != 4 && pointedTypeSize != 8)
			{
				fprintf(stderr, "Error: dereferencing pointer pointing to type "
					"with size %llu not implemented, line %llu:%llu in %s\n", 
					pointedTypeSize, line, col, ctr->currPath);
				exit(-1);
			}

			if (!isUsedAsLValue) {
				type = *(struct ba_Type*)type.extraInfo;
			}

			if (ba_IsLexemeLiteral(item->lexemeType)) {
				bool isNeg = (i64)item->val < 0;
				u64 ofst = pointedTypeSize * 
					(isNeg ? (u64)item->val : -(u64)item->val);
				ba_AddIM(ctr, 5, isUsedAsLValue ? BA_IM_LEA : BA_IM_MOV, 
					deReg, isNeg ? BA_IM_ADRADD : BA_IM_ADRSUB, deReg, ofst);
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

			if (item->lexemeType == BA_TK_IDENTIFIER) {
				ba_AddIM(ctr, 5, BA_IM_MOV, effAddReg, BA_IM_ADRADD, 
					ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
					ba_CalcSTValOffset(ctr->currScope, item->val));
			}
			else if (item->lexemeType == BA_TK_IMRBPSUB) {
				ba_AddIM(ctr, 5, BA_IM_MOV, effAddReg, BA_IM_ADRSUB, 
					BA_IM_RBP, (u64)item->val);
			}

			ba_AddIM(ctr, 6, isUsedAsLValue ? BA_IM_LEA : BA_IM_MOV, 
				deReg, BA_IM_ADRADDREGMUL, deReg, pointedTypeSize, addReg);

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

	if (!isUsedAsLValue && derefArgsCnt == 1) {
		ba_AddIM(ctr, 4, BA_IM_MOV, deReg ? deReg : BA_IM_RAX, 
			BA_IM_ADR, deReg ? deReg : BA_IM_RAX);
		type = *(struct ba_Type*)type.extraInfo;
	}

	struct ba_PTkStkItem* result = malloc(sizeof(*result));
	if (!result) {
		return ba_ErrorMallocNoMem();
	}

	isUsedAsLValue && (type.type = BA_TYPE_DPTR);
	
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

	result->typeInfo = type;
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
			// Atom: note that ba_PAtom pushes the atom to pTkStk
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
				if (!ba_PBaseType(ctr)) {
					return ba_ExitMsg(BA_EXIT_ERR, "cast to expression that is "
						"not a type on", op->line, op->col, ctr->currPath);
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
				if (lhs->lexemeType == BA_TK_IDENTIFIER) {
					ba_AddIM(ctr, 5, BA_IM_MOV, realReg, 
						BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP,
						ba_CalcSTValOffset(ctr->currScope, lhs->val));
				}
				else if (lhs->lexemeType == BA_TK_IMRBPSUB) {
					ba_AddIM(ctr, 5, BA_IM_MOV, realReg, BA_IM_ADRSUB,
						BA_IM_RBP, (u64)lhs->val);
				}
				else if (lhs->lexemeType != BA_TK_IMREGISTER) {
					// Literal
					ba_AddIM(ctr, 4, BA_IM_MOV, realReg, BA_IM_IMM, 
						(u64)lhs->val);
				}

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
