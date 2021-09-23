// See LICENSE for copyright/license information

#ifndef BA__EXITMSG_H
#define BA__EXITMSG_H

// Options

u8 ba_IsSilenceWarnings = 0;
u8 ba_IsWarningsAsErrors = 0;

// "2" because it requires 2 message strings

u8 ba_ExitMsg2(u8 isWarning, char* preMsg, u64 line, u64 col, char* postMsg) {
	if (isWarning && ba_IsSilenceWarnings) {
		return 0;
	}
	printf(isWarning ? "Warning" : "Error");
	printf(": %s line %llu:%llu%s\n", preMsg, line, col, postMsg);
	if (!isWarning || ba_IsWarningsAsErrors) {
		exit(-1);
	}
	return 0;
}

// ----- Errors based on the basic function -----

u8 ba_ExitMsg(u8 isWarning, char* msg, u64 line, u64 col) {
	return ba_ExitMsg2(isWarning, msg, line, col, "");
}

u8 ba_ErrorIntLitChar(u64 line, u64 col) {
	return ba_ExitMsg(0, "invalid character in integer literal on",
		line, col);
}

u8 ba_ErrorIntLitTooLong(u64 line, u64 col) {
	return ba_ExitMsg2(0, "integer literal on", line, col,
		" too long to be parsed");
}

// ----- Independent errors ------

u8 ba_ErrorTknOverflow(char* type, u64 line, u64 col, u64 max) {
	printf("Error: encountered %s at %llu:%llu greater than maximum allowed "
		"length (%llu characters)\n", type, line, col, max);
	exit(-1);
	return 0;
}

u8 ba_ErrorAssignTypes(char* expType, char* var, char* varType, 
	u64 line, u64 col)
{
	printf("Error: cannot assign expression of type %s to variable"
		"'%s' of type %s on line %llu:%llu\n", expType, var, varType, 
		line, col);
	exit(-1);
	return 0;
}

u8 ba_ErrorVarRedef(char* var, u64 line, u64 col) {
	printf("Error: redefinition of variable '%s' on line %llu:%llu\n", var, 
		line, col);
	exit(-1);
	return 0;
}

u8 ba_ErrorIMArgs(char* instr, u64 args) {
	printf("Error: %s instruction requires %llu valid arguments\n", instr, args);
	exit(-1);
	return 0;
}

#endif
