// See LICENSE for copyright/license information

#include "parser.h"
#include "elf64.h"

#include "bltin/bltin.h"

char usageStr[] = 
	"Usage: basque [options] file\n"
	"Use \"-\" for file to read from stdin.\n"
	"Options:\n"
	"  -h,--help               Displays this text.\n"
	"  -v,--version            Display the current version of the C Basque compiler.\n"
	"  -o <FILE>               Output compiled code into <FILE>. Use - for stdout.\n"
	"  -s,--silence-warnings   Silence warnings.\n"
	"  -w,--extra-warnings     Show extra warnings.\n"
	"  -W,--warnings-as-errors Treat warnings as errors.\n"
;

int main(int argc, char* argv[]) {
	// ----- Handle args ------
	
	if (argc < 2) {
		printf("%s", usageStr);
		return 0;
	}

	FILE* srcFile = 0;
	char* outFileName = 0;

	for (u64 i = 1; i < argc; i++) {
		u8 isHandledCLO = 0;
		u8 isCLOHelp = 0;
		u8 isCLOVersion = 0;
		
		u64 len = strlen(argv[i]);
		
		if (!strcmp(argv[i], "-o")) {
			if (++i == argc) {
				printf("Error: no file name provided after -o\n");
				return -1;
			}
			outFileName = malloc(len * sizeof(char));
			if (!outFileName) {
				return ba_ErrorMallocNoMem();
			}
			strcpy(outFileName, argv[i]);
			goto BA_LBL_MAIN_ARGSLOOPEND;
		}
		else if (argv[i][0] == '-') {
			if (len > 1 && argv[i][1] != '-') {
				isHandledCLO = 1;
				for (u64 j = 1; j < len; j++) {
					switch (argv[i][j]) {
						case 'h':
							isCLOHelp = 1;
							break;
						case 'v':
							isCLOVersion = 1;
							break;
						case 's':
							ba_IsSilenceWarnings = 1;
							break;
						case 'w':
							ba_IsExtraWarnings = 1;
							break;
						case 'W':
							ba_IsWarningsAsErrors = 1;
							break;
						default:
							printf("Command line option %s not found\n", argv[i]);
							return -1;
					}
				}
			}
		}
		
		if (isCLOHelp || !strcmp(argv[i], "--help")) {
			printf("%s", usageStr);
			return 0;
		}
		else if (isCLOVersion || !strcmp(argv[i], "--version")) {
			printf("basque " BA_VERSION "\n");
			return 0;
		}
		else if (isHandledCLO) {
			goto BA_LBL_MAIN_ARGSLOOPEND;
		}
		else if (!strcmp(argv[i], "--silence-warnings")) {
			ba_IsSilenceWarnings = 1;
		}
		else if (!strcmp(argv[i], "--extra-warnings")) {
			ba_IsExtraWarnings = 1;
		}
		else if (!strcmp(argv[i], "--warnings-as-errors")) {
			ba_IsWarningsAsErrors = 1;
		}
		else if (len > 1 && argv[i][0] == '-' && argv[i][1] == '-') {
			printf("Command line option %s not found\n", argv[i]);
			return -1;
		}
		else if (!srcFile) {
			if (!strcmp(argv[i], "-")) {
				srcFile = stdin;
			}
			else {
				srcFile = fopen(argv[i], "r");
			}
			if (!srcFile) {
				printf("Cannot find %s: no such file\n", argv[i]);
				return -1;
			}
			if (!outFileName) {
				if ((len > 3) && !strcmp(argv[i]+len-3, ".ba")) {
					outFileName = malloc(len-3);
					if (!outFileName) {
						return ba_ErrorMallocNoMem();
					}
					strcpy(outFileName, argv[i]);
					*(outFileName+len-3) = '\0';
				}
				else {
					outFileName = "out";
				}
			}
		}
		else {
			printf("Error: only one input file may be specified\n");
			return -1;
		}
		BA_LBL_MAIN_ARGSLOOPEND:;
	}
	if (ba_IsWarningsAsErrors && ba_IsSilenceWarnings) {
		printf("Error: cannot both silence warnings and have warnings as errors\n");
		return -1;
	}
	if (!srcFile) {
		printf("%s", usageStr);
		return 0;
	}

	// ----- Begin parsing -----
	
	struct ba_Controller* ctr = ba_NewController();

	if (!ba_Tokenize(srcFile, ctr)) {
		return -1;
	}
	fclose(srcFile);
	
	// DEBUG
	/*
	struct ba_Lexeme* lex = ctr->startLex;
	while (lex) {
		printf("%llx %s\n", lex->type, lex->val);
		lex = lex->next;
	}
	*/

	if (!ba_Parse(ctr)) {
		return -1;
	}
	
	// DEBUG
	/*
	for (u64 i = 0; i < ctr->globalST->capacity; i++) {
		struct ba_STEntry e = ctr->globalST->entries[i];
		if (e.val) {
			printf("entry %lld: %s %llx\n", i, e.key, e.val->type);
		}
	}
	*/

	// ----- Optimization -----
	// TODO: optimization

	// ----- Binary generation -----
	
	if (!ba_WriteBinary(outFileName, ctr)) {
		return -1;
	}
	
	return 0;
}

