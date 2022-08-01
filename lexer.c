// See LICENSE for copyright/license information

#include "lexer.h"

u64 ba_LexerSafeIncFileIter(u64* fileIterPtr) {
	u64 fileIter = *fileIterPtr;
	++fileIter;
	if (fileIter >= BA_FILE_BUF_SIZE) {
		fileIter = 0;
	}
	*fileIterPtr = fileIter;
	return fileIter;
}

char ba_LexerEscSequence(struct ba_Ctr* ctr, FILE* srcFile, char* fileBuf, 
	u64* colPtr, u64* linePtr, u64* fileIterPtr) 
{
	char ret = 0;

	u64 col = *colPtr;
	u64 line = *linePtr;
	u64 fileIter = *fileIterPtr;

	char c = fileBuf[fileIter];
	++col;

	switch (c) {
		case '"':  ret = '"';  break;
		case '\'': ret = '\''; break;
		case '\\': ret = '\\'; break;
		case 'n':  ret = '\n'; break;
		case 't':  ret = '\t'; break;
		case 'v':  ret = '\v'; break;
		case 'f':  ret = '\f'; break;
		case 'r':  ret = '\r'; break;
		case 'b':  ret = '\b'; break;
	}

	if (ret) {
		++col;
		ba_LexerSafeIncFileIter(&fileIter) &&
			fread(fileBuf, sizeof(char), BA_FILE_BUF_SIZE, srcFile);
	}
	else if (c == '0') {
		ret = 0;
		++col;
		ba_LexerSafeIncFileIter(&fileIter) &&
			fread(fileBuf, sizeof(char), BA_FILE_BUF_SIZE, srcFile);
	}
	else if (c == 'x') {
		u8 val = 0;
		// Note: Currently only works with 7-bit ascii
		
		// Add the first number
		ba_LexerSafeIncFileIter(&fileIter) &&
			fread(fileBuf, sizeof(char), BA_FILE_BUF_SIZE, srcFile);
		c = fileBuf[fileIter];
		++col;
		if ((c < '0') || (c > '7')) {
			return ba_ExitMsg(BA_EXIT_ERR, "invalid escape sequence at", 
				line, col, ctr->currPath);
		}
		val += c - '0';
		val <<= 4;

		if ((c == EOF) || (c == 0)) {
			return ba_ExitMsg(BA_EXIT_ERR, "invalid escape sequence at", 
				line, col, ctr->currPath);
		}

		// Add the second number
		ba_LexerSafeIncFileIter(&fileIter) &&
			fread(fileBuf, sizeof(char), BA_FILE_BUF_SIZE, srcFile);
		c = fileBuf[fileIter];
		++col;
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

		ret = val;
		++col;
		ba_LexerSafeIncFileIter(&fileIter) &&
			fread(fileBuf, sizeof(char), BA_FILE_BUF_SIZE, srcFile);
	}
	// TODO: add more escape patterns
	else {
		ret = '\\';
	}

	*colPtr = col;
	*linePtr = line;
	*fileIterPtr = fileIter;
	return ret;
}

u8 ba_Tokenize(FILE* srcFile, struct ba_Ctr* ctr) {
	enum {
		ST_NONE = 0,
		ST_CMNT,
		ST_CMNT_SG,
		ST_CMNT_ML,
		ST_CMNT_MLBRAC,
		ST_ID,
		ST_STR,
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
				// Char literals
				else if (c == '\'') {
					colStart = col++;
					char charVal = 0;

					ba_LexerSafeIncFileIter(&fileIter) &&
						fread(fileBuf, sizeof(char), BA_FILE_BUF_SIZE, srcFile);
					c = fileBuf[fileIter];
					if (c == '\n') {
						++line;
						col = 1;
						charVal = c;
						ba_LexerSafeIncFileIter(&fileIter) &&
							fread(fileBuf, sizeof(char), 
								BA_FILE_BUF_SIZE, srcFile);
					}
					else if (c == '\\') {
						ba_LexerSafeIncFileIter(&fileIter) &&
							fread(fileBuf, sizeof(char), 
								BA_FILE_BUF_SIZE, srcFile);
						charVal = ba_LexerEscSequence(ctr, srcFile, fileBuf, 
							&col, &line, &fileIter);
					}
					else {
						charVal = c;
						++col;
						ba_LexerSafeIncFileIter(&fileIter) &&
							fread(fileBuf, sizeof(char), 
								BA_FILE_BUF_SIZE, srcFile);
					}

					c = fileBuf[fileIter];
					if (c != '\'') {
						return ba_ExitMsg(BA_EXIT_ERR, "encountered invalid "
							"character literal at", line, col, ctr->currPath);
					}

					nextLex->line = line;
					nextLex->colStart = colStart;
					nextLex->val = ba_MAlloc(1);
					nextLex->valLen = 1;
					*nextLex->val = charVal;
					nextLex->type = BA_TK_LITCHAR;

					nextLex->next = ba_NewLexeme();
					nextLex = nextLex->next;
					litIter = 0;
				}
				// Number literals
				else if (c == '0') {
					colStart = col++;
					litBuf[litIter++] = c;

					ba_LexerSafeIncFileIter(&fileIter) &&
						fread(fileBuf, sizeof(char), BA_FILE_BUF_SIZE, srcFile);
					c = fileBuf[fileIter];

					while (c == '_') {
						ba_LexerSafeIncFileIter(&fileIter) &&
							fread(fileBuf, sizeof(char), 
							BA_FILE_BUF_SIZE, srcFile);
						c = fileBuf[fileIter];
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
						nextLex->val = ba_MAlloc(BA_LITERAL_SIZE+1);
						nextLex->valLen = litIter;
						strcpy(nextLex->val, litBuf);
						nextLex->type = BA_TK_LITINT;

						nextLex->next = ba_NewLexeme();
						nextLex = nextLex->next;
						
						litIter = 0;

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
						ba_LexerSafeIncFileIter(&fileIter) &&
							fread(fileBuf, sizeof(char), 
								BA_FILE_BUF_SIZE, srcFile);
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
						ba_LexerSafeIncFileIter(&fileIter) &&
							fread(fileBuf, sizeof(char), 
								BA_FILE_BUF_SIZE, srcFile);
						c = fileBuf[fileIter];

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
								ba_LexerSafeIncFileIter(&fileIter) &&
									fread(fileBuf, sizeof(char), 
										BA_FILE_BUF_SIZE, srcFile);
								c = fileBuf[fileIter];
								
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
								ba_LexerSafeIncFileIter(&fileIter) &&
									fread(fileBuf, sizeof(char), 
										BA_FILE_BUF_SIZE, srcFile);
								c = fileBuf[fileIter];
								
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
								ba_LexerSafeIncFileIter(&fileIter) &&
									fread(fileBuf, sizeof(char), 
										BA_FILE_BUF_SIZE, srcFile);
								c = fileBuf[fileIter];
								
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
					ba_LexerSafeIncFileIter(&fileIter) &&
						fread(fileBuf, sizeof(char), BA_FILE_BUF_SIZE, srcFile);
					state = 0;
					goto BA_LBL_LEX_LOOPEND;
				}
			}
			else if (state == ST_CMNT_ML) {
				if (c == '\n') {
					++line;
					col = 1;
					ba_LexerSafeIncFileIter(&fileIter) &&
						fread(fileBuf, sizeof(char), BA_FILE_BUF_SIZE, srcFile);
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
					ba_LexerSafeIncFileIter(&fileIter) &&
						fread(fileBuf, sizeof(char), BA_FILE_BUF_SIZE, srcFile);
					state = ST_CMNT_ML;
					goto BA_LBL_LEX_LOOPEND;
				}
				else if (c == '#') {
					state = 0;
				}
				else if (c == '}') {
					state = ST_CMNT_MLBRAC;
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
					
					if (!strcmp(idBuf, "bool")) {
						nextLex->type = BA_TK_KW_BOOL;
					}
					else if (!strcmp(idBuf, "break")) {
						nextLex->type = BA_TK_KW_BREAK;
					}
					else if (!strcmp(idBuf, "elif")) {
						nextLex->type = BA_TK_KW_ELIF;
					}
					else if (!strcmp(idBuf, "else")) {
						nextLex->type = BA_TK_KW_ELSE;
					}
					else if (!strcmp(idBuf, "exit")) {
						nextLex->type = BA_TK_KW_EXIT;
					}
					else if (!strcmp(idBuf, "garbage")) {
						nextLex->type = BA_TK_KW_GARBAGE;
					}
					else if (!strcmp(idBuf, "goto")) {
						nextLex->type = BA_TK_KW_GOTO;
					}
					else if (!strcmp(idBuf, "i64")) {
						nextLex->type = BA_TK_KW_I64;
					}
					else if (!strcmp(idBuf, "i8")) {
						nextLex->type = BA_TK_KW_I8;
					}
					else if (!strcmp(idBuf, "if")) {
						nextLex->type = BA_TK_KW_IF;
					}
					else if (!strcmp(idBuf, "include")) {
						nextLex->type = BA_TK_KW_INCLUDE;
					}
					else if (!strcmp(idBuf, "iter")) {
						nextLex->type = BA_TK_KW_ITER;
					}
					else if (!strcmp(idBuf, "lengthof")) {
						nextLex->type = BA_TK_KW_LENGTHOF;
					}
					else if (!strcmp(idBuf, "return")) {
						nextLex->type = BA_TK_KW_RETURN;
					}
					else if (!strcmp(idBuf, "u64")) {
						nextLex->type = BA_TK_KW_U64;
					}
					else if (!strcmp(idBuf, "u8")) {
						nextLex->type = BA_TK_KW_U8;
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
						nextLex->val = ba_MAlloc(BA_LITERAL_SIZE+1);
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
					
					nextLex->val = ba_MAlloc(BA_LITERAL_SIZE+1);
					// memcpy, not strcpy because there could be zeros
					memcpy(nextLex->val, litBuf, litIter+1);
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
					ba_LexerSafeIncFileIter(&fileIter) &&
						fread(fileBuf, sizeof(char), BA_FILE_BUF_SIZE, srcFile);
					goto BA_LBL_LEX_LOOPEND;
				}
				else if (c == '\\') {
					ba_LexerSafeIncFileIter(&fileIter) &&
						fread(fileBuf, sizeof(char), BA_FILE_BUF_SIZE, srcFile);
					if (fileBuf[fileIter] == '\n') {
						++line;
						col = 1;
						ba_LexerSafeIncFileIter(&fileIter) &&
							fread(fileBuf, sizeof(char), 
								BA_FILE_BUF_SIZE, srcFile);
						goto BA_LBL_LEX_LOOPEND;
					}

					litBuf[litIter++] = ba_LexerEscSequence(ctr, srcFile, 
						fileBuf, &col, &line, &fileIter);

					goto BA_LBL_LEX_LOOPEND;
				}
				else {
					litBuf[litIter++] = c;
				}
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
					nextLex->val = ba_MAlloc(litIter+1);
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

