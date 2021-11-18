// See LICENSE for copyright/license information

#ifndef BA__EXITMSG_IM_H
#define BA__EXITMSG_IM_H

#include "exitmsg.h"
#include "im.h"

u8 ba_ErrorIMArgCount(u64 args, struct ba_IM* im) {
	printf("Error: instruction requires %llu arguments: %s\n", args, ba_IMToStr(im));
	exit(-1);
	return 0;
}

u8 ba_ErrorIMArgInvalid(struct ba_IM* im) {
	printf("Error: invalid set of arguments to instruction %s\n", ba_IMToStr(im));
	exit(-1);
	return 0;
}

#endif
