// See LICENSE for copyright/license information

#ifndef BA__PARSER_H
#define BA__PARSER_H

#include "lexer.h"

u8 ba_PAccept(u64 type, struct ba_Controller* ctr) {
	if (!ctr->lex || (ctr->lex->type != type)) {
		return 0;
	}
	ctr->lex = ba_DelLexeme(ctr->lex);
	return 1;
}

u8 ba_PExpect(u64 type, struct ba_Controller* ctr) {
	if (!ba_PAccept(type, ctr)) {
		printf("Error: expected %s at line %llu:%llu\n",
			ba_GetLexemeStr(type), ctr->lex->line, ctr->lex->colStart);
		exit(-1);
		return 0;
	}
	return 1;
}

// Implicit definitions

u8 ba_PExp(struct ba_Controller* ctr);

/* base_type = "u64" | "i64" */
u8 ba_PBaseType(struct ba_Controller* ctr) {
	if (ba_PAccept(BA_TK_KW_U64, ctr)) {
		ctr->lastType = BA_TYPE_U64;
	}
	else if (ba_PAccept(BA_TK_KW_I64, ctr)) {
		ctr->lastType = BA_TYPE_I64;
	}
	else {
		return 0;
	}

	return 1;
}

/* atom = lit_str { lit_str }
 *      | lit_int
 */
u8 ba_PAtom(struct ba_Controller* ctr) {
	u64 lexLine = ctr->lex->line;
	u64 lexColStart = ctr->lex->colStart;
	u64 lexValLen = ctr->lex->valLen;
	char* lexVal = 0;
	if (ctr->lex->val) {
		lexVal = malloc(lexValLen+1);
		strcpy(lexVal, ctr->lex->val);
	}

	// lit_str { lit_str }
	if (ba_PAccept(BA_TK_LITSTR, ctr)) {
		u64 len = lexValLen;
		char* str = malloc(len + 1);
		strcpy(str, lexVal);
		
		do {
			if (ctr->lex->type != BA_TK_LITSTR) {
				break;
			}
			u64 oldLen = len;
			len += ctr->lex->valLen;
			
			if (len > BA_STACK_SIZE) {
				return ba_ExitMsg2(0, "string at", ctr->lex->line, ctr->lex->colStart, 
					" too large to fit on the stack");
			}

			str = realloc(str, len);
			strcpy(str+oldLen, ctr->lex->val);
		} while (ba_PAccept(BA_TK_LITSTR, ctr));

		// Some operations are different for string literal vs. char*
		// e.g. !STR is valid for char* but invalid for string literal
		ctr->lastType = BA_TK_LITSTR;
		ba_StkPush((void*)len, ctr->stk);
		ba_StkPush((void*)str, ctr->stk);
	}
	// lit_int
	else if (ba_PAccept(BA_TK_LITINT, ctr)) {
		u64 num;
		ctr->lastType = BA_TYPE_U64;
		if (lexVal[lexValLen-1] == 'u') {
			// ba_StrToU64 cannot parse the 'u' suffix so it must be removed
			lexVal[--lexValLen] = 0;
			num = ba_StrToU64(lexVal, lexLine, lexColStart);
		}
		else {
			num = ba_StrToU64(lexVal, lexLine, lexColStart);
			if (num < (1llu << 63)) {
				ctr->lastType = BA_TYPE_I64;
			}
		}
		ba_StkPush((void*)num, ctr->stk);
	}
	// Other
	else {
		return 0;
	}

	free(lexVal);
	return 1;
}

/* expPre = "+" expPre
 *        | "-" expPre
 *        | "!" expPre
 *        | "~" expPre
 *        | "(" exp ")"
 *        | atom
 */
u8 ba_PExpPre(struct ba_Controller* ctr) {
	u64 lexLine = ctr->lex->line;
	u64 lexCol = ctr->lex->colStart;

	// "+" expPre
	if (ba_PAccept('+', ctr)) {
		if (!ba_PExpPre(ctr)) {
			return 0;
		}

		if (ba_IsTypeUnsigned(ctr->lastType)) {
			u64 oprnd = (u64)ba_StkPop(ctr->stk);
			ctr->lastType = BA_TYPE_U64;
			ba_StkPush((void*)oprnd, ctr->stk);
		}
		else if (ba_IsTypeSigned(ctr->lastType)) {
			i64 oprnd = (i64)ba_StkPop(ctr->stk);
			ctr->lastType = BA_TYPE_I64;
			ba_StkPush((void*)oprnd, ctr->stk);
		}
		else {
			return ba_ExitMsg(0, "unary '+' used with non numeric operand on", 
				lexLine, lexCol);
		}
		
		// Don't do anything with the ctr stk since the number is just the same

		return 1;
	}
	// "-" expPre
	else if (ba_PAccept('-', ctr)) {
		if (!ba_PExpPre(ctr)) {
			return 0;
		}

		if (ba_IsTypeUnsigned(ctr->lastType)) {
			u64 oprnd = (u64)ba_StkPop(ctr->stk);
			ctr->lastType = BA_TYPE_U64;
			if (oprnd > (1llu << 63)) {
				ba_ExitMsg(1, "unary '-' operation on integer too large to be "
					"represented as i64 on", lexLine, lexCol);
			}
			oprnd = -oprnd;
			ba_StkPush((void*)oprnd, ctr->stk);
		}
		else if (ba_IsTypeSigned(ctr->lastType)) {
			i64 oprnd = (i64)ba_StkPop(ctr->stk);
			ctr->lastType = BA_TYPE_I64;
			oprnd = -oprnd;
			ba_StkPush((void*)oprnd, ctr->stk);
		}
		else {
			return ba_ExitMsg(0, "unary '-' used with non numeric operand on", 
				lexLine, lexCol);
		}

		return 1;
	}
	// "!" expPre
	else if (ba_PAccept('!', ctr)) {
		if (!ba_PExpPre(ctr)) {
			return 0;
		}
		
		if (!ba_IsTypeNumeric(ctr->lastType)) {
			ba_ExitMsg(0, "unary '!' used with non numeric operand on",
				lexLine, lexCol);
			exit(-1);
			return 0;
		}
		
		u64 oprnd = (u64)ba_StkPop(ctr->stk);
		oprnd = !oprnd;
		ctr->lastType = BA_TYPE_U8;
		ba_StkPush((void*)oprnd, ctr->stk);

		return 1;
	}
	// "~" expPre
	else if (ba_PAccept('~', ctr)) {
		if (!ba_PExpPre(ctr)) {
			return 0;
		}
		
		if (ba_IsTypeUnsigned(ctr->lastType)) {
			u64 oprnd = (u64)ba_StkPop(ctr->stk);
			ctr->lastType = BA_TYPE_U64;
			oprnd = ~oprnd;
			ba_StkPush((void*)oprnd, ctr->stk);
		}
		else if (ba_IsTypeSigned(ctr->lastType)) {
			i64 oprnd = (i64)ba_StkPop(ctr->stk);
			ctr->lastType = BA_TYPE_I64;
			oprnd = ~oprnd;
			ba_StkPush((void*)oprnd, ctr->stk);
		}
		else {
			ba_ExitMsg(0, "unary '~' used with non numeric operand on",
				lexLine, lexCol);
			exit(-1);
			return 0;
		}

		return 1;
	}
	// "(" exp ")"
	else if (ba_PAccept('(', ctr)) {
		lexLine = ctr->lex->line;
		lexCol = ctr->lex->colStart;
		if (!ba_PExp(ctr)) {
			ba_ExitMsg(0, "expected expression on", lexLine, lexCol);
			exit(-1);
			return 0;
		}
		if (ba_PExpect(')', ctr)) {
			return 1;
		}
	}
	// atom
	else if (ba_PAtom(ctr)) {
		return 1;
	}
	
	return 0;
}

/* expFactor = expPre { ">>" expPre 
 *           | "<<" expPre } */
u8 ba_PExpFactor(struct ba_Controller* ctr) {
	u64 lhsLine = ctr->lex->line;
	u64 lhsCol = ctr->lex->colStart;
	
	if (!ba_PExpPre(ctr)) {
		return 0;
	}

	u64 lhsType = ctr->lastType;
	
	u64 opType = ctr->lex->type;
	
	while (ba_PAccept(BA_TK_RSHIFT, ctr) || ba_PAccept(BA_TK_LSHIFT, ctr)) {
		u64 rhsLine = ctr->lex->line;
		u64 rhsCol = ctr->lex->colStart;

		if (!ba_PExpPre(ctr)) {
			return 0;
		}
		
		union { u64 u; i64 i; } lhs;
		union { u64 u; i64 i; } rhs;
		
		rhs.u = (u64)ba_StkPop(ctr->stk);
		lhs.u = (u64)ba_StkPop(ctr->stk);

		if (ba_IsTypeSigned(ctr->lastType)) {
			if (rhs.i < 0) {
				return ba_ExitMsg(0, "negative rhs operand of bit shift on", 
					rhsLine, rhsCol);
			}
		}
		else if (!ba_IsTypeUnsigned(ctr->lastType)) {
			return ba_ExitMsg(0, "non integral operand of bit shift on", 
				rhsLine, rhsCol);
		}

		if (ba_IsTypeSigned(lhsType)) {
			ctr->lastType = BA_TYPE_I64;
			if (opType == BA_TK_RSHIFT) {
				lhs.u >>= rhs.u;
			}
			else if (opType == BA_TK_LSHIFT) {
				lhs.u <<= rhs.u;
			}
		}
		else if (ba_IsTypeUnsigned(lhsType)) {
			ctr->lastType = BA_TYPE_U64;
			if (opType == BA_TK_RSHIFT) {
				lhs.u >>= rhs.u;
			}
			else if (opType == BA_TK_LSHIFT) {
				lhs.u <<= rhs.u;
			}
		}
		else {
			return ba_ExitMsg(0, "non integral lhs operand of bit shift on",
				lhsLine, lhsCol);
		}

		ba_StkPush((void*)lhs.u, ctr->stk);

		opType = ctr->lex->type;
	}
	
	return 1;
}

/* expTerm = expFactor { "*" expFactor
 *                     | "//" expFactor
 *                     | "%" expFactor }
 */
u8 ba_PExpTerm(struct ba_Controller* ctr) {
	u64 lhsLine = ctr->lex->line;
	u64 lhsCol = ctr->lex->colStart;
	
	if (!ba_PExpFactor(ctr)) {
		return 0;
	}

	u64 lhsType = ctr->lastType;
	
	u64 opType = ctr->lex->type;
	
	while (ba_PAccept('*', ctr) || ba_PAccept(BA_TK_DBSLASH, ctr) ||
			ba_PAccept('%', ctr))
	{
		u64 rhsLine = ctr->lex->line;
		u64 rhsCol = ctr->lex->colStart;

		if (!ba_PExpFactor(ctr)) {
			return 0;
		}
		
		if (opType == '*') {
			union { u64 u; i64 i; } lhs;
			union { u64 u; i64 i; } rhs;
			
			rhs.u = (u64)ba_StkPop(ctr->stk);
			lhs.u = (u64)ba_StkPop(ctr->stk);

			if (ba_IsTypeUnsigned(lhsType)) {
				if (ctr->lastType == BA_TYPE_I64) {
					char* msgPost = "";
					if (!ba_IsWarningsAsErrors) {
						msgPost = ", implicitly converted lhs to i64";
					}
					ba_ExitMsg2(1, "multiplying integers of different signedness "
						"(u64, i64) on", rhsLine, rhsCol, msgPost);

					ctr->lastType = BA_TYPE_I64;
					lhs.i *= rhs.i;
				}
				else {
					ctr->lastType = BA_TYPE_U64;
					lhs.u *= rhs.u;
				}
			}
			else if (ba_IsTypeSigned(lhsType)) {
				if (ctr->lastType == BA_TYPE_U64) {
					char* msgPost = "";
					if (!ba_IsWarningsAsErrors) {
						msgPost = ", implicitly converted rhs to i64";
					}
					ba_ExitMsg2(1, "multiplying integers of different signedness "
						"(i64, u64) on", rhsLine, rhsCol, msgPost);
				}

				ctr->lastType = BA_TYPE_I64;
				lhs.i *= rhs.i;
			}
			else {
				return ba_ExitMsg(0, "non numeric operand of multiplication on",
					lhsLine, lhsCol);
			}

			ba_StkPush((void*)lhs.u, ctr->stk);
		}
		else if (opType == BA_TK_DBSLASH) {
			union { u64 u; i64 i; } lhs;
			union { u64 u; i64 i; } rhs;
			
			rhs.u = (u64)ba_StkPop(ctr->stk);
			lhs.u = (u64)ba_StkPop(ctr->stk);

			if (ba_IsTypeUnsigned(lhsType)) {
				if (ctr->lastType == BA_TYPE_I64) {
					char* msgPost = "";
					if (!ba_IsWarningsAsErrors) {
						msgPost = ", implicitly converted lhs to i64";
					}
					ba_ExitMsg2(1, "integer division of integers of different signedness "
						"(u64, i64) on", rhsLine, rhsCol, msgPost);

					ctr->lastType = BA_TYPE_I64;
					lhs.i /= rhs.i;
				}
				else {
					ctr->lastType = BA_TYPE_U64;
					lhs.u /= rhs.u;
				}
			}
			else if (ba_IsTypeSigned(lhsType)) {
				if (ctr->lastType == BA_TYPE_U64) {
					char* msgPost = "";
					if (!ba_IsWarningsAsErrors) {
						msgPost = ", implicitly converted rhs to i64";
					}
					ba_ExitMsg2(1, "integer division of integers of different signedness "
						"(i64, u64) on", rhsLine, rhsCol, msgPost);
				}

				ctr->lastType = BA_TYPE_I64;
				lhs.i /= rhs.i;
			}
			else {
				return ba_ExitMsg(0, "non numeric operand of integer division on",
					lhsLine, lhsCol);
			}

			ba_StkPush((void*)lhs.u, ctr->stk);
		}
		else if (opType == '%') {
			union { u64 u; i64 i; } lhs;
			union { u64 u; i64 i; } rhs;
			
			rhs.u = (u64)ba_StkPop(ctr->stk);
			lhs.u = (u64)ba_StkPop(ctr->stk);

			if (ba_IsTypeUnsigned(lhsType)) {
				if (ctr->lastType == BA_TYPE_I64) {
					char* msgPost = "";
					if (!ba_IsWarningsAsErrors) {
						msgPost = ", implicitly converted lhs to i64";
					}
					ba_ExitMsg2(1, "modulo of integers of different signedness "
						"(u64, i64) on", rhsLine, rhsCol, msgPost);

					ctr->lastType = BA_TYPE_I64;
					lhs.i %= rhs.i;
				}
				else {
					ctr->lastType = BA_TYPE_U64;
					lhs.u %= rhs.u;
				}
			}
			else if (ba_IsTypeSigned(lhsType)) {
				if (ctr->lastType == BA_TYPE_U64) {
					char* msgPost = "";
					if (!ba_IsWarningsAsErrors) {
						msgPost = ", implicitly converted rhs to i64";
					}
					ba_ExitMsg2(1, "modulo of integers of different signedness "
						"(i64, u64) on", rhsLine, rhsCol, msgPost);
				}

				ctr->lastType = BA_TYPE_I64;
				lhs.i %= rhs.i;
			}
			else {
				return ba_ExitMsg(0, "non numeric operand of modulo on",
					lhsLine, lhsCol);
			}

			ba_StkPush((void*)lhs.u, ctr->stk);
		}

		opType = ctr->lex->type;
	}
	
	return 1;
}

/* expBitAnd = expTerm { "&" expTerm } */
u8 ba_PExpBitAnd(struct ba_Controller* ctr) {
	u64 lhsLine = ctr->lex->line;
	u64 lhsCol = ctr->lex->colStart;

	if (!ba_PExpTerm(ctr)) {
		return 0;
	}

	u64 lhsType = ctr->lastType;
	
	while (ba_PAccept('&', ctr)) {
		u64 rhsLine = ctr->lex->line;
		u64 rhsCol = ctr->lex->colStart;

		if (!ba_PExpTerm(ctr)) {
			return 0;
		}
		
		union { u64 u; i64 i; } lhs;
		union { u64 u; i64 i; } rhs;
		
		rhs.u = (u64)ba_StkPop(ctr->stk);
		lhs.u = (u64)ba_StkPop(ctr->stk);

		if (ba_IsTypeUnsigned(lhsType)) {
			if (ctr->lastType == BA_TYPE_I64) {
				char* msgPost = "";
				if (!ba_IsWarningsAsErrors) {
					msgPost = ", implicitly converted lhs to i64";
				}
				ba_ExitMsg2(1, "'&' operation on integers of different signedness "
					"(u64, i64) on", rhsLine, rhsCol, msgPost);

				ctr->lastType = BA_TYPE_I64;
				lhs.i &= rhs.i;
			}
			else {
				ctr->lastType = BA_TYPE_U64;
				lhs.u &= rhs.u;
			}
		}
		else if (ba_IsTypeSigned(lhsType)) {
			if (ctr->lastType == BA_TYPE_U64) {
				char* msgPost = "";
				if (!ba_IsWarningsAsErrors) {
					msgPost = ", implicitly converted rhs to i64";
				}
				ba_ExitMsg2(1, "'&' operation on integers of different signedness "
					"(i64, u64) on", rhsLine, rhsCol, msgPost);
			}

			ctr->lastType = BA_TYPE_I64;
			lhs.i &= rhs.i;
		}
		else {
			return ba_ExitMsg(0, "non integral operand of '&' operation on",
				lhsLine, lhsCol);
		}

		ba_StkPush((void*)lhs.u, ctr->stk);
	}

	return 1;
}

/* expBitOr = expBitAnd { "|" expBitAnd } */
u8 ba_PExpBitOr(struct ba_Controller* ctr) {
	u64 lhsLine = ctr->lex->line;
	u64 lhsCol = ctr->lex->colStart;

	if (!ba_PExpBitAnd(ctr)) {
		return 0;
	}

	u64 lhsType = ctr->lastType;
	
	while (ba_PAccept('|', ctr)) {
		u64 rhsLine = ctr->lex->line;
		u64 rhsCol = ctr->lex->colStart;

		if (!ba_PExpBitAnd(ctr)) {
			return 0;
		}
		
		union { u64 u; i64 i; } lhs;
		union { u64 u; i64 i; } rhs;
		
		rhs.u = (u64)ba_StkPop(ctr->stk);
		lhs.u = (u64)ba_StkPop(ctr->stk);

		if (ba_IsTypeUnsigned(lhsType)) {
			if (ctr->lastType == BA_TYPE_I64) {
				char* msgPost = "";
				if (!ba_IsWarningsAsErrors) {
					msgPost = ", implicitly converted lhs to i64";
				}
				ba_ExitMsg2(1, "'|' operation on integers of different signedness "
					"(u64, i64) on", rhsLine, rhsCol, msgPost);

				ctr->lastType = BA_TYPE_I64;
				lhs.i |= rhs.i;
			}
			else {
				ctr->lastType = BA_TYPE_U64;
				lhs.u |= rhs.u;
			}
		}
		else if (ba_IsTypeSigned(lhsType)) {
			if (ctr->lastType == BA_TYPE_U64) {
				char* msgPost = "";
				if (!ba_IsWarningsAsErrors) {
					msgPost = ", implicitly converted rhs to i64";
				}
				ba_ExitMsg2(1, "'|' operation on integers of different signedness "
					"(i64, u64) on", rhsLine, rhsCol, msgPost);
			}

			ctr->lastType = BA_TYPE_I64;
			lhs.i |= rhs.i;
		}
		else {
			return ba_ExitMsg(0, "non integral operand of '|' operation on",
				lhsLine, lhsCol);
		}

		ba_StkPush((void*)lhs.u, ctr->stk);
	}

	return 1;
}

/* expBitXor = expBitOr { "^" expBitOr } */
u8 ba_PExpBitXor(struct ba_Controller* ctr) {
	u64 lhsLine = ctr->lex->line;
	u64 lhsCol = ctr->lex->colStart;

	if (!ba_PExpBitOr(ctr)) {
		return 0;
	}

	u64 lhsType = ctr->lastType;
	
	while (ba_PAccept('^', ctr)) {
		u64 rhsLine = ctr->lex->line;
		u64 rhsCol = ctr->lex->colStart;

		if (!ba_PExpBitOr(ctr)) {
			return 0;
		}
		
		union { u64 u; i64 i; } lhs;
		union { u64 u; i64 i; } rhs;
		
		rhs.u = (u64)ba_StkPop(ctr->stk);
		lhs.u = (u64)ba_StkPop(ctr->stk);

		if (ba_IsTypeUnsigned(lhsType)) {
			if (ctr->lastType == BA_TYPE_I64) {
				char* msgPost = "";
				if (!ba_IsWarningsAsErrors) {
					msgPost = ", implicitly converted lhs to i64";
				}
				ba_ExitMsg2(1, "'^' operation on integers of different signedness "
					"(u64, i64) on", rhsLine, rhsCol, msgPost);

				ctr->lastType = BA_TYPE_I64;
				lhs.i ^= rhs.i;
			}
			else {
				ctr->lastType = BA_TYPE_U64;
				lhs.u ^= rhs.u;
			}
		}
		else if (ba_IsTypeSigned(lhsType)) {
			if (ctr->lastType == BA_TYPE_U64) {
				char* msgPost = "";
				if (!ba_IsWarningsAsErrors) {
					msgPost = ", implicitly converted rhs to i64";
				}
				ba_ExitMsg2(1, "'^' operation on integers of different signedness "
					"(i64, u64) on", rhsLine, rhsCol, msgPost);
			}

			ctr->lastType = BA_TYPE_I64;
			lhs.i ^= rhs.i;
		}
		else {
			return ba_ExitMsg(0, "non integral operand of '^' operation on",
				lhsLine, lhsCol);
		}

		ba_StkPush((void*)lhs.u, ctr->stk);
	}

	return 1;
}

/* expSum = expBitXor { "+" expBitXor 
 *                    | "-" expBitXor } 
 */
u8 ba_PExpSum(struct ba_Controller* ctr) {
	u64 lhsLine = ctr->lex->line;
	u64 lhsCol = ctr->lex->colStart;
	
	if (!ba_PExpBitXor(ctr)) {
		return 0;
	}

	u64 lhsType = ctr->lastType;
	
	u64 opType = ctr->lex->type;
	
	while (ba_PAccept('+', ctr) || ba_PAccept('-', ctr)) {
		u64 rhsLine = ctr->lex->line;
		u64 rhsCol = ctr->lex->colStart;

		if (!ba_PExpBitXor(ctr)) {
			return 0;
		}

		if (opType == '+') {
			union { u64 u; i64 i; } lhs;
			union { u64 u; i64 i; } rhs;
			
			rhs.u = (u64)ba_StkPop(ctr->stk);
			lhs.u = (u64)ba_StkPop(ctr->stk);

			if (ba_IsTypeUnsigned(lhsType)) {
				if (ctr->lastType == BA_TYPE_I64) {
					char* msgPost = "";
					if (!ba_IsWarningsAsErrors) {
						msgPost = ", implicitly converted lhs to i64";
					}
					ba_ExitMsg2(1, "adding integers of different signedness "
						"(u64, i64) on", rhsLine, rhsCol, msgPost);

					ctr->lastType = BA_TYPE_I64;
					lhs.i += rhs.i;
				}
				else {
					ctr->lastType = BA_TYPE_U64;
					lhs.u += rhs.u;
				}
			}
			else if (ba_IsTypeSigned(lhsType)) {
				if (ctr->lastType == BA_TYPE_U64) {
					char* msgPost = "";
					if (!ba_IsWarningsAsErrors) {
						msgPost = ", implicitly converted rhs to i64";
					}
					ba_ExitMsg2(1, "adding integers of different signedness "
						"(i64, u64) on", rhsLine, rhsCol, msgPost);
				}

				ctr->lastType = BA_TYPE_I64;
				lhs.i += rhs.i;
			}
			else {
				return ba_ExitMsg(0, "non numeric operand of addition on",
					lhsLine, lhsCol);
			}

			ba_StkPush((void*)lhs.u, ctr->stk);
		}
		else if (opType == '-') {
			union { u64 u; i64 i; } lhs;
			union { u64 u; i64 i; } rhs;
			
			rhs.u = (u64)ba_StkPop(ctr->stk);
			lhs.u = (u64)ba_StkPop(ctr->stk);

			if (ba_IsTypeUnsigned(lhsType)) {
				if (ctr->lastType == BA_TYPE_I64) {
					char* msgPost = "";
					if (!ba_IsWarningsAsErrors) {
						msgPost = ", implicitly converted lhs to i64";
					}
					ba_ExitMsg2(1, "subtraction of integers of different signedness "
						"(u64, i64) on", rhsLine, rhsCol, msgPost);

					ctr->lastType = BA_TYPE_I64;
					lhs.i -= rhs.i;
				}
				else {
					ctr->lastType = BA_TYPE_U64;
					lhs.u -= rhs.u;
				}
			}
			else if (ba_IsTypeSigned(lhsType)) {
				if (ctr->lastType == BA_TYPE_U64) {
					char* msgPost = "";
					if (!ba_IsWarningsAsErrors) {
						msgPost = ", implicitly converted rhs to i64";
					}
					ba_ExitMsg2(1, "subtraction of integers of different signedness "
						"(i64, u64) on", rhsLine, rhsCol, msgPost);
				}

				ctr->lastType = BA_TYPE_I64;
				lhs.i -= rhs.i;
			}
			else {
				return ba_ExitMsg(0, "non numeric operand of subtraction on",
					lhsLine, lhsCol);
			}

			ba_StkPush((void*)lhs.u, ctr->stk);
		}

		opType = ctr->lex->type;
	}
	
	return 1;
}

/* expComp = TODO */
u8 ba_PExpComp(struct ba_Controller* ctr) {
	return ba_PExpSum(ctr);
}

/* expLAnd = expComp { "&&" expComp } */
u8 ba_PExpLAnd(struct ba_Controller* ctr) {
	u64 lhsLine = ctr->lex->line;
	u64 lhsCol = ctr->lex->colStart;

	if (!ba_PExpComp(ctr)) {
		return 0;
	}

	u64 lhsType = ctr->lastType;
	
	while (ba_PAccept(BA_TK_DBAMPD, ctr)) {
		u64 rhsLine = ctr->lex->line;
		u64 rhsCol = ctr->lex->colStart;

		if (!ba_PExpComp(ctr)) {
			return 0;
		}

		u64 rhsType = ctr->lastType;
		
		if (!ba_IsTypeUnsigned(lhsType) && !ba_IsTypeSigned(lhsType)) {
			return ba_ExitMsg(0, "non numeric operand of '&&' operation on",
				lhsLine, lhsCol);
		}
		else if (!ba_IsTypeUnsigned(rhsType) && !ba_IsTypeSigned(rhsType)) {
			return ba_ExitMsg(0, "non numeric operand of '&&' operation on",
				rhsLine, rhsCol);
		}
		else {
			u64 rhs = (u64)ba_StkPop(ctr->stk);
			u64 lhs = (u64)ba_StkPop(ctr->stk);
			lhs = lhs && rhs;
			ba_StkPush((void*)lhs, ctr->stk);
		}
	}

	return 1;
}

/* expLOr = expLAnd { "||" expLAnd } */
u8 ba_PExpLOr(struct ba_Controller* ctr) {
	u64 lhsLine = ctr->lex->line;
	u64 lhsCol = ctr->lex->colStart;

	if (!ba_PExpLAnd(ctr)) {
		return 0;
	}

	u64 lhsType = ctr->lastType;
	
	while (ba_PAccept(BA_TK_DBBAR, ctr)) {
		u64 rhsLine = ctr->lex->line;
		u64 rhsCol = ctr->lex->colStart;

		if (!ba_PExpLAnd(ctr)) {
			return 0;
		}

		u64 rhsType = ctr->lastType;
		
		if (!ba_IsTypeUnsigned(lhsType) && !ba_IsTypeSigned(lhsType)) {
			return ba_ExitMsg(0, "non numeric operand of '||' operation on",
				lhsLine, lhsCol);
		}
		else if (!ba_IsTypeUnsigned(rhsType) && !ba_IsTypeSigned(rhsType)) {
			return ba_ExitMsg(0, "non numeric operand of '||' operation on",
				rhsLine, rhsCol);
		}
		else {
			u64 rhs = (u64)ba_StkPop(ctr->stk);
			u64 lhs = (u64)ba_StkPop(ctr->stk);
			lhs = lhs || rhs;
			ba_StkPush((void*)lhs, ctr->stk);
		}
	}

	return 1;
}

/* exp = TODO */
u8 ba_PExp(struct ba_Controller* ctr) {
	return ba_PExpLOr(ctr);
}

/* stmt = "write" exp ";" 
 *      | base_type identifier [ "=" exp ] ";" 
 *      | exp ";"
 *      | ";" 
 */
u8 ba_PStmt(struct ba_Controller* ctr) {
	// TODO: this should eventually be replaced with a Write() function
	// and a lone string literal statement
	
	// "write" ...
	if (ba_PAccept(BA_TK_KW_WRITE, ctr)) {
		// ... exp ...
		if (!ba_PExp(ctr)) {
			return 0;
		}

		char* str;
		u64 len;
		if (ctr->lastType == BA_TK_LITSTR) {
			str = (char*)ba_StkPop(ctr->stk);
			len = (u64)ba_StkPop(ctr->stk);
		}
		// Everything is printed as unsigned, this will be removed in the 
		// future anyway so i don't care about adding signed representation
		else if (ba_IsTypeNumeric(ctr->lastType)) {
			str = ba_U64ToStr((u64)ba_StkPop(ctr->stk));
			len = strlen(str);
		}

		// ... ";"
		// Generates IM code
		if (ba_PExpect(';', ctr)) {
			// Round up to nearest 0x10
			u64 memLen = len & 0xf;
			if (memLen) {
				memLen ^= len;
				memLen += 0x10;
			}
			else {
				memLen = len;
			}
			
			// Allocate stack memory
			ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
			ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RSP, BA_IM_IMM, memLen);

			u64 val = 0;
			u64 strIter = 0;
			u64 adrSub = memLen;
			
			// Store the string on the stack
			while (1) {
				if ((strIter && !(strIter & 7)) || (strIter >= len)) {
					ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, val);
					ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP, adrSub, BA_IM_RAX);
					
					if (!(strIter & 7)) {
						adrSub -= 8;
						val = 0;
					}
					if (strIter >= len) {
						break;
					}
				}
				val |= (u64)str[strIter] << ((strIter & 7) << 3);
				++strIter;
			}
			
			// write
			ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 1);
			ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RDI, BA_IM_IMM, 1);
			ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSI, BA_IM_RSP);
			ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RDX, BA_IM_IMM, len);
			ba_AddIM(ctr, 1, BA_IM_SYSCALL);
			
			// deallocate stack memory
			ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_RSP, BA_IM_IMM, memLen);
			free(str);

			return 1;
		}
	}
	// base_type identifier [ "=" exp ] ";"
	else if (ba_PBaseType(ctr)) {
		u64 varType = ctr->lastType;
		u64 idNameLen = ctr->lex->valLen;
		char* idName = 0;
		if (ctr->lex->val) {
			idName = malloc(idNameLen+1);
			strcpy(idName, ctr->lex->val);
		}

		u64 line = ctr->lex->line;
		u64 col = ctr->lex->colStart;

		if (!ba_PExpect(BA_TK_IDENTIFIER, ctr)) {
			return 0;
		}

		struct ba_STVal* idVal = ba_STGet(ctr->globalST, idName);
		
		// TODO: scope
		
		if (idVal) {
			return ba_ErrorVarRedef(idName, line, col);
		}

		idVal = malloc(sizeof(struct ba_STVal)); //TODO: free later on
		idVal->type = varType;
		idVal->address = 0;
		idVal->initVal = 0;
		idVal->isInited = 0;

		// TODO: scope
		ba_STSet(ctr->globalST, idName, idVal);

		if (ba_PAccept('=', ctr)) {
			idVal->isInited = 1;

			u64 line = ctr->lex->line;
			u64 col = ctr->lex->colStart;

			if (!ba_PExp(ctr)) {
				return 0;
			}

			u64 expType = ctr->lastType;
			
			if (ba_IsTypeNumeric(varType)) {
				if (!ba_IsTypeNumeric(expType)) {
					char* expTypeStr = ba_GetTypeStr(expType);
					char* varTypeStr = ba_GetTypeStr(varType);
					return ba_ErrorAssignTypes(expTypeStr, idName, 
						varTypeStr, line, col);
				}

				idVal->initVal = (void*)ba_StkPop(ctr->stk);
			}
		}

		if (!ba_PExpect(';', ctr)) {
			return 0;
		}

		return 1;
	}
	// exp ";"
	else if (ba_PExp(ctr)) {
		// String literals by themselves in statements are written to standard output
		if (ctr->lastType == BA_TK_LITSTR) {
			// Copy pasted from "write", TODO: remove this notice once write is removed
			char* str;
			u64 len;
			str = (char*)ba_StkPop(ctr->stk);
			len = (u64)ba_StkPop(ctr->stk);

			// Generates IM code
			if (ba_PExpect(';', ctr)) {
				// Round up to nearest 0x10
				u64 memLen = len & 0xf;
				if (memLen) {
					memLen ^= len;
					memLen += 0x10;
				}
				else {
					memLen = len;
				}
				
				// Allocate stack memory
				ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
				ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RSP, BA_IM_IMM, memLen);

				u64 val = 0;
				u64 strIter = 0;
				u64 adrSub = memLen;
				
				// Store the string on the stack
				while (1) {
					if ((strIter && !(strIter & 7)) || (strIter >= len)) {
						ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, val);
						ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP, adrSub, BA_IM_RAX);
						
						if (!(strIter & 7)) {
							adrSub -= 8;
							val = 0;
						}
						if (strIter >= len) {
							break;
						}
					}
					val |= (u64)str[strIter] << ((strIter & 7) << 3);
					++strIter;
				}
				
				// write
				ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 1);
				ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RDI, BA_IM_IMM, 1);
				ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSI, BA_IM_RSP);
				ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RDX, BA_IM_IMM, len);
				ba_AddIM(ctr, 1, BA_IM_SYSCALL);
				
				// deallocate stack memory
				ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_RSP, BA_IM_IMM, memLen);
				free(str);

				return 1;
			}
		}

		// TODO
		
		if (!ba_PExpect(';', ctr)) {
			return 0;
		}

		return 1;
	}
	// ";"
	else if (ba_PAccept(';', ctr)) {
		return 1;
	}

	return 0;
}

/* program = { stmt } eof */
u8 ba_Parse(struct ba_Controller* ctr) {
	while (!ba_PAccept(BA_TK_EOF, ctr)) {
		if (!ba_PStmt(ctr)) {
			// doesn't have anything to do with literals, this is just 
			// a decent buffer size
			char msg[BA_LITERAL_SIZE+1];
			sprintf(msg, "unexpected %s at", ba_GetLexemeStr(ctr->lex->type));
			return ba_ExitMsg(0, msg, ctr->lex->line, ctr->lex->colStart);
		}
	}
	
	// Exit
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 60);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RBX, BA_IM_IMM, 0);
	ba_AddIM(ctr, 1, BA_IM_SYSCALL);

	return 1;
}

#endif
