// See LICENSE for copyright/license information

#include "common.h"

/* program = { stmt } eof */
u8 ba_Parse(struct ba_Ctr* ctr) {
	ba_IncludeCore(ctr);
	
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	while (!ba_PAccept(BA_TK_EOF, ctr)) {
		if (!ba_PStmt(ctr)) {
			// doesn't have anything to do with literals, 
			// this is just a decent buffer size
			char msg[BA_LITERAL_SIZE+1];
			if (ctr->lex->type == BA_TK_FILECHANGE) {
				snprintf(msg, BA_LITERAL_SIZE+1, 
					"unexpected end of file %s, included on", ctr->currPath);
				ctr->currPath = ctr->lex->val;
			}
			else if (ctr->lex->type == BA_TK_EOF) {
				fprintf(stderr, "Error: unexpected end of file in %s\n",
					ctr->currPath);
				exit(1);
			}
			else {
				snprintf(msg, BA_LITERAL_SIZE+1, "unexpected %s at", 
					ba_GetLexemeStr(ctr->lex->type));
			}
			return ba_ExitMsg(BA_EXIT_ERR, msg, ctr->lex->line, 
				ctr->lex->col, ctr->currPath);
		}
	}
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP);

	// Exit
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 60);
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_RDI, BA_IM_RDI);
	ba_AddIM(ctr, 1, BA_IM_SYSCALL);

	return 1;
}

