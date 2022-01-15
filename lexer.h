// See LICENSE for copyright/license information

#ifndef BA__LEXER_H
#define BA__LEXER_H

#include "common/common.h"

u8 ba_Tokenize(FILE* srcFile, struct ba_Controller* ctr) {
	enum {
		ST_NONE = 0,
		ST_CMNT,
		ST_CMNT_SG,
		ST_CMNT_ML,
		ST_CMNT_MLBRAC,
		ST_ID,
		ST_STR,
		ST_STR_ESC,
		ST_STR_XESC,
		ST_NUM_DEC,
		ST_NUM_HEX,
		ST_NUM_OCT,
		ST_NUM_BIN,
	}
	state = 0;

	char fileBuf[BA_FILE_BUF_SIZE] = {0};
	char litBuf[BA_LITERAL_SIZE+1] = {0};
	char idBuf[BA_IDENTIFIER_SIZE+1] = {0};
	u64 fileIter = 0, litIter = 0, idIter = 0, line = 1, col = 1, colStart = 1;
	struct ba_Lexeme* nextLex = ctr->lex;
	
	while (fread(fileBuf, sizeof(char), BA_FILE_BUF_SIZE, srcFile)) {
		fileIter = 0;
		while (fileIter < BA_FILE_BUF_SIZE) {
			char c = fileBuf[fileIter];

			if ((c == EOF) || (c == 0)) {
				break;
			}

			// Starts of lexemes
			if (!state) {
				// Comments
				if (c == '#') {
					state = ST_CMNT;
					goto BA_LBL_LEX_LOOPITER;
				}

				// Identifiers
				else if ((c == '_') || ((c >= 'a') && (c <= 'z')) || 
					((c >= 'A') && (c <= 'Z')))
				{
					state = ST_ID;
					colStart = col;
					goto BA_LBL_LEX_LOOPEND;
				}
				// String literals
				else if (c == '"') {
					state = ST_STR;
					colStart = col;
					goto BA_LBL_LEX_LOOPITER;
				}
				// Number literals
				else if (c == '0') {
					colStart = col++;
					litBuf[litIter++] = c;

					c = fileBuf[++fileIter];

					while (c == '_') {
						c = fileBuf[++fileIter];
					}

					if ((c == EOF) || (c == 0)) {
						break;
					}

					if ((c == 'x') || (c == 'X')) {
						litBuf[litIter++] = c;
						state = ST_NUM_HEX;
						goto BA_LBL_LEX_LOOPITER;
					}
					else if ((c == 'o') || (c == 'O')) {
						litBuf[litIter++] = c;
						state = ST_NUM_OCT;
						goto BA_LBL_LEX_LOOPITER;
					}
					else if ((c == 'b') || (c == 'B')) {
						litBuf[litIter++] = c;
						state = ST_NUM_BIN;
						goto BA_LBL_LEX_LOOPITER;
					}
					else if ((c >= '0') && (c <= '9')) {
						state = ST_NUM_DEC;
						goto BA_LBL_LEX_LOOPEND;
					}
					else {
						if ((c == 'u') || (c == 'U')) {
							litBuf[litIter++] = 'u';
						}

						litBuf[litIter] = 0;

						nextLex->line = line;
						nextLex->colStart = colStart;
						nextLex->val = malloc(BA_LITERAL_SIZE+1);
						if (!nextLex->val) {
							return ba_ErrorMallocNoMem();
						}
						nextLex->valLen = litIter;
						strcpy(nextLex->val, litBuf);
						nextLex->type = BA_TK_LITINT;

						nextLex->next = ba_NewLexeme();
						nextLex = nextLex->next;
						
						litIter = 0;
						state = 0;

						if (!((c == 'u') || (c == 'U'))) {
							goto BA_LBL_LEX_LOOPEND;
						}
					}
				}
				else if ((c >= '1') && (c <= '9')) {
					state = ST_NUM_DEC;
					colStart = col;
					goto BA_LBL_LEX_LOOPEND;
				}
				// Other characters
				else if (!((c == ' ') || (c == '\t') || (c == '\v') || 
					(c == '\f') || (c == '\r')))
				{
					bool needsToIterate = 1;

					if (c == '\n') {
						++line;
						col = 1;
						++fileIter;
						goto BA_LBL_LEX_LOOPEND;
					}
					else if (c == ';' || c == ':' || c == '~' || c == '$' || 
						c == '(' || c == ')' || c == '{' || c == '}' || 
						c == '[' || c == ']' || c == ',') 
					{
						nextLex->type = c;
					}
					else {
						char oldC = c;
						colStart = col++;
						c = fileBuf[++fileIter];

						if (oldC == '=') {
							((c == '=') && (nextLex->type = BA_TK_DBEQUAL)) || 
							((nextLex->type = '=') && (needsToIterate = 0));
						}
						else if (oldC == '!') {
							((c == '=') && (nextLex->type = BA_TK_NEQUAL)) || 
							((nextLex->type = '!') && (needsToIterate = 0));
						}
						else if (oldC == '+') {
							((c == '=') && (nextLex->type = BA_TK_ADDEQ)) || 
							((c == '+') && (nextLex->type = BA_TK_INC)) || 
							((nextLex->type = '+') && (needsToIterate = 0));
						}
						else if (oldC == '-') {
							((c == '=') && (nextLex->type = BA_TK_SUBEQ)) || 
							((c == '-') && (nextLex->type = BA_TK_DEC)) || 
							((nextLex->type = '-') && (needsToIterate = 0));
						}
						else if (oldC == '*') {
							((c == '=') && (nextLex->type = BA_TK_MULEQ)) || 
							((nextLex->type = '*') && (needsToIterate = 0));
						}
						else if (oldC == '%') {
							((c == '=') && (nextLex->type = BA_TK_MODEQ)) || 
							((nextLex->type = '%') && (needsToIterate = 0));
						}
						else if (oldC == '^') {
							((c == '=') && (nextLex->type = BA_TK_BITXOREQ)) || 
							((nextLex->type = '^') && (needsToIterate = 0));
						}
						else if (oldC == '&') {
							((c == '=') && (nextLex->type = BA_TK_BITANDEQ)) || 
							((c == '&') && (nextLex->type = BA_TK_LOGAND)) || 
							((nextLex->type = '&') && (needsToIterate = 0));
						}
						else if (oldC == '|') {
							((c == '=') && (nextLex->type = BA_TK_BITOREQ)) || 
							((c == '|') && (nextLex->type = BA_TK_LOGOR)) || 
							((nextLex->type = '|') && (needsToIterate = 0));
						}
						else if (oldC == '>') {
							if (c == '=') {
								nextLex->type = BA_TK_GTE;
							}
							else if (c == '>') {
								colStart = col++;
								c = fileBuf[++fileIter];
								
								if (c == '=') {
									nextLex->type = BA_TK_RSHIFTEQ;
								}
								else {
									nextLex->type = BA_TK_RSHIFT;
									needsToIterate = 0;
								}
							}
							else {
								nextLex->type = '>';
								needsToIterate = 0;
							}
						}
						else if (oldC == '<') {
							if (c == '=') {
								nextLex->type = BA_TK_LTE;
							}
							else if (c == '<') {
								colStart = col++;
								c = fileBuf[++fileIter];
								
								if (c == '=') {
									nextLex->type = BA_TK_LSHIFTEQ;
								}
								else {
									nextLex->type = BA_TK_LSHIFT;
									needsToIterate = 0;
								}
							}
							else {
								nextLex->type = '<';
								needsToIterate = 0;
							}
						}
						else if (oldC == '/') {
							if (c == '=') {
								nextLex->type = BA_TK_FDIVEQ;
							}
							else if (c == '/') {
								colStart = col++;
								c = fileBuf[++fileIter];
								
								if (c == '=') {
									nextLex->type = BA_TK_IDIVEQ;
								}
								else {
									nextLex->type = BA_TK_IDIV;
									needsToIterate = 0;
								}
							}
							else {
								nextLex->type = '/';
								needsToIterate = 0;
							}
						}
						else {
							return ba_ExitMsg(BA_EXIT_ERR, "encountered invalid "
								"character at", line, col, ctr->currPath);
						}
					}

					nextLex->line = line;
					nextLex->colStart = col;
					nextLex->next = ba_NewLexeme();
					nextLex = nextLex->next;

					if ((c == EOF) || (c == 0)) {
						break;
					}

					if (needsToIterate) {
						goto BA_LBL_LEX_LOOPITER;
					}

					goto BA_LBL_LEX_LOOPEND;
				}
			}
			// Comments
			else if (state == ST_CMNT) {
				if (c == '{') {
					state = ST_CMNT_ML;
				}
				else {
					state = ST_CMNT_SG;
					goto BA_LBL_LEX_LOOPEND;
				}
			}
			else if (state == ST_CMNT_SG) {
				if (c == '\n') {
					++line;
					col = 1;
					++fileIter;
					state = 0;
					goto BA_LBL_LEX_LOOPEND;
				}
			}
			else if (state == ST_CMNT_ML) {
				if (c == '\n') {
					++line;
					col = 1;
					++fileIter;
					goto BA_LBL_LEX_LOOPEND;
				}
				else if (c == '}') {
					state = ST_CMNT_MLBRAC;
				}
			}
			else if (state == ST_CMNT_MLBRAC) {
				if (c == '\n') {
					++line;
					col = 1;
					++fileIter;
					state = ST_CMNT_ML;
					goto BA_LBL_LEX_LOOPEND;
				}
				else if (c == '#') {
					state = 0;
				}
				else {
					state = ST_CMNT_ML;
				}
			}
			// Identifiers
			else if (state == ST_ID) {
				if ((c == '_') || ((c >= 'a') && (c <= 'z')) || 
					((c >= 'A') && (c <= 'Z')) ||
					((c >= '0') && (c <= '9')))
				{
					if (idIter > BA_IDENTIFIER_SIZE) {
						return ba_ErrorTknOverflow("identifier", line, 
							colStart, ctr->currPath, BA_IDENTIFIER_SIZE);
					}
					idBuf[idIter++] = c;
					goto BA_LBL_LEX_LOOPITER;
				}
				if (idIter > 0) {
					idBuf[idIter] = 0;
					
					nextLex->line = line;
					nextLex->colStart = colStart;
					
					if (!strcmp(idBuf, "break")) {
						nextLex->type = BA_TK_KW_BREAK;
					}
					else if (!strcmp(idBuf, "elif")) {
						nextLex->type = BA_TK_KW_ELIF;
					}
					else if (!strcmp(idBuf, "else")) {
						nextLex->type = BA_TK_KW_ELSE;
					}
					else if (!strcmp(idBuf, "goto")) {
						nextLex->type = BA_TK_KW_GOTO;
					}
					else if (!strcmp(idBuf, "i64")) {
						nextLex->type = BA_TK_KW_I64;
					}
					else if (!strcmp(idBuf, "if")) {
						nextLex->type = BA_TK_KW_IF;
					}
					else if (!strcmp(idBuf, "include")) {
						nextLex->type = BA_TK_KW_INCLUDE;
					}
					else if (!strcmp(idBuf, "return")) {
						nextLex->type = BA_TK_KW_RETURN;
					}
					else if (!strcmp(idBuf, "u64")) {
						nextLex->type = BA_TK_KW_U64;
					}
					else if (!strcmp(idBuf, "void")) {
						nextLex->type = BA_TK_KW_VOID;
					}
					else if (!strcmp(idBuf, "while")) {
						nextLex->type = BA_TK_KW_WHILE;
					}
					else if (!strcmp(idBuf, "write")) {
						nextLex->type = BA_TK_KW_WRITE;
					}
					else {
						nextLex->val = malloc(BA_LITERAL_SIZE+1);
						if (!nextLex->val) {
							return ba_ErrorMallocNoMem();
						}
						strcpy(nextLex->val, idBuf);
						nextLex->valLen = idIter;
						nextLex->type = BA_TK_IDENTIFIER;
					}

					nextLex->next = ba_NewLexeme();
					nextLex = nextLex->next;
					idIter = 0;
					state = 0;
					goto BA_LBL_LEX_LOOPEND;
				}
			}
			// String literals
			else if (state == ST_STR) {
				if (litIter > BA_LITERAL_SIZE) {
					return ba_ErrorTknOverflow("string literal", line, 
						colStart, ctr->currPath, BA_LITERAL_SIZE);
				}
				
				if (c == '"') {
					litBuf[litIter] = 0;
					
					nextLex->line = line;
					nextLex->colStart = colStart;
					
					nextLex->val = malloc(BA_LITERAL_SIZE+1);
					if (!nextLex->val) {
						return ba_ErrorMallocNoMem();
					}
					strcpy(nextLex->val, litBuf);
					nextLex->valLen = litIter;
					nextLex->type = BA_TK_LITSTR;

					nextLex->next = ba_NewLexeme();
					nextLex = nextLex->next;

					litIter = 0;
					state = 0;
				}
				else if (c == '\n') {
					litBuf[litIter++] = c;
					++line;
					col = 1;
					++fileIter;
					goto BA_LBL_LEX_LOOPEND;
				}
				else if (c == '\\') {
					state = ST_STR_ESC;
					goto BA_LBL_LEX_LOOPITER;
				}
				else {
					litBuf[litIter++] = c;
				}
			}
			// String literal escape characters
			else if (state == ST_STR_ESC) {
				if (c == '"') {
					litBuf[litIter++] = '"';
				}
				else if (c == '\'') {
					litBuf[litIter++] = '\'';
				}
				else if (c == '\\') {
					litBuf[litIter++] = '\\';
				}
				else if (c == 'n') {
					litBuf[litIter++] = '\n';
				}
				else if (c == 't') {
					litBuf[litIter++] = '\t';
				}
				else if (c == 'v') {
					litBuf[litIter++] = '\v';
				}
				else if (c == 'f') {
					litBuf[litIter++] = '\f';
				}
				else if (c == 'r') {
					litBuf[litIter++] = '\r';
				}
				else if (c == 'b') {
					litBuf[litIter++] = '\b';
				}
				else if (c == '0') {
					litBuf[litIter++] = '\0';
				}
				else if (c == '\n') {
					++line;
					col = 1;
					++fileIter;
					state = ST_STR;
					goto BA_LBL_LEX_LOOPEND;
				}
				else if (c == 'x') {
					state = ST_STR_XESC;
					goto BA_LBL_LEX_LOOPITER;
				}
				// TODO: add more escape patterns
				else {
					litBuf[litIter++] = '\\';
				}

				state = ST_STR;
			}
			// String literal hex escapes
			else if (state == ST_STR_XESC) {
				u8 val = 0;
				
				// Currently only works with 7-bit ascii

				// Add the first number
				if ((c >= '0') && (c <= '7')) {
					val += c - '0';
				}
				else {
					return ba_ExitMsg(BA_EXIT_ERR, "invalid escape "
						"sequence at", line, col, ctr->currPath);
				}

				val <<= 4;

				++col;
				c = fileBuf[++fileIter];

				if ((c == EOF) || (c == 0)) {
					break;
				}

				// Add the second number
				if ((c >= '0') && (c <= '9')) {
					val += c - '0';
				}
				else if ((c >= 'a') && (c <= 'f')) {
					val += c - 'a' + 10;
				}
				else if ((c >= 'A') && (c <= 'F')) {
					val += c - 'A' + 10;
				}
				else {
					return ba_ExitMsg(BA_EXIT_ERR, "invalid escape sequence at", 
						line, col, ctr->currPath);
				}

				litBuf[litIter++] = val;
				state = ST_STR;
			}
			// Number literals
			else if (state == ST_NUM_HEX || state == ST_NUM_DEC || 
				state == ST_NUM_OCT || state == ST_NUM_BIN) 
			{
				if ((state == ST_NUM_HEX && 
					((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || 
					(c >= 'A' && c <= 'F'))) || 
					(state == ST_NUM_DEC && c >= '0' && c <= '9') ||
					(state == ST_NUM_OCT && c >= '0' && c <= '7') ||
					(state == ST_NUM_BIN && c >= '0' && c <= '1'))
				{
					if (litIter > BA_LITERAL_SIZE) {
						return ba_ErrorTknOverflow("string literal", line, 
							colStart, ctr->currPath, BA_LITERAL_SIZE);
					}
					litBuf[litIter++] = c;
				}
				else if (c == '_') {
					// Ignore
				}
				else {
					if ((c == 'u') || (c == 'U')) {
						litBuf[litIter++] = 'u';
					}
					litBuf[litIter] = 0;

					nextLex->line = line;
					nextLex->colStart = colStart;
					nextLex->val = malloc(litIter+1);
					if (!nextLex->val) {
						return ba_ErrorMallocNoMem();
					}
					nextLex->valLen = litIter;
					strcpy(nextLex->val, litBuf);
					nextLex->type = BA_TK_LITINT;

					nextLex->next = ba_NewLexeme();
					nextLex = nextLex->next;

					litIter = 0;
					state = 0;

					if (!((c == 'u') || (c == 'U'))) {
						goto BA_LBL_LEX_LOOPEND;
					}
				}
			}
			
			BA_LBL_LEX_LOOPITER:
			++col;
			++fileIter;

			BA_LBL_LEX_LOOPEND:;
		}
	}

	if (state) {
		fprintf(stderr, "Error: unexpected end of input\n");
		return 0;
	}
	
	return 1;
}

#endif
