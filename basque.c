// See LICENSE for copyright/license information

#include "parser.h"
#include "elf64.h"

char usageStr[] = 
	"Usage: basque [options] file\n"
	"Options:\n"
	"  -h,--help               Displays this text.\n"
	"  -o <FILE>               Output compiled code into <FILE>.\n"
	"  -s,--silence-warnings   Silence warnings.\n"
	"  -v,--version            Display the current version of Basque.\n"
	"  -w,--warnings-as-errors Treat warnings as errors.\n"
;

int main(int argc, char* argv[]) {
	// ----- Handle args ------
	
	if (argc < 2) {
		printf("%s", usageStr);
		return 0;
	}

	FILE* srcFile = NULL;
	char* outFileName = NULL;

	for (u64 i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
			printf("%s", usageStr);
			return 0;
		}
		else if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-v")) {
			printf("Basque " BA_VERSION "\n");
			return 0;
		}
		else if (!strcmp(argv[i], "-o")) {
			if (++i == argc) {
				printf("Error: no file name provided after -o\n");
				return -1;
			}
			outFileName = malloc(strlen(argv[i]) * sizeof(char));
			strcpy(outFileName, argv[i]);
		}
		else if (!strcmp(argv[i], "--silence-warnings") || !strcmp(argv[i], "-s")) {
			ba_IsSilenceWarnings = 1;
		}
		else if (!strcmp(argv[i], "--warnings-as-errors") || !strcmp(argv[i], "-w")) {
			ba_IsWarningsAsErrors = 1;
		}
		else if (argv[i][0] == '-') {
			printf("Option %s not found\n", argv[i]);
			return -1;
		}
		else if (srcFile == NULL) {
			srcFile = fopen(argv[i], "r");
			if (srcFile == NULL) {
				printf("Cannot find %s: no such file or directory\n", argv[i]);
				return -1;
			}
			if (outFileName == NULL) {
				u64 len = strlen(argv[i]);
				if ((len > 3) && !strcmp(argv[i]+len-3, ".ba")) {
					outFileName = malloc((len-3) * sizeof(char));
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
	}
	if (ba_IsWarningsAsErrors && ba_IsSilenceWarnings) {
		printf("Error: cannot both silence warnings and have warnings as errors\n");
		return -1;
	}
	if (srcFile == NULL) {
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
			printf("entry %lld: %s %llx\n", i, e.key, e.val->type);//dodo
		}
	}
	*/

	// TODO: optimization
	
	// ----- Binary generation -----
	
	if (!ba_WriteBinary(outFileName, ctr)) {
		return -1;
	}
	
	return 0;
}

