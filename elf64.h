// See LICENSE for copyright/license information

#ifndef BA__BINARY_H
#define BA__BINARY_H

#include "sys/stat.h"
#include "common/common.h"

inline u8 ba_ConditionalCodeResize(u8** code, u64* codeCap, u64 codeSize) {
	if (codeSize > *codeCap) {
		*codeCap <<= 1;
		*code = realloc(*code, *codeCap);
		if (!*code) {
			return ba_ErrorMallocNoMem();
		}
	}
	return 1;
}

u8 ba_WriteBinary(char* fileName, struct ba_Controller* ctr) {
	// Note: start memory location is fixed at 0x400000, 
	// entry point fixed at 0x401000

	u64 tmp;
	
	// Generate data segment
	u64 dataSgmtAddr = 0;
	u64 dataSgmtSize = ctr->dataSgmtSize;
	u8* dataSgmt = malloc(dataSgmtSize);
	if (!dataSgmt) {
		return ba_ErrorMallocNoMem();
	}

	for (u64 i = 0; i < ctr->globalST->capacity; i++) {
		struct ba_STEntry e = ctr->globalST->entries[i];
		if (e.key) {
			u64 sz = ba_GetSizeOfType(e.val->type);
			tmp = (u64)e.val->initVal;
			for (u64 j = e.val->address; j < sz; j++) {
				dataSgmt[j] = tmp & 0xff;
				tmp >>= 8;
			}
		}
	}

	// Array of addresses in code that need to be turned into data addresses
	// (using RIP relative addressing)
	u64 ripAddrsCap = 0x1000;
	// Addresses of where the offset is stored in code
	u64* ripAddrData = malloc(ripAddrsCap);
	if (!ripAddrData) {
		return ba_ErrorMallocNoMem();
	}
	// Addresses of the next instruction pointer
	u64* ripAddrPtrs = malloc(ripAddrsCap);
	if (!ripAddrPtrs) {
		return ba_ErrorMallocNoMem();
	}
	u64 ripAddrsSize = 0;
	
	// Generate binary code
	u64 codeCap = 0x1000;
	u8* code = malloc(codeCap);
	if (!code) {
		return ba_ErrorMallocNoMem();
	}
	u64 codeSize = 0;

	struct ba_IM* im = ctr->startIM;
	while (im && im->count) {
		switch (im->vals[0]) {
			case BA_IM_MOV:
				if (im->count < 3) {
					return ba_ErrorIMArgs("MOV", 2);
				}

				// Into GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					// GPR, GPR
					if ((BA_IM_RAX <= im->vals[2]) && (BA_IM_R15 >= im->vals[2])) {
						// Bytes to be written
						u8 b0 = 0x48, b2 = 0xc0;
						// Register values
						u8 r0 = im->vals[1] - BA_IM_RAX;
						u8 r1 = im->vals[2] - BA_IM_RAX;
						
						b0 |= (r1 >= 8) << 2;
						b0 |= (r0 >= 8);
						
						b2 |= (r1 & 7) << 3;
						b2 |= (r0 & 7);
						
						codeSize += 3;
						if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
							return 0;
						}

						code[codeSize-3] = b0;
						code[codeSize-2] = 0x89;
						code[codeSize-1] = b2;
					}

					// GPR, DATASGMT
					else if (im->vals[2] == BA_IM_DATASGMT) {
						if (im->count < 4) {
							return ba_ErrorIMArgs("MOV", 2);
						}

						// Addr. rel. to start of data segment
						u64 imm = im->vals[3];

						u8 r0 = im->vals[1] - BA_IM_RAX;
						u8 b0 = 0x48, b2 = 0x5;

						b0 |= (r0 >= 8) << 2;
						b2 |= (r0 & 7) << 3;

						// Where imm (data location) is stored
						ripAddrData[ripAddrsSize] = codeSize+3;
						// RIP
						ripAddrPtrs[ripAddrsSize] = codeSize+7;

						++ripAddrsSize;
						if (ripAddrsSize > ripAddrsCap) {
							ripAddrsCap <<= 1;
							ripAddrData = realloc(ripAddrData, ripAddrsCap);
							if (!ripAddrData) {
								return ba_ErrorMallocNoMem();
							}
							ripAddrPtrs = realloc(ripAddrPtrs, ripAddrsCap);
							if (!ripAddrPtrs) {
								return ba_ErrorMallocNoMem();
							}
						}

						codeSize += 7;
						if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
							return 0;
						}

						code[codeSize-7] = b0;
						code[codeSize-6] = 0x8b;
						code[codeSize-5] = b2;
						code[codeSize-4] = imm & 0xff;
						imm >>= 8;
						code[codeSize-3] = imm & 0xff;
						imm >>= 8;
						code[codeSize-2] = imm & 0xff;
						imm >>= 8;
						code[codeSize-1] = imm & 0xff;
					}

					// GPR, IMM
					else if (im->vals[2] == BA_IM_IMM) {
						if (im->count < 4) {
							return ba_ErrorIMArgs("MOV", 2);
						}
						u64 imm = im->vals[3];

						// 32 bits
						if (imm < (1llu << 32)) {
							u8 b1 = 0xb8;
							u8 r0 = im->vals[1] - BA_IM_RAX;

							b1 |= (r0 & 7);

							codeSize += 5 + (r0 >= 8);
							if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
								return 0;
							}

							if (r0 >= 8) {
								code[codeSize-6] = 0x41;
							}
							code[codeSize-5] = b1;
							code[codeSize-4] = imm & 0xff;
							imm >>= 8;
							code[codeSize-3] = imm & 0xff;
							imm >>= 8;
							code[codeSize-2] = imm & 0xff;
							imm >>= 8;
							code[codeSize-1] = imm & 0xff;
						}

						// 64 bits
						else {
							u8 b0 = 0x48, b1 = 0xb8;
							u8 r0 = im->vals[1] - BA_IM_RAX;

							b0 |= (r0 >= 8);
							b1 |= (r0 & 7);

							codeSize += 10;
							if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
								return 0;
							}

							code[codeSize-10] = b0;
							code[codeSize-9] = b1;
							code[codeSize-8] = imm & 0xff;
							imm >>= 8;
							code[codeSize-7] = imm & 0xff;
							imm >>= 8;
							code[codeSize-6] = imm & 0xff;
							imm >>= 8;
							code[codeSize-5] = imm & 0xff;
							imm >>= 8;
							code[codeSize-4] = imm & 0xff;
							imm >>= 8;
							code[codeSize-3] = imm & 0xff;
							imm >>= 8;
							code[codeSize-2] = imm & 0xff;
							imm >>= 8;
							code[codeSize-1] = imm & 0xff;
						}
					}

					// TODO: else
				}
				
				// Into ADRADD/ADRSUB GPR effective address
				else if ((im->vals[1] == BA_IM_ADRADD) || (im->vals[1] == BA_IM_ADRSUB)) {
					if (im->count < 5) {
						return ba_ErrorIMArgs("MOV", 2);
					}
					
					if (!(BA_IM_RAX <= im->vals[2]) || !(BA_IM_R15 >= im->vals[2])) {
						printf("Error: first argument for ADRADD/ADRSUB must be a "
							"general purpose register\n");
						exit(-1);
					}
					
					// From GPR
					if ((BA_IM_RAX <= im->vals[4]) && (BA_IM_R15 >= im->vals[4])) {
						u8 sub = im->vals[1] == BA_IM_ADRSUB;

						u8 r0 = im->vals[2] - BA_IM_RAX;
						u8 r1 = im->vals[4] - BA_IM_RAX;
						u8 b0, b2;
						
						b0 = 0x48 | (r0 >= 8) | ((r1 >= 8) << 3);

						u64 offset = im->vals[3];

						// Work out scale of the offset
						if (offset < 0x80) {
							// Offset byte
							if (sub) {
								offset = -offset;
							}

							u8 ofb0 = offset & 0xff;
							
							b2 = 0x40 | (r0 & 7) | ((r1 & 7) << 3);

							if (r1 == 4) {
								codeSize += 5;
								if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
									return 0;
								}
								code[codeSize-5] = b0;
								code[codeSize-4] = 0x89;
								code[codeSize-3] = b2;
								code[codeSize-2] = 0x24;
							}
							else {
								codeSize += 4;
								if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
									return 0;
								}
								code[codeSize-4] = b0;
								code[codeSize-3] = 0x89;
								code[codeSize-2] = b2;
							}

							code[codeSize-1] = ofb0;
						}
						else if (offset < (1llu << 31)) {
							if (sub) {
								offset = -offset;
							}

							b2 = 0x80 | (r0 & 7) | ((r1 & 7) << 3);

							if (r1 == 4) {
								codeSize += 8;
								if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
									return 0;
								}
								code[codeSize-8] = b0;
								code[codeSize-7] = 0x89;
								code[codeSize-6] = b2;
								code[codeSize-5] = 0x24;
							}
							else {
								codeSize += 7;
								if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
									return 0;
								}
								code[codeSize-7] = b0;
								code[codeSize-6] = 0x89;
								code[codeSize-5] = b2;
							}

							code[codeSize-4] = offset & 0xff;
							offset >>= 8;
							code[codeSize-3] = offset & 0xff;
							offset >>= 8;
							code[codeSize-2] = offset & 0xff;
							offset >>= 8;
							code[codeSize-1] = offset & 0xff;
						}
						else {
							printf("Error: Effective address cannot have a more "
								"than 32 bit offset\n");
							exit(-1);
						}
					}

					// TODO: else
				}
				
				else {
					printf("Error: invalid set of arguments to MOV instruction\n");
					exit(-1);
				}

				break;

			case BA_IM_ADD:
				if (im->count < 3) {
					return ba_ErrorIMArgs("ADD", 2);
				}

				// GPR, IMM
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1]) &&
					(im->vals[2] == BA_IM_IMM))
				{
					if (im->count < 4) {
						return ba_ErrorIMArgs("ADD", 2);
					}
					u64 imm = im->vals[3];

					// 7 bits
					if (imm < 0x80) {
						u8 b0 = 0x48, b2 = 0xc0, b3 = imm & 0xff;
						u8 r0 = im->vals[1] - BA_IM_RAX;

						b0 |= (r0 >= 8);
						b2 |= (r0 & 7);

						codeSize += 4;
						if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
							return 0;
						}

						code[codeSize-4] = b0;
						code[codeSize-3] = 0x83;
						code[codeSize-2] = b2;
						code[codeSize-1] = b3;
					}
					else {
						// Do not allow >31 bits
						if (imm >= (1llu << 31)) {
							printf("Error: Cannot ADD more than 31 bits to a register");
							exit(-1);
						}

						u8 b0 = 0x48, b2 = 0xc0;
						u8 r0 = im->vals[1] - BA_IM_RAX;
						b0 |= (r0 >= 8);
						b2 |= (r0 & 7);

						codeSize += 6;
						if (im->vals[1] != BA_IM_RAX) {
							codeSize++;
						}

						if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
							return 0;
						}

						if (im->vals[1] == BA_IM_RAX) {
							code[codeSize-6] = 0x48;
							code[codeSize-5] = 0x05;
						}
						else {
							code[codeSize-7] = b0;
							code[codeSize-6] = 0x81;
							code[codeSize-5] = b2;
						}

						code[codeSize-4] = imm & 0xff;
						imm >>= 8;
						code[codeSize-3] = imm & 0xff;
						imm >>= 8;
						code[codeSize-2] = imm & 0xff;
						imm >>= 8;
						code[codeSize-1] = imm & 0xff;
					}
				}

				else {
					printf("Error: invalid set of arguments to ADD instruction\n");
					exit(-1);
				}

				break;
			
			case BA_IM_SUB:
				if (im->count < 3) {
					return ba_ErrorIMArgs("SUB", 2);
				}

				// GPR, IMM
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1]) &&
					(im->vals[2] == BA_IM_IMM))
				{
					if (im->count < 4) {
						return ba_ErrorIMArgs("SUB", 2);
					}
					u64 imm = im->vals[3];

					// 7 bits
					if (imm < 0x80) {
						u8 b0 = 0x48, b2 = 0xe8, b3 = imm & 0xff;
						u8 r0 = im->vals[1] - BA_IM_RAX;
						
						b0 |= (r0 >= 8);
						b2 |= (r0 & 7);

						codeSize += 4;
						if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
							return 0;
						}

						code[codeSize-4] = b0;
						code[codeSize-3] = 0x83;
						code[codeSize-2] = b2;
						code[codeSize-1] = b3;
					}
					// More bits
					else {
						// Do not allow >32 bits
						if (imm >= (1llu << 31)) {
							printf("Error: Cannot SUB more than 31 bits from a register");
							exit(-1);
						}
						
						u8 b0 = 0x48, b2 = 0xe8;
						u8 r0 = im->vals[1] - BA_IM_RAX;
						b0 |= (r0 >= 8);
						b2 |= (r0 & 7);

						codeSize += 6;
						if (im->vals[1] != BA_IM_RAX) {
							codeSize++;
						}

						if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
							return 0;
						}

						if (im->vals[1] == BA_IM_RAX) {
							code[codeSize-6] = 0x48;
							code[codeSize-5] = 0x2d;
						}
						else {
							code[codeSize-7] = b0;
							code[codeSize-6] = 0x81;
							code[codeSize-5] = b2;
						}

						code[codeSize-4] = imm & 0xff;
						imm >>= 8;
						code[codeSize-3] = imm & 0xff;
						imm >>= 8;
						code[codeSize-2] = imm & 0xff;
						imm >>= 8;
						code[codeSize-1] = imm & 0xff;
					}
				}
				
				else {
					printf("Error: invalid set of arguments to SUB instruction\n");
					exit(-1);
				}

				break;

			case BA_IM_INC:
				if (im->count < 2) {
					return ba_ErrorIMArgs("INC", 1);
				}

				// GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					u8 b0 = 0x48, b2 = 0xc0;
					u8 r0 = im->vals[1] - BA_IM_RAX;

					b0 |= (r0 >= 8);
					b2 |= (r0 & 7);

					codeSize += 3;
					if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
						return 0;
					}

					code[codeSize-3] = b0;
					code[codeSize-2] = 0xff;
					code[codeSize-1] = b2;
				}

				else {
					printf("Error: invalid set of arguments to INC instruction\n");
					exit(-1);
				}

				break;

			case BA_IM_NOT:
				if (im->count < 2) {
					return ba_ErrorIMArgs("NOT", 1);
				}

				// GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					u8 b0 = 0x48, b2 = 0xd0;
					u8 r0 = im->vals[1] - BA_IM_RAX;

					b0 |= (r0 >= 8);
					b2 |= (r0 & 7);

					codeSize += 3;
					if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
						return 0;
					}

					code[codeSize-3] = b0;
					code[codeSize-2] = 0xf7;
					code[codeSize-1] = b2;
				}

				else {
					printf("Error: invalid set of arguments to INC instruction\n");
					exit(-1);
				}

				break;

			case BA_IM_TEST:
				if (im->count < 3) {
					return ba_ErrorIMArgs("TEST", 2);
				}

				// First arg GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					// GPR, GPR
					if ((BA_IM_RAX <= im->vals[2]) && (BA_IM_R15 >= im->vals[2])) {
						u8 b0 = 0x48, b2 = 0xc0;
						u8 r0 = im->vals[1] - BA_IM_RAX;
						u8 r1 = im->vals[2] - BA_IM_RAX;
						
						b0 |= (r1 >= 8) << 2;
						b0 |= (r0 >= 8);
						
						b2 |= (r1 & 7) << 3;
						b2 |= (r0 & 7);
						
						codeSize += 3;
						if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
							return 0;
						}

						code[codeSize-3] = b0;
						code[codeSize-2] = 0x85;
						code[codeSize-1] = b2;
					}

					// TODO: else
				}
				// First arg GPRb
				else if ((BA_IM_AL <= im->vals[1]) && (BA_IM_R15B >= im->vals[1])) {
					// GPRb, GPRb
					if ((BA_IM_AL <= im->vals[2]) && (BA_IM_R15B >= im->vals[2])) {
						u8 b0 = 0x40, b2 = 0xc0;
						u8 r0 = im->vals[1] - BA_IM_AL;
						u8 r1 = im->vals[2] - BA_IM_AL;

						b2 |= (r1 & 7) << 3;
						b2 |= (r0 & 7);

						tmp = ((BA_IM_SPL <= im->vals[1]) | 
							(BA_IM_SPL <= im->vals[2]));
						codeSize += 2 + tmp;
						
						if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
							return 0;
						}
						
						if (tmp) {
							b0 |= (r1 >= 8) << 2;
							b0 |= (r0 >= 8);
							code[codeSize-3] = b0;
						}
						code[codeSize-2] = 0x84;
						code[codeSize-1] = b2;
					}

					// TODO: else
				}

				else {
					printf("Error: invalid set of arguments to TEST instruction\n");
					exit(-1);
				}

				break;

			case BA_IM_AND:
				if (im->count < 3) {
					return ba_ErrorIMArgs("AND", 2);
				}

				// First arg GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					// GPR, GPR
					if ((BA_IM_RAX <= im->vals[2]) && (BA_IM_R15 >= im->vals[2])) {
						u8 b0 = 0x48, b2 = 0xc0;
						u8 r0 = im->vals[1] - BA_IM_RAX;
						u8 r1 = im->vals[2] - BA_IM_RAX;
						
						b0 |= (r1 >= 8) << 2;
						b0 |= (r0 >= 8);
						
						b2 |= (r1 & 7) << 3;
						b2 |= (r0 & 7);
						
						codeSize += 3;
						if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
							return 0;
						}

						code[codeSize-3] = b0;
						code[codeSize-2] = 0x21;
						code[codeSize-1] = b2;
					}

					// TODO: else
				}

				else {
					printf("Error: invalid set of arguments to TEST instruction\n");
					exit(-1);
				}

				break;
			case BA_IM_SYSCALL:
				codeSize += 2;
				if (!ba_ConditionalCodeResize(&code, &codeCap, codeSize)) {
					return 0;
				}

				code[codeSize-2] = 0xf;
				code[codeSize-1] = 0x5;

				break;

			default:
				printf("Error: unrecognized intermediate instruction: %#llx\n",
					im->vals[0]);
				exit(-1);
		}

		im = ba_DelIM(im);
	}

	// Data segment
	dataSgmtAddr = 0x1000 + codeSize;
	tmp = dataSgmtAddr & 0xfff;
	// Round up to nearest 0x1000
	if (tmp) {
		dataSgmtAddr ^= tmp;
		dataSgmtAddr += 0x1000;
	}

	// Fix RIP relative addresses
	for (u64 i = 0; i < ripAddrsSize; i++) {
		u64 codeLoc = ripAddrData[i];
		tmp = dataSgmtAddr - ripAddrPtrs[i] + 
			code[codeLoc] + (code[codeLoc+1] << 8) +
			(code[codeLoc+2] << 16) + (code[codeLoc+3] << 24);
		code[codeLoc] = tmp & 0xff;
		tmp >>= 8;
		code[codeLoc+1] = tmp & 0xff;
		tmp >>= 8;
		code[codeLoc+2] = tmp & 0xff;
		tmp >>= 8;
		code[codeLoc+3] = tmp & 0xff;
	}

	// Generate file header
	u8 fileHeader[64] = {
		0x7f, 'E',  'L',  'F',  2,    1,    1,    3,
		0,    0,    0,    0,    0,    0,    0,    0,
		2,    0,    62,   0,    1,    0,    0,    0,
		0,    0x10, 0x40, 0,    0,    0,    0,    0,    // Entry point 0x401000
		0x40, 0,    0,    0,    0,    0,    0,    0,
		0,    0,    0,    0,    0,    0,    0,    0,    // No section headers
		0,    0,    0,    0,    0x40, 0,    0x38, 0,
		3,    0,    0,    0,    0,    0,    0,    0,    // 3 p header entries
	};

	// Generate program headers
	enum {
		phSz = 3 * 0x38
	};
	u8 programHeader[phSz] = {
		1,    0,    0,    0,    4,    0,    0,    0,    // LOAD, Read
		0,    0,    0,    0,    0,    0,    0,    0,    // 0x0000 in file
		0,    0,    0x40, 0,    0,    0,    0,    0,    // 0x400000 in mem
		0,    0,    0x40, 0,    0,    0,    0,    0,    // "
		0xe8, 0,    0,    0,    0,    0,    0,    0,    // f.h.sz (0x40) + p.h.sz (3 * 0x56)
		0xe8, 0,    0,    0,    0,    0,    0,    0,    // "
		0,    0x10, 0,    0,    0,    0,    0,    0,    // 0x1000 offset

		1,    0,    0,    0,    5,    0,    0,    0,    // LOAD, Read+Execute
		0,    0x10, 0,    0,    0,    0,    0,    0,    // 0x1000 in file
		0,    0x10, 0x40, 0,    0,    0,    0,    0,    // 0x401000 in mem
		0,    0x10, 0x40, 0,    0,    0,    0,    0,    // "
		0,    0,    0,    0,    0,    0,    0,    0,    // {codeSize}
		0,    0,    0,    0,    0,    0,    0,    0,    // "
		0,    0x10, 0,    0,    0,    0,    0,    0,    // 0x1000 offset

		1,    0,    0,    0,    6,    0,    0,    0,    // LOAD, Read+Write
		0,    0,    0,    0,    0,    0,    0,    0,    // {dataSgmtAddr}
		0,    0,    0,    0,    0,    0,    0,    0,    // {0x400000+dataSgmtAddr} in mem
		0,    0,    0,    0,    0,    0,    0,    0,    // "
		0,    0,    0,    0,    0,    0,    0,    0,    // {dataSgmtSize}
		0,    0,    0,    0,    0,    0,    0,    0,    // "
		0,    0x10, 0,    0,    0,    0,    0,    0,    // 0x1000 offset
	};

	// {codeSize} in second header (code)
	for (u64 i = 0; i < 16; i += 8) {
		tmp = codeSize;
		for (u64 j = 0; j < 8; j++) {
			programHeader[0x58+i+j] = tmp & 0xff;
			tmp >>= 8;
		}
	}

	// {dataSgmtAddr} in third header (data)
	tmp = dataSgmtAddr;
	for (u64 i = 0; i < 8; i++) {
		programHeader[0x78+i] = tmp & 0xff;
		tmp >>= 8;
	}

	// {0x400000+dataSgmtAddr} in third header (data)
	for (u64 i = 0; i < 16; i += 8) {
		tmp = 0x400000 + dataSgmtAddr;
		for (u64 j = 0; j < 8; j++) {
			programHeader[0x80+i+j] = tmp & 0xff;
			tmp >>= 8;
		}
	}

	// {dataSgmtSize} in third header (data)
	for (u64 i = 0; i < 16; i += 8) {
		tmp = dataSgmtSize;
		for (u64 j = 0; j < 8; j++) {
			programHeader[0x90+i+j] = tmp & 0xff;
			tmp >>= 8;
		}
	}
	
	// String this together into a file
	
	// Create the initial file
	FILE* file = fopen(fileName, "wb");
	if (!file) {
		return 0;
	}
	fclose(file);

	// Create the file for appending
	file = fopen(fileName, "ab");
	if (!file) {
		return 0;
	}
	
	// Write headers and code
	u8 buf[BA_FILE_BUF_SIZE];
	memcpy(buf, fileHeader, 64);
	memcpy(64+buf, programHeader, phSz);
	memset(64+phSz+buf, 0, 0x1000-phSz-64);
	u64 bufCodeSize = ( 0x1000 + codeSize ) >= BA_FILE_BUF_SIZE ? 
		BA_FILE_BUF_SIZE-0x1000 : codeSize;
	memcpy(0x1000+buf, code, bufCodeSize);
	fwrite(buf, sizeof(u8), 0x1000+bufCodeSize, file);
	
	// Write more code, if not everything can fit in the buffer
	u8* codePtr = code+BA_FILE_BUF_SIZE-0x1000;
	while (codePtr-code < codeSize) {
		fwrite(codePtr, sizeof(u8), codeSize >= BA_FILE_BUF_SIZE ? 
			BA_FILE_BUF_SIZE : codeSize, file);
		codePtr += BA_FILE_BUF_SIZE;
	}
	// Zeros are written at the end if it doesn't align to 0x1000
	memset(buf, 0, 0x1000);
	tmp = codeSize & 0xfff;
	if (tmp) {
		fwrite(buf, sizeof(u8), 0x1000-tmp, file);
	}

	// Write data segment
	u64 bufDataSgmtSize = dataSgmtSize >= BA_FILE_BUF_SIZE ? 
		BA_FILE_BUF_SIZE : dataSgmtSize;
	fwrite(dataSgmt, sizeof(u8), bufDataSgmtSize, file);

	// Write more data segment, if not everything can fit in the buffer
	u8* dataSgmtPtr = dataSgmt+BA_FILE_BUF_SIZE;
	while (dataSgmtPtr-dataSgmt < dataSgmtSize) {
		fwrite(dataSgmtPtr, sizeof(u8), dataSgmtSize >= BA_FILE_BUF_SIZE ? 
			BA_FILE_BUF_SIZE : dataSgmtSize, file);
	}

	fclose(file);
	
	chmod(fileName, 0755);

	return 1;
}

#endif
