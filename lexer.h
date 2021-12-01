// See LICENSE for copyright/license information

#ifndef BA__LEXER_H
#define BA__LEXER_H

#include "common/common.h"

int ba_Tokenize(FILE* srcFile, struct ba_Controller* ctr) {
	enum StateType {
		ST_NONE,
		ST_CMNT,
		ST_CMNT_SG,
		ST_CMNT_ML,
		ST_CMNT_MLHASH,
		ST_ID,
		ST_STR,
		ST_STR_ESC,
		ST_STR_XESC,
		ST_NUM_DEC,
		ST_NUM_HEX,
		ST_NUM_OCT,
		ST_NUM_BIN,
	}
	state = ST_NONE;

	char fileBuf[BA_FILE_BUF_SIZE] = {0};
	char litBuf[BA_LITERAL_SIZE+1] = {0};
	char idBuf[BA_IDENTIFIER_SIZE+1] = {0};
	u64 fileIter = 0, litIter = 0, idIter = 0, line = 1, col = 1, colStart = 1;
	struct ba_Lexeme* nextLex = ctr->startLex;
	
	while (fread(fileBuf, sizeof(char), BA_FILE_BUF_SIZE, srcFile)) {
		fileIter = 0;
		while (fileIter < BA_FILE_BUF_SIZE) {
			char c = fileBuf[fileIter];

			if ((c == EOF) || (c == 0)) {
				break;
			}

			// Starts of lexemes
			if (state == ST_NONE) {
				// Comments
				if (c == '#') {
					state = ST_CMNT;
					++col;
					++fileIter;
					continue;
				}

				// Identifiers
				else if ((c == '_') || ((c >= 'a') && (c <= 'z')) || 
					((c >= 'A') && (c <= 'Z')))
				{
					state = ST_ID;
					colStart = col;
					continue;
				}
				// String literals
				else if (c == '"') {
					state = ST_STR;
					colStart = col++;
					++fileIter;
					continue;
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
						++col;
						++fileIter;
						continue;
					}
					else if ((c == 'o') || (c == 'O')) {
						litBuf[litIter++] = c;
						state = ST_NUM_OCT;
						++col;
						++fileIter;
						continue;
					}
					else if ((c == 'b') || (c == 'B')) {
						litBuf[litIter++] = c;
						state = ST_NUM_BIN;
						++col;
						++fileIter;
						continue;
					}
					else if ((c >= '0') && (c <= '9')) {
						state = ST_NUM_DEC;
						continue;
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
						state = ST_NONE;

						if (!((c == 'u') || (c == 'U'))) {
							continue;
						}
					}
				}
				else if ((c >= '1') && (c <= '9')) {
					state = ST_NUM_DEC;
					colStart = col;
					continue;
				}
				// Other characters
				else if (!((c == ' ') || (c == '\t') || (c == '\v') || 
					(c == '\f') || (c == '\r')))
				{
					u8 needsToIterate = 1;

					if (c == '\n') {
						++line;
						col = 1;
						++fileIter;
						continue;
					}
					else if (c == ';' || c == '!' || c == '~' || c == '@' || 
						c == '(' || c == ')' || c == '=')
					{
						nextLex->type = c;
					}
					else {
						char oldC = c;
						colStart = col++;
						c = fileBuf[++fileIter];

						if (oldC == '+') {
							((c == '=') && (nextLex->type = BA_TK_ADDEQ)) || 
							((nextLex->type = '+') && (needsToIterate = 0));
						}
						else if (oldC == '-') {
							((c == '=') && (nextLex->type = BA_TK_SUBEQ)) || 
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
								"character at", line, col);
						}
					}

					nextLex->line = line;
					nextLex->colStart = col;
					nextLex->next = ba_NewLexeme();
					nextLex = nextLex->next;

					if (needsToIterate) {
						++col;
						++fileIter;
					}

					if ((c == EOF) || (c == 0)) {
						break;
					}
					else {
						continue;
					}
				}
			}
			// Comments
			else if (state == ST_CMNT) {
				if (c == '#') {
					state = ST_CMNT_ML;
				}
				else {
					state = ST_CMNT_SG;
					continue;
				}
			}
			else if (state == ST_CMNT_SG) {
				if (c == '\n') {
					++line;
					col = 1;
					++fileIter;
					state = ST_NONE;
					continue;
				}
			}
			else if (state == ST_CMNT_ML) {
				if (c == '\n') {
					++line;
					col = 1;
					++fileIter;
					continue;
				}
				else if (c == '#') {
					state = ST_CMNT_MLHASH;
				}
			}
			else if (state == ST_CMNT_MLHASH) {
				if (c == '\n') {
					++line;
					col = 1;
					++fileIter;
					state = ST_CMNT_ML;
					continue;
				}
				else if (c == '#') {
					state = ST_NONE;
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
						return ba_ErrorTknOverflow("identifier", line, colStart, 
							BA_IDENTIFIER_SIZE);
					}

					idBuf[idIter++] = c;
					++col;
					++fileIter;
					continue;
				}
				if (idIter > 0) {
					idBuf[idIter] = 0;
					
					nextLex->line = line;
					nextLex->colStart = colStart;

					if (!strcmp(idBuf, "write")) {
						nextLex->type = BA_TK_KW_WRITE;
					}
					else if (!strcmp(idBuf, "u64")) {
						nextLex->type = BA_TK_KW_U64;
					}
					else if (!strcmp(idBuf, "i64")) {
						nextLex->type = BA_TK_KW_I64;
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
					state = ST_NONE;
					continue;
				}
			}
			// String literals
			else if (state == ST_STR) {
				if (litIter > BA_LITERAL_SIZE) {
					return ba_ErrorTknOverflow("string literal", line, colStart, 
						BA_LITERAL_SIZE);
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
					state = ST_NONE;
				}
				else if (c == '\n') {
					litBuf[litIter++] = c;
					++line;
					col = 1;
					++fileIter;
					continue;
				}
				else if (c == '\\') {
					++col;
					++fileIter;
					state = ST_STR_ESC;
					continue;
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
					continue;
				}
				else if (c == 'x') {
					++col;
					++fileIter;
					state = ST_STR_XESC;
					continue;
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
					return ba_ExitMsg(BA_EXIT_ERR, "invalid escape sequence at", 
						line, col);
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
						line, col);
				}

				litBuf[litIter++] = val;

				state = ST_STR;
			}
			// Decimal number literals
			else if (state == ST_NUM_DEC) {
				if ((c >= '0') && (c <= '9')) {
					if (litIter > BA_LITERAL_SIZE) {
						return ba_ErrorTknOverflow("string literal", line, colStart,
							BA_LITERAL_SIZE);
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
					state = ST_NONE;

					if (!((c == 'u') || (c == 'U'))) {
						continue;
					}
				}
			}

			// Hexadecimal number literals
			else if (state == ST_NUM_HEX) {
				if (((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'f')) || 
					((c >= 'A') && (c <= 'F')))
				{
					if (litIter > BA_LITERAL_SIZE) {
						return ba_ErrorTknOverflow("string literal", line, colStart,
							BA_LITERAL_SIZE);
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
					state = ST_NONE;

					if (!((c == 'u') || (c == 'U'))) {
						continue;
					}
				}
			}
			// Octal number literals
			else if (state == ST_NUM_OCT) {
				if ((c >= '0') && (c <= '7')) {
					if (litIter > BA_LITERAL_SIZE) {
						return ba_ErrorTknOverflow("integer literal", line, colStart,
							BA_LITERAL_SIZE);
					}

					litBuf[litIter++] = c;
				}
				else if (c == '_') {
					// Ignore
				}
				else if ((c >= '8') && (c <= '9')) {
					return ba_ErrorIntLitChar(line, col);
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
					state = ST_NONE;

					if (!((c == 'u') || (c == 'U'))) {
						continue;
					}
				}
			}
			// Binary number literals
			else if (state == ST_NUM_BIN) {
				if ((c == '0') || (c == '1')) {
					if (litIter > BA_LITERAL_SIZE) {
						return ba_ErrorTknOverflow("string literal", line, colStart,
							BA_LITERAL_SIZE);
					}

					litBuf[litIter++] = c;
				}
				else if (c == '_') {
					// Ignore
				}
				else if ((c >= '2') && (c <= '9')) {
					return ba_ErrorIntLitChar(line, col);
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
					state = ST_NONE;

					if (!((c == 'u') || (c == 'U'))) {
						continue;
					}
				}
			}
			
			// Iterate
			++col;
			++fileIter;
		}
	}

	if (state != ST_NONE) {
		printf("Error: unexpected end of input\n");
		return 0;
	}
	
	return 1;
}

#endif
