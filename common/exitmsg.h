// See LICENSE for copyright/license information

#ifndef BA__EXITMSG_H
#define BA__EXITMSG_H

// Options

bool ba_IsSilenceWarnings = 0;
bool ba_IsExtraWarnings = 0;
bool ba_IsWarningsAsErrors = 0;

enum {
	BA_EXIT_ERR = 0,
	BA_EXIT_WARN = 1,
	BA_EXIT_EXTRAWARN = 2,
};

// "2" because it requires 2 message strings

u8 ba_ExitMsg2(u8 type, char* preMsg, u64 line, u64 col, char* postMsg) {
	if ((type != BA_EXIT_ERR && ba_IsSilenceWarnings) || 
			(type == BA_EXIT_EXTRAWARN && !ba_IsExtraWarnings)) {
		return 0;
	}
	switch (type) {
		case BA_EXIT_ERR:
			fprintf(stderr, "Error");
			break;
		case BA_EXIT_WARN:
		case BA_EXIT_EXTRAWARN:
			fprintf(stderr, "Warning");
			break;
	}
	fprintf(stderr, ": %s line %llu:%llu%s\n", preMsg, line, col, postMsg);
	if (type == BA_EXIT_ERR || (ba_IsWarningsAsErrors && 
		(type == BA_EXIT_WARN || (type == BA_EXIT_EXTRAWARN && 
		ba_IsExtraWarnings)))) 
	{
		exit(-1);
	}
	return 0;
}

// ----- Errors based on the basic function -----

u8 ba_ExitMsg(u8 type, char* msg, u64 line, u64 col) {
	return ba_ExitMsg2(type, msg, line, col, "");
}

u8 ba_ErrorIntLitChar(u64 line, u64 col) {
	return ba_ExitMsg(BA_EXIT_ERR, "invalid character in integer literal on",
		line, col);
}

u8 ba_ErrorIntLitTooLong(u64 line, u64 col) {
	return ba_ExitMsg2(BA_EXIT_ERR, "integer literal on", line, col,
		" too long to be parsed");
}

// ----- Independent errors ------

u8 ba_ErrorMallocNoMem() {
	fprintf(stderr, "Error: insufficient memory\n");
	exit(-1);
	return 0;
}

u8 ba_ErrorTknOverflow(char* type, u64 line, u64 col, u64 max) {
	fprintf(stderr, "Error: encountered %s at %llu:%llu greater than maximum "
		"allowed length (%llu characters)\n", type, line, col, max);
	exit(-1);
	return 0;
}

u8 ba_ErrorAssignTypes(char* expType, char* var, char* varType, 
	u64 line, u64 col)
{
	fprintf(stderr, "Error: cannot assign expression of type %s to variable "
		"'%s' of type %s on line %llu:%llu\n", expType, var, varType, 
		line, col);
	exit(-1);
	return 0;
}

u8 ba_ErrorIdUndef(char* var, u64 line, u64 col) {
	fprintf(stderr, "Error: identifier '%s' not defined on line %llu:%llu\n", 
		var, line, col);
	exit(-1);
	return 0;
}

u8 ba_ErrorVarRedef(char* var, u64 line, u64 col) {
	fprintf(stderr, "Error: redefinition of '%s' on line %llu:%llu\n", var, 
		line, col);
	exit(-1);
	return 0;
}

u8 ba_ErrorVarVoid(u64 line, u64 col) {
	return ba_ExitMsg(BA_EXIT_ERR, "variable cannot be of type 'void', "
		/*"did you mean 'void*'?"*/, line, col);
}

#endif
