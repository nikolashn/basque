// See LICENSE for copyright/license information

#include "lexer.h"
#include "common/lexeme.h"
#include "common/exitmsg.h"
#include "common/format.h"

/* Returns 0 if unable to read more */
bool IncFile(u64* jPtr, char* f, FILE* srcFile) {
	if (++*jPtr >= BA_FILE_BUF_SIZE) {
		*jPtr = 0;
		return fread(f, sizeof(char), BA_FILE_BUF_SIZE, srcFile);
	}
	return 1;
}

/* Returns -1 for invalid escape or escape that cannot be handled alone */
u8 CharEscape(char c) {
	switch (c) {
		case '"': case '\'': case '\\':
			return c;
		case 'n': return '\n';
		case 't': return '\t';
		case 'v': return '\v';
		case 'f': return '\f';
		case 'b': return '\b';
		case 'a': return '\a';
		case 'e': return '\x1b';
		case '0': return '\0';
	}
	return -1;
}

u8 ErrorEOF() {
	fprintf(stderr, "Error: unexpected end of file\n");
	exit(1);
	return 0;
}

u8 HexCharToNum(char c, u64 line, u64 col, char* path) {
	if ('0' <= c && c <= '9') {
		return c - '0';
	}
	if ('a' <= c && c <= 'f') {
		return c - 'a';
	}
	if ('A' <= c && c <= 'F') {
		return c - 'A';
	}
	return ba_ExitMsg(BA_EXIT_ERR, "unexpected character in hexadecimal integer "
			"on", line, col, path);
}

u8 TryKeyword(char* buf, char* keyword, u64 lexType, struct ba_Lexeme* lex) {
	if (!strcmp(buf, keyword)) {
		lex->type = lexType;
		return 1;
	}
	return 0;
}

bool TokenizeWithState(struct ba_Ctr* ctr, FILE* srcFile, char* f, u64* jPtr, 
		u64* linePtr, u64* colPtr, struct ba_Lexeme* lex, bool isInFString)
{
	u64 j = *jPtr;
	u64 line = *linePtr;
	u64 col = *colPtr;

	bool willInc = 1;
	bool willCont = 1;

	while (!willInc || IncFile(&j, f, srcFile)) {
		char c = f[j];
		lex->line = line;
		lex->col = col;
		lex->val = 0;
		willInc = 1;
		
		if (!c) {
			break;
		}
		else if (isInFString && c == '}') {
			*jPtr = ++j;
			*linePtr = line;
			*colPtr = ++col;
			return 1;
		}
		else if (c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r') {
			++col;
			goto BA_LBL_LEXSKIP;
		}
		else if (c == '\n') {
			++line;
			col = 1;
			goto BA_LBL_LEXSKIP;
		}
		else if (c == '#') {
			IncFile(&j, f, srcFile) || ErrorEOF();
			++col;
			bool isMultiLine = 0;
			if (f[j] == '{') {
				isMultiLine = 1;
				++col;
			}
			else {
				willInc = 0;
			}

			while (!willInc || IncFile(&j, f, srcFile)) {
				willInc = 1;
				if (f[j] == '\n') {
					++line;
					col = 1;
				}
				if (f[j] == '#' && isMultiLine) {
					IncFile(&j, f, srcFile) || ErrorEOF();
					++col;
					if (f[j] == '}') {
						++col;
						goto BA_LBL_LEXSKIP;
					}
					willInc = 0;
				}
				else if (f[j] == '\n' && !isMultiLine) {
					goto BA_LBL_LEXSKIP;
				}
				else {
					++col;
				}
			}
			// Didn't finish
			ErrorEOF();
		}
		else if (c == '>') {
			willCont = IncFile(&j, f, srcFile);
			++col;
			if (f[j] == '>' && willCont) {
				willCont = IncFile(&j, f, srcFile);
				++col;
				if (f[j] == '=') {
					lex->type = BA_TK_RSHIFTEQ;
					++col;
				}
				else {
					lex->type = BA_TK_RSHIFT;
					willInc = 0;
				}
			}
			else if (f[j] == '=' && willCont) {
				lex->type = BA_TK_GTE;
				++col;
			}
			else {
				lex->type = '>';
				willInc = 0;
			}
		}
		else if (c == '<') {
			willCont = IncFile(&j, f, srcFile);
			++col;
			if (f[j] == '<' && willCont) {
				willCont = IncFile(&j, f, srcFile);
				++col;
				if (f[j] == '=') {
					lex->type = BA_TK_LSHIFTEQ;
					++col;
				}
				else {
					lex->type = BA_TK_LSHIFT;
					willInc = 0;
				}
			}
			else if (f[j] == '=' && willCont) {
				lex->type = BA_TK_LTE;
				++col;
			}
			else {
				lex->type = '<';
				willInc = 0;
			}
		}
		else if (c == '/') {
			willCont = IncFile(&j, f, srcFile);
			++col;
			if (f[j] == '/' && willCont) {
				willCont = IncFile(&j, f, srcFile);
				++col;
				if (f[j] == '=' && willCont) {
					lex->type = BA_TK_IDIVEQ;
					++col;
				}
				else {
					lex->type = BA_TK_IDIV;
					willInc = 0;
				}
			}
			else if (f[j] == '=' && willCont) {
				lex->type = BA_TK_FDIVEQ;
				++col;
			}
			else {
				lex->type = '/';
				willInc = 0;
			}
		}
		else if (c == '&') {
			willCont = IncFile(&j, f, srcFile);
			++col;
			if (f[j] == '&' && willCont) {
				lex->type = BA_TK_LOGAND;
				++col;
			}
			else if (f[j] == '=' && willCont) {
				lex->type = BA_TK_BITANDEQ;
				++col;
			}
			else {
				lex->type = '&';
				willInc = 0;
			}
		}
		else if (c == '^') {
			willCont = IncFile(&j, f, srcFile);
			++col;
			if (f[j] == '=' && willCont) {
				lex->type = BA_TK_BITXOREQ;
				++col;
			}
			else {
				lex->type = '^';
				willInc = 0;
			}
		}
		else if (c == '|') {
			willCont = IncFile(&j, f, srcFile);
			++col;
			if (f[j] == '|' && willCont) {
				lex->type = BA_TK_LOGOR;
				++col;
			}
			else if (f[j] == '=' && willCont) {
				lex->type = BA_TK_BITOREQ;
				++col;
			}
			else {
				lex->type = '|';
				willInc = 0;
			}
		}
		else if (c == '=') {
			willCont = IncFile(&j, f, srcFile);
			++col;
			if (f[j] == '=' && willCont) {
				lex->type = BA_TK_DBEQUAL;
				++col;
			}
			else {
				lex->type = '=';
				willInc = 0;
			}
		}
		else if (c == '!') {
			willCont = IncFile(&j, f, srcFile);
			++col;
			if (f[j] == '=' && willCont) {
				lex->type = BA_TK_NEQUAL;
				++col;
			}
			else {
				lex->type = '!';
				willInc = 0;
			}
		}
		else if (c == '+') {
			willCont = IncFile(&j, f, srcFile);
			++col;
			if ((f[j] == '=' || f[j] == '+') && willCont) {
				lex->type = (f[j] == '=') ? BA_TK_ADDEQ : BA_TK_INC;
				++col;
			}
			else {
				lex->type = '+';
				willInc = 0;
			}
		}
		else if (c == '-') {
			willCont = IncFile(&j, f, srcFile);
			++col;
			if ((f[j] == '=' || f[j] == '-') && willCont) {
				lex->type = (f[j] == '=') ? BA_TK_SUBEQ : BA_TK_DEC;
				++col;
			}
			else {
				lex->type = '-';
				willInc = 0;
			}
		}
		else if (c == '*') {
			willCont = IncFile(&j, f, srcFile);
			++col;
			if (f[j] == '=' && willCont) {
				lex->type = BA_TK_MULEQ;
				++col;
			}
			else {
				lex->type = '*';
				willInc = 0;
			}
		}
		else if (c == '%') {
			willCont = IncFile(&j, f, srcFile);
			++col;
			if (f[j] == '=' && willCont) {
				lex->type = BA_TK_MODEQ;
				++col;
			}
			else {
				lex->type = '%';
				willInc = 0;
			}
		}
		else if (c == ';' || c == '~' || c == '$' || c == '(' || c == ')' || 
			c == '[' || c == ']' || c == '{' || c == '}' || c == ',' || c == '.' ||
			c == ':') 
		{
			lex->type = c;
			++col;
		}
		else if (c == '"') {
			char* buf = ba_CAlloc(BA_LITERAL_SIZE + 1, sizeof(char));
			char* bufCurr = buf;
			++col;
			while (!willInc || IncFile(&j, f, srcFile)) {
				c = f[j];
				willInc = 1;
				if (c == '"') {
					++col;
					break;
				}
				if (c == '\\') {
					IncFile(&j, f, srcFile) || ErrorEOF();
					++col;
					c = CharEscape(f[j]);
					if (c == -1) {
						if (f[j] == '\n') {
							++line;
							col = 1;
							goto BA_LBL_STRSKIP;
						}
						else if (f[j] == 'x') {
							IncFile(&j, f, srcFile) || ErrorEOF();
							++col;
							c = HexCharToNum(f[j], line, col, ctr->currPath) << 4;
							IncFile(&j, f, srcFile) || ErrorEOF();
							++col;
							c += HexCharToNum(f[j], line, col, ctr->currPath);
						}
						else {
							c = '\\';
							willInc = 0;
						}
					}
					else {
						++col;
					}
				}
				else if (c == '\n') {
					++line;
					col = 1;
				}
				else {
					++col;
				}
				*bufCurr = c;
				++bufCurr;
				if (bufCurr - buf > BA_LITERAL_SIZE) {
					return ba_ErrorTknOverflow("string literal", line, col, 
							ctr->currPath, BA_LITERAL_SIZE);
				}
				BA_LBL_STRSKIP:;
			}
			lex->type = BA_TK_LITSTR;
			lex->val = buf;
			lex->valLen = bufCurr - buf;
		}
		else if (c >= '0' && c <= '9') {
			char* buf = ba_CAlloc(BA_LITERAL_SIZE + 1, sizeof(char));
			char* bufCurr = buf;
			u64 base = 10;

			willCont = IncFile(&j, f, srcFile);
			col += 2;
			*bufCurr = c;
			++bufCurr;

			if (f[j] == 'x' && willCont) {
				base = 16;
				*bufCurr = f[j];
				++bufCurr;
			}
			else if (f[j] == 'o' && willCont) {
				base = 8;
				*bufCurr = f[j];
				++bufCurr;
			}
			else if (f[j] == 'b' && willCont) {
				base = 2;
				*bufCurr = f[j];
				++bufCurr;
			}
			else {
				if (f[j] >= '0' && f[j] <= '9' && willCont) {
					*bufCurr = f[j];
					++bufCurr;
				}
				else {
					--col;
					willInc = 0;
				}
			}
			
			while (!willInc || IncFile(&j, f, srcFile)) {
				c = f[j];
				willInc = 1;
				++col;
				if (c == '_') {
					goto BA_LBL_INTSKIP;
				}
				else if ((c >= '2' && c <= '7' && base < 8) ||
					(c >= '8' && c == '9' && base < 10) ||
					(c >= 'a' && c <= 'f' && base < 16) ||
					(c >= 'A' && c <= 'F' && base < 16) || 
					(buf == bufCurr && (c == 'u' || c == 'U'))) 
				{
					return ba_ErrorIntLitChar(line, col, ctr->currPath);
				}
				else if (c == 'u' || c == 'U') {
					*bufCurr = 'u';
					++bufCurr;
					break;
				}
				else if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f') && 
					!(c >= 'A' && c <= 'F')) 
				{
					--col;
					willInc = 0;
					break;
				}
				*bufCurr = c;
				++bufCurr;
				if (bufCurr - buf > BA_LITERAL_SIZE) {
					return ba_ErrorTknOverflow("integer literal", line, col, 
							ctr->currPath, BA_LITERAL_SIZE);
				}
				BA_LBL_INTSKIP:;
			}
			lex->type = BA_TK_LITINT;
			lex->val = buf;
			lex->valLen = bufCurr - buf;
		}
		else if (c == '\'') {
			IncFile(&j, f, srcFile) || ErrorEOF();
			++col;
			c = f[j];
			(c == '\'') && ba_ErrorCharLit(line, col, ctr->currPath);
			if (c == '\\') {
				IncFile(&j, f, srcFile) || ErrorEOF();
				++col;
				c = CharEscape(f[j]);
				if (c == -1) {
					if (f[j] == '\n') {
						ba_ErrorCharLit(line, col, ctr->currPath);
					}
					else if (f[j] == 'x') {
						IncFile(&j, f, srcFile) || ErrorEOF();
						++col;
						c = HexCharToNum(f[j], line, col, ctr->currPath);
						IncFile(&j, f, srcFile) || ErrorEOF();
						++col;
						c += HexCharToNum(f[j], line, col, ctr->currPath) << 4;
					}
					else {
						c = '\\';
						willInc = 0;
					}
				}
				else {
					++col;
				}
			}
			else if (c == '\n') {
				++line;
				col = 1;
			}
			else {
				++col;
			}
			IncFile(&j, f, srcFile) || ErrorEOF();
			(f[j] != '\'') && ba_ErrorCharLit(line, col, ctr->currPath);
			++col;
			lex->type = BA_TK_LITCHAR;
			lex->val = (void*)(u64)c;
		}
		else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
			char* buf = ba_CAlloc(BA_IDENTIFIER_SIZE + 1, sizeof(char));
			char* bufCurr = buf;
			willCont = IncFile(&j, f, srcFile);
			++col;
			
			if ((c == 'f' || c == 'F') && f[j] == '"' && willCont) {
				// Formatted strings
				free(buf);
				++col;

				struct ba_FStr* fstrFirst = ba_MAlloc(sizeof(*fstrFirst));
				struct ba_FStr* fstr = fstrFirst;
				char autoBuf[BA_LITERAL_SIZE] = {0};
				bufCurr = autoBuf;

				while (!willInc || IncFile(&j, f, srcFile)) {
					c = f[j];
					willInc = 1;
					if (c == '"') {
						++col;
						*bufCurr = 0;
						*fstr = (struct ba_FStr){ .val = ba_MAlloc(bufCurr - autoBuf), 
							.len = bufCurr - autoBuf, .next = 0, .formatType = 0 };
						memcpy(fstr->val, autoBuf, fstr->len);
						fstrFirst->last = fstr;
						break;
					}
					if (c == '\\') {
						IncFile(&j, f, srcFile) || ErrorEOF();
						++col;
						c = CharEscape(f[j]);
						if (c == -1) {
							if (f[j] == '\n') {
								++line;
								col = 1;
								goto BA_LBL_FSTRSKIP;
							}
							else if (f[j] == 'x') {
								IncFile(&j, f, srcFile) || ErrorEOF();
								++col;
								c = HexCharToNum(f[j], line, col, ctr->currPath);
								IncFile(&j, f, srcFile) || ErrorEOF();
								++col;
								c += HexCharToNum(f[j], line, col, ctr->currPath) << 4;
							}
							else {
								c = '\\';
								willInc = 0;
							}
						}
						else {
							++col;
						}
					}
					else if (c == '%') {
						if (bufCurr != autoBuf) {
							*bufCurr = 0;
							*fstr = (struct ba_FStr){ .val = ba_MAlloc(bufCurr - autoBuf), 
								.len = bufCurr - autoBuf, .formatType = 0, 
								.next = ba_MAlloc(sizeof(*fstr->next)), };
							memcpy(fstr->val, autoBuf, fstr->len);
							fstr = fstr->next;
						}
						IncFile(&j, f, srcFile) || ErrorEOF();
						col += 2;
						if (f[j] == '%') {
							c = '%';
						}
						else if (f[j] == '{') {
							c = '{';
						}
						else if (f[j] == '}') {
							c = '}';
						}
						else {
							switch (f[j]) {
								case 'i': fstr->formatType = BA_FTYPE_I64;
								          break;
								case 'u': fstr->formatType = BA_FTYPE_U64;
								          break;
								case 'x': fstr->formatType = BA_FTYPE_HEX;
								          break;
								case 'o': fstr->formatType = BA_FTYPE_OCT;
								          break;
								case 'b': fstr->formatType = BA_FTYPE_BIN;
								          break;
								case 'c': fstr->formatType = BA_FTYPE_CHAR;
								          break;
								case 's': fstr->formatType = BA_FTYPE_STR;
								          break;
								default:  c = '%';
								          --col;
								          willInc = 0;
													goto BA_LBL_FSTREPILOGUE;
							}
							IncFile(&j, f, srcFile) || ErrorEOF();
							col += 2;
							(f[j] != '{') &&
								ba_ErrorFString(line, col, ctr->currPath, ", expected '{'");

							if (fstr->formatType == BA_FTYPE_STR) {
								struct ba_Lexeme* fstrLen = ba_MAlloc(sizeof(*fstrLen));
								if (!TokenizeWithState(ctr, srcFile, f, &j, &line, &col, 
									fstrLen, /* isInFString = */ 1))
								{
									return 0;
								}
								fstr->len = (u64)fstrLen;
								(f[j] != '{') &&
									ba_ErrorFString(line, col, ctr->currPath, ", expected '{'");
							}
							
							fstr->val = ba_MAlloc(sizeof(struct ba_Lexeme));
							if (!TokenizeWithState(ctr, srcFile, f, &j, &line, &col, 
								fstr->val, /* isInFString = */ 1))
							{
								return 0;
							}

							bufCurr = autoBuf;
							fstr->next = ba_MAlloc(sizeof(*fstr->next));
							fstr = fstr->next;
							willInc = 0;
							goto BA_LBL_FSTRSKIP;
						}
					}
					else if (c == '\n') {
						++line;
						col = 1;
					}
					else {
						++col;
					}
					BA_LBL_FSTREPILOGUE:
					*bufCurr = c;
					++bufCurr;
					if (bufCurr - autoBuf > BA_LITERAL_SIZE) {
						fstr->val = ba_MAlloc(BA_LITERAL_SIZE);
						fstr->len = BA_LITERAL_SIZE;
						fstr->next = ba_MAlloc(sizeof(*fstr->next));
						fstr->formatType = 0;
						memcpy(fstr->val, autoBuf, BA_LITERAL_SIZE);
						bufCurr = autoBuf;
						fstr = fstr->next;
					}
					BA_LBL_FSTRSKIP:;
				}

				lex->type = BA_TK_FSTRING;
				lex->val = (void*)fstrFirst;
				lex->next = ba_NewLexeme();
				lex = lex->next;
				goto BA_LBL_LEXSKIP;
			}
			
			++col;
			*bufCurr = c;
			++bufCurr;

			if (((f[j] >= 'a' && f[j] <= 'z') || (f[j] >= 'A' && f[j] <= 'Z') || 
				(f[j] >= '0' && f[j] <= '9') || f[j] == '_') && willCont)
			{
				*bufCurr = f[j];
				++bufCurr;
			}
			else {
				willInc = 0;
			}

			// Identifiers and keywords
			while (!willInc || IncFile(&j, f, srcFile)) {
				willInc = 1;
				if (!(f[j] >= 'a' && f[j] <= 'z') && !(f[j] >= 'A' && f[j] <= 'Z') && 
					!(f[j] >= '0' && f[j] <= '9') && f[j] != '_')
				{
					willInc = 0;
					break;
				}
				++col;
				*bufCurr = f[j];
				++bufCurr;
				if (bufCurr - buf > BA_IDENTIFIER_SIZE) {
					return ba_ErrorTknOverflow("identifier", line, col, 
							ctr->currPath, BA_IDENTIFIER_SIZE);
				}
			}

			bool isKeyword = 
				TryKeyword(buf, "if", BA_TK_KW_IF, lex) || 
				TryKeyword(buf, "i8", BA_TK_KW_I8, lex) || 
				TryKeyword(buf, "u8", BA_TK_KW_U8, lex) || 
				TryKeyword(buf, "i64", BA_TK_KW_I64, lex) || 
				TryKeyword(buf, "u64", BA_TK_KW_U64, lex) || 
				TryKeyword(buf, "bool", BA_TK_KW_BOOL, lex) ||
				TryKeyword(buf, "elif", BA_TK_KW_ELIF, lex) || 
				TryKeyword(buf, "else", BA_TK_KW_ELSE, lex) || 
				TryKeyword(buf, "exit", BA_TK_KW_EXIT, lex) || 
				TryKeyword(buf, "iter", BA_TK_KW_ITER, lex) || 
				TryKeyword(buf, "goto", BA_TK_KW_GOTO, lex) || 
				TryKeyword(buf, "void", BA_TK_KW_VOID, lex) ||
				TryKeyword(buf, "while", BA_TK_KW_WHILE, lex) || 
				TryKeyword(buf, "break", BA_TK_KW_BREAK, lex) || 
				TryKeyword(buf, "return", BA_TK_KW_RETURN, lex) || 
				TryKeyword(buf, "assert", BA_TK_KW_ASSERT, lex) || 
				TryKeyword(buf, "include", BA_TK_KW_INCLUDE, lex) || 
				TryKeyword(buf, "garbage", BA_TK_KW_GARBAGE, lex) || 
				TryKeyword(buf, "lengthof", BA_TK_KW_LENGTHOF, lex);

			if (isKeyword) {
				free(buf);
			}
			else {
				lex->type = BA_TK_IDENTIFIER;
				lex->val = buf;
				lex->valLen = bufCurr - buf;
			}
		}
		else {
			ba_ExitMsg(BA_EXIT_ERR, "invalid character on", line, col, ctr->currPath);
		}

		lex->next = ba_NewLexeme();
		lex = lex->next;
		
		BA_LBL_LEXSKIP:

		if (!willCont) {
			break;
		}
	}
	return !isInFString;
}

/* Returns 0 on error and 1 on success. */
bool ba_Tokenize(struct ba_Ctr* ctr, FILE* srcFile) {
	char f[BA_FILE_BUF_SIZE] = {0}; // File buffer
	u64 j = BA_FILE_BUF_SIZE - 1; // File buffer iterator
	u64 line = 1;
	u64 col = 1;
	struct ba_Lexeme* lex = ctr->lex;
	return TokenizeWithState(ctr, srcFile, f, &j, &line, &col, lex, 
		/* isInFString = */ 0);
}

