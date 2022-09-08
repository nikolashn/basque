// See LICENSE for copyright/license information

#include "common.h"
#include "../common/format.h"

u8 ba_PFStr(struct ba_Ctr* ctr) {
	struct ba_FStr* fstr = (void*)ctr->lex->val;
	if (!ba_PAccept(BA_TK_FSTRING, ctr)) {
		return 0;
	}
	while (1) {
		struct ba_FStr* val = (void*)ctr->lex->val;
		if (!ba_PAccept(BA_TK_FSTRING, ctr)) {
			break;
		}
		fstr->last->next = val;
	}
	ba_PTkStkPush(ctr->pTkStk, fstr, (struct ba_Type){0}, 0, 0, 0);
	return 1;
}

