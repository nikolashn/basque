// See LICENSE for copyright/license information

#ifndef BA__BINARY_H
#define BA__BINARY_H

#include "sys/stat.h"
#include "common/common.h"

u8 ba_PessimalInstrSize(struct ba_IM* im);

u8 ba_WriteBinary(char* fileName, struct ba_Controller* ctr) {
	// Note: start memory location is fixed at 0x400000, 
	// entry point fixed at 0x401000

	u64 tmp;
	
	// Generate data segment
	u64 dataSgmtAddr = 0;

	struct ba_DynArr8* dataSgmt = ba_NewDynArr8(ctr->dataSgmtSize);
	dataSgmt->cnt = ctr->dataSgmtSize;

	for (u64 i = 0; i < ctr->globalST->capacity; i++) {
		struct ba_STEntry e = ctr->globalST->entries[i];
		if (e.key) {
			u64 sz = ba_GetSizeOfType(e.val->type);
			tmp = (u64)e.val->initVal;
			for (u64 j = e.val->address; j < sz; j++) {
				dataSgmt->arr[j] = tmp & 0xff;
				tmp >>= 8;
			}
		}
	}

	// Array of addresses in code that need to be turned 
	// into relative data segment addresses
	struct ba_DynArr64* relDSOffsets = ba_NewDynArr64(0x100);
	// Array of addresses of the instruction pointer afterwards
	struct ba_DynArr64* relDSRips = ba_NewDynArr64(0x100);

	// Addresses are relative to the start of the code segment
	struct ba_IMLabel* labels = calloc(ctr->labelCnt, sizeof(*labels));
	if (!labels) {
		return ba_ErrorMallocNoMem();
	}
	
	// Generate binary code
	struct ba_DynArr8* code = ba_NewDynArr8(0x1000);

	struct ba_IM* im = ctr->startIM;
	while (im && im->count) {
		switch (im->vals[0]) {
			case BA_IM_LABEL:
			{
				if (im->count < 2) {
					return ba_ErrorIMArgs("LABEL", 1);
				}
				
				u64 labelID = im->vals[1];
				struct ba_IMLabel* lbl = &labels[labelID];
				
				if (labelID >= ctr->labelCnt || lbl->addr) {
					printf("Error: cannot generate intermediate label %lld\n", 
						labelID);
					exit(-1);
				}

				lbl->addr = code->cnt;
				
				if (lbl->jmpOfsts) {
					for (u64 i = 0; i < lbl->jmpOfsts->cnt; i++) {
						u64 addr = lbl->jmpOfsts->arr[i];
						u8 ros = lbl->jmpOfstSizes->arr[i];

						tmp = lbl->addr - (addr + ros);
						for (u64 j = 0; j < ros; j++) {
							code->arr[addr+j] = tmp & 0xff;
							tmp >>= 8;
						}
					}
				}

				break;
			}

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
						
						code->cnt += 3;
						if (code->cnt > code->cap) {
							ba_ResizeDynArr8(code);
						}

						code->arr[code->cnt-3] = b0;
						code->arr[code->cnt-2] = 0x89;
						code->arr[code->cnt-1] = b2;
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

						// Make sure there is enough space in the data segment
						// address related arrays
						
						if (++relDSOffsets->cnt > relDSOffsets->cap) {
							ba_ResizeDynArr64(relDSOffsets);
						}
						if (++relDSRips->cnt > relDSRips->cap) {
							ba_ResizeDynArr64(relDSOffsets);
						}

						// Where imm (data location) is stored; instruction ptr
						// (Looks like dereferencing NULL, but shouldn't occur)
						*ba_TopDynArr64(relDSOffsets) = code->cnt + 3;
						*ba_TopDynArr64(relDSRips) = code->cnt + 7;

						code->cnt += 7;
						if (code->cnt > code->cap) {
							ba_ResizeDynArr8(code);
						}

						code->arr[code->cnt-7] = b0;
						code->arr[code->cnt-6] = 0x8b;
						code->arr[code->cnt-5] = b2;
						code->arr[code->cnt-4] = imm & 0xff;
						imm >>= 8;
						code->arr[code->cnt-3] = imm & 0xff;
						imm >>= 8;
						code->arr[code->cnt-2] = imm & 0xff;
						imm >>= 8;
						code->arr[code->cnt-1] = imm & 0xff;
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

							code->cnt += 5 + (r0 >= 8);
							if (code->cnt > code->cap) {
								ba_ResizeDynArr8(code);
							}

							if (r0 >= 8) {
								code->arr[code->cnt-6] = 0x41;
							}
							code->arr[code->cnt-5] = b1;
							code->arr[code->cnt-4] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-3] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-2] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-1] = imm & 0xff;
						}

						// 64 bits
						else {
							u8 b0 = 0x48, b1 = 0xb8;
							u8 r0 = im->vals[1] - BA_IM_RAX;

							b0 |= (r0 >= 8);
							b1 |= (r0 & 7);

							code->cnt += 10;
							if (code->cnt > code->cap) {
								ba_ResizeDynArr8(code);
							}

							code->arr[code->cnt-10] = b0;
							code->arr[code->cnt-9] = b1;
							code->arr[code->cnt-8] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-7] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-6] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-5] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-4] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-3] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-2] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-1] = imm & 0xff;
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

							if ((r0 & 7) == 4) {
								code->cnt += 5;
								if (code->cnt > code->cap) {
									ba_ResizeDynArr8(code);
								}
								code->arr[code->cnt-5] = b0;
								code->arr[code->cnt-4] = 0x89;
								code->arr[code->cnt-3] = b2;
								code->arr[code->cnt-2] = 0x24;
							}
							else {
								code->cnt += 4;
								if (code->cnt > code->cap) {
									ba_ResizeDynArr8(code);
								}
								code->arr[code->cnt-4] = b0;
								code->arr[code->cnt-3] = 0x89;
								code->arr[code->cnt-2] = b2;
							}

							code->arr[code->cnt-1] = ofb0;
						}
						else if (offset < (1llu << 31)) {
							if (sub) {
								offset = -offset;
							}

							b2 = 0x80 | (r0 & 7) | ((r1 & 7) << 3);

							if ((r0 & 7) == 4) {
								code->cnt += 8;
								if (code->cnt > code->cap) {
									ba_ResizeDynArr8(code);
								}
								code->arr[code->cnt-8] = b0;
								code->arr[code->cnt-7] = 0x89;
								code->arr[code->cnt-6] = b2;
								code->arr[code->cnt-5] = 0x24;
							}
							else {
								code->cnt += 7;
								if (code->cnt > code->cap) {
									ba_ResizeDynArr8(code);
								}
								code->arr[code->cnt-7] = b0;
								code->arr[code->cnt-6] = 0x89;
								code->arr[code->cnt-5] = b2;
							}

							code->arr[code->cnt-4] = offset & 0xff;
							offset >>= 8;
							code->arr[code->cnt-3] = offset & 0xff;
							offset >>= 8;
							code->arr[code->cnt-2] = offset & 0xff;
							offset >>= 8;
							code->arr[code->cnt-1] = offset & 0xff;
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

				// First argument GPR
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
						
						code->cnt += 3;
						if (code->cnt > code->cap) {
							ba_ResizeDynArr8(code);
						}

						code->arr[code->cnt-3] = b0;
						code->arr[code->cnt-2] = 0x1;
						code->arr[code->cnt-1] = b2;
					}
					// GPR, IMM
					else if (im->vals[2] == BA_IM_IMM) {
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

							code->cnt += 4;
							if (code->cnt > code->cap) {
								ba_ResizeDynArr8(code);
							}

							code->arr[code->cnt-4] = b0;
							code->arr[code->cnt-3] = 0x83;
							code->arr[code->cnt-2] = b2;
							code->arr[code->cnt-1] = b3;
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

							code->cnt += 6;
							if (im->vals[1] != BA_IM_RAX) {
								++code->cnt;
							}

							if (code->cnt > code->cap) {
								ba_ResizeDynArr8(code);
							}

							if (im->vals[1] == BA_IM_RAX) {
								code->arr[code->cnt-6] = 0x48;
								code->arr[code->cnt-5] = 0x05;
							}
							else {
								code->arr[code->cnt-7] = b0;
								code->arr[code->cnt-6] = 0x81;
								code->arr[code->cnt-5] = b2;
							}

							code->arr[code->cnt-4] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-3] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-2] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-1] = imm & 0xff;
						}
					}
				}
				// Into ADRADD/ADRSUB GPR effective address
				else if ((im->vals[1] == BA_IM_ADRADD) || (im->vals[1] == BA_IM_ADRSUB)) {
					if (im->count < 5) {
						return ba_ErrorIMArgs("ADD", 2);
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

							if ((r0 & 7) == 4) {
								code->cnt += 5;
								if (code->cnt > code->cap) {
									ba_ResizeDynArr8(code);
								}
								code->arr[code->cnt-5] = b0;
								code->arr[code->cnt-4] = 0x01;
								code->arr[code->cnt-3] = b2;
								code->arr[code->cnt-2] = 0x24;
							}
							else {
								code->cnt += 4;
								if (code->cnt > code->cap) {
									ba_ResizeDynArr8(code);
								}
								code->arr[code->cnt-4] = b0;
								code->arr[code->cnt-3] = 0x01;
								code->arr[code->cnt-2] = b2;
							}

							code->arr[code->cnt-1] = ofb0;
						}
						else if (offset < (1llu << 31)) {
							if (sub) {
								offset = -offset;
							}

							b2 = 0x80 | (r0 & 7) | ((r1 & 7) << 3);

							if ((r0 & 7) == 4) {
								code->cnt += 8;
								if (code->cnt > code->cap) {
									ba_ResizeDynArr8(code);
								}
								code->arr[code->cnt-8] = b0;
								code->arr[code->cnt-7] = 0x01;
								code->arr[code->cnt-6] = b2;
								code->arr[code->cnt-5] = 0x24;
							}
							else {
								code->cnt += 7;
								if (code->cnt > code->cap) {
									ba_ResizeDynArr8(code);
								}
								code->arr[code->cnt-7] = b0;
								code->arr[code->cnt-6] = 0x01;
								code->arr[code->cnt-5] = b2;
							}

							code->arr[code->cnt-4] = offset & 0xff;
							offset >>= 8;
							code->arr[code->cnt-3] = offset & 0xff;
							offset >>= 8;
							code->arr[code->cnt-2] = offset & 0xff;
							offset >>= 8;
							code->arr[code->cnt-1] = offset & 0xff;
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
					printf("Error: invalid set of arguments to ADD instruction\n");
					exit(-1);
				}

				break;
			
			case BA_IM_SUB:
				if (im->count < 3) {
					return ba_ErrorIMArgs("SUB", 2);
				}

				// First argument GPR
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
						
						code->cnt += 3;
						if (code->cnt > code->cap) {
							ba_ResizeDynArr8(code);
						}

						code->arr[code->cnt-3] = b0;
						code->arr[code->cnt-2] = 0x29;
						code->arr[code->cnt-1] = b2;
					}
					// GPR, IMM
					else if (im->vals[2] == BA_IM_IMM) {
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

							code->cnt += 4;
							if (code->cnt > code->cap) {
								ba_ResizeDynArr8(code);
							}

							code->arr[code->cnt-4] = b0;
							code->arr[code->cnt-3] = 0x83;
							code->arr[code->cnt-2] = b2;
							code->arr[code->cnt-1] = b3;
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

							code->cnt += 6;
							if (im->vals[1] != BA_IM_RAX) {
								code->cnt++;
							}

							if (code->cnt > code->cap) {
								ba_ResizeDynArr8(code);
							}

							if (im->vals[1] == BA_IM_RAX) {
								code->arr[code->cnt-6] = 0x48;
								code->arr[code->cnt-5] = 0x2d;
							}
							else {
								code->arr[code->cnt-7] = b0;
								code->arr[code->cnt-6] = 0x81;
								code->arr[code->cnt-5] = b2;
							}

							code->arr[code->cnt-4] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-3] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-2] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-1] = imm & 0xff;
						}
					}
				}
				// Into ADRADD/ADRSUB GPR effective address
				else if ((im->vals[1] == BA_IM_ADRADD) || (im->vals[1] == BA_IM_ADRSUB)) {
					if (im->count < 5) {
						return ba_ErrorIMArgs("SUB", 2);
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

							if ((r0 & 7) == 4) {
								code->cnt += 5;
								if (code->cnt > code->cap) {
									ba_ResizeDynArr8(code);
								}
								code->arr[code->cnt-5] = b0;
								code->arr[code->cnt-4] = 0x29;
								code->arr[code->cnt-3] = b2;
								code->arr[code->cnt-2] = 0x24;
							}
							else {
								code->cnt += 4;
								if (code->cnt > code->cap) {
									ba_ResizeDynArr8(code);
								}
								code->arr[code->cnt-4] = b0;
								code->arr[code->cnt-3] = 0x29;
								code->arr[code->cnt-2] = b2;
							}

							code->arr[code->cnt-1] = ofb0;
						}
						else if (offset < (1llu << 31)) {
							if (sub) {
								offset = -offset;
							}

							b2 = 0x80 | (r0 & 7) | ((r1 & 7) << 3);

							if ((r0 & 7) == 4) {
								code->cnt += 8;
								if (code->cnt > code->cap) {
									ba_ResizeDynArr8(code);
								}
								code->arr[code->cnt-8] = b0;
								code->arr[code->cnt-7] = 0x29;
								code->arr[code->cnt-6] = b2;
								code->arr[code->cnt-5] = 0x24;
							}
							else {
								code->cnt += 7;
								if (code->cnt > code->cap) {
									ba_ResizeDynArr8(code);
								}
								code->arr[code->cnt-7] = b0;
								code->arr[code->cnt-6] = 0x29;
								code->arr[code->cnt-5] = b2;
							}

							code->arr[code->cnt-4] = offset & 0xff;
							offset >>= 8;
							code->arr[code->cnt-3] = offset & 0xff;
							offset >>= 8;
							code->arr[code->cnt-2] = offset & 0xff;
							offset >>= 8;
							code->arr[code->cnt-1] = offset & 0xff;
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

					code->cnt += 3;
					if (code->cnt > code->cap) {
						ba_ResizeDynArr8(code);
					}

					code->arr[code->cnt-3] = b0;
					code->arr[code->cnt-2] = 0xff;
					code->arr[code->cnt-1] = b2;
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

					code->cnt += 3;
					if (code->cnt > code->cap) {
						ba_ResizeDynArr8(code);
					}

					code->arr[code->cnt-3] = b0;
					code->arr[code->cnt-2] = 0xf7;
					code->arr[code->cnt-1] = b2;
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
						
						code->cnt += 3;
						if (code->cnt > code->cap) {
							ba_ResizeDynArr8(code);
						}

						code->arr[code->cnt-3] = b0;
						code->arr[code->cnt-2] = 0x85;
						code->arr[code->cnt-1] = b2;
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
						code->cnt += 2 + tmp;
						
						if (code->cnt > code->cap) {
							ba_ResizeDynArr8(code);
						}
						
						if (tmp) {
							b0 |= (r1 >= 8) << 2;
							b0 |= (r0 >= 8);
							code->arr[code->cnt-3] = b0;
						}
						code->arr[code->cnt-2] = 0x84;
						code->arr[code->cnt-1] = b2;
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
						
						code->cnt += 3;
						if (code->cnt > code->cap) {
							ba_ResizeDynArr8(code);
						}

						code->arr[code->cnt-3] = b0;
						code->arr[code->cnt-2] = 0x21;
						code->arr[code->cnt-1] = b2;
					}

					// TODO: else
				}

				else {
					printf("Error: invalid set of arguments to AND instruction\n");
					exit(-1);
				}

				break;

			case BA_IM_XOR:
				if (im->count < 3) {
					return ba_ErrorIMArgs("XOR", 2);
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
						
						code->cnt += 3;
						if (code->cnt > code->cap) {
							ba_ResizeDynArr8(code);
						}

						code->arr[code->cnt-3] = b0;
						code->arr[code->cnt-2] = 0x31;
						code->arr[code->cnt-1] = b2;
					}

					// GPR, IMM
					else if (im->vals[2] == BA_IM_IMM) {
						if (im->count < 4) {
							return ba_ErrorIMArgs("XOR", 2);
						}
						u64 imm = im->vals[3];

						// 7 bits
						if (imm < 0x80) {
							u8 b0 = 0x48, b2 = 0xf0, b3 = imm & 0xff;
							u8 r0 = im->vals[1] - BA_IM_RAX;

							b0 |= (r0 >= 8);
							b2 |= (r0 & 7);

							code->cnt += 4;
							if (code->cnt > code->cap) {
								ba_ResizeDynArr8(code);
							}

							code->arr[code->cnt-4] = b0;
							code->arr[code->cnt-3] = 0x83;
							code->arr[code->cnt-2] = b2;
							code->arr[code->cnt-1] = b3;
						}
						else {
							// Do not allow >31 bits
							if (imm >= (1llu << 31)) {
								printf("Error: Cannot XOR more than 31 bits to a register");
								exit(-1);
							}

							u8 b0 = 0x48, b2 = 0xf0;
							u8 r0 = im->vals[1] - BA_IM_RAX;
							b0 |= (r0 >= 8);
							b2 |= (r0 & 7);

							code->cnt += 6;
							if (im->vals[1] != BA_IM_RAX) {
								code->cnt++;
							}

							if (code->cnt > code->cap) {
								ba_ResizeDynArr8(code);
							}

							if (im->vals[1] == BA_IM_RAX) {
								code->arr[code->cnt-6] = 0x48;
								code->arr[code->cnt-5] = 0x35;
							}
							else {
								code->arr[code->cnt-7] = b0;
								code->arr[code->cnt-6] = 0x81;
								code->arr[code->cnt-5] = b2;
							}

							code->arr[code->cnt-4] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-3] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-2] = imm & 0xff;
							imm >>= 8;
							code->arr[code->cnt-1] = imm & 0xff;
						}
					}

					// TODO: else
				}

				else {
					printf("Error: invalid set of arguments to XOR instruction\n");
					exit(-1);
				}

				break;

			case BA_IM_ROR:
				if (im->count < 3) {
					return ba_ErrorIMArgs("ROR", 2);
				}

				// First arg GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					// GPR, CL
					if (im->vals[2] == BA_IM_CL) {
						u8 r0 = im->vals[1] - BA_IM_RAX;
						u8 b0 = 0x48, b2 = 0xc8;

						b0 |= (r0 >= 8);
						b2 |= (r0 & 7);

						code->cnt += 3;
						if (code->cnt > code->cap) {
							ba_ResizeDynArr8(code);
						}

						code->arr[code->cnt-3] = b0;
						code->arr[code->cnt-2] = 0xd3;
						code->arr[code->cnt-1] = b2;
					}

					// TODO: else
				}

				else {
					printf("Error: invalid set of arguments to ROR instruction\n");
					exit(-1);
				}

				break;

			case BA_IM_SHL:
				if (im->count < 3) {
					return ba_ErrorIMArgs("SHL", 2);
				}

				// First arg GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					// GPR, IMM
					if (im->vals[2] == BA_IM_IMM) {
						if (im->count < 4) {
							return ba_ErrorIMArgs("SHL", 2);
						}
						u64 imm = im->vals[3];
						u8 r0 = im->vals[1] - BA_IM_RAX;
						u8 b0 = 0x48, b2 = 0xe0;

						b0 |= (r0 >= 8);
						b2 |= (r0 & 7);

						// Shift of 1
						if (imm == 1) {
							code->cnt += 3;
							if (code->cnt > code->cap) {
								ba_ResizeDynArr8(code);
							}

							code->arr[code->cnt-3] = b0;
							code->arr[code->cnt-2] = 0xd1;
							code->arr[code->cnt-1] = b2;
						}
						// 8 bits
						else if (imm < 0x100) {
							code->cnt += 4;
							if (code->cnt > code->cap) {
								ba_ResizeDynArr8(code);
							}

							u8 b3 = imm & 0xff;

							code->arr[code->cnt-4] = b0;
							code->arr[code->cnt-3] = 0xc1;
							code->arr[code->cnt-2] = b2;
							code->arr[code->cnt-1] = b3;
						}
						else {
							printf("Error: Cannot SHL a register by more than 8 bytes");
							exit(-1);
						}
					}

					// TODO: else
				}

				else {
					printf("Error: invalid set of arguments to SHL instruction\n");
					exit(-1);
				}

				break;

			case BA_IM_SHR:
				if (im->count < 3) {
					return ba_ErrorIMArgs("SHR", 2);
				}

				// First arg GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					// GPR, CL
					if (im->vals[2] == BA_IM_CL) {
						u8 r0 = im->vals[1] - BA_IM_RAX;
						u8 b0 = 0x48, b2 = 0xe8;

						b0 |= (r0 >= 8);
						b2 |= (r0 & 7);

						code->cnt += 3;
						if (code->cnt > code->cap) {
							ba_ResizeDynArr8(code);
						}

						code->arr[code->cnt-3] = b0;
						code->arr[code->cnt-2] = 0xd3;
						code->arr[code->cnt-1] = b2;
					}
					// GPR, IMM
					else if (im->vals[2] == BA_IM_IMM) {
						if (im->count < 4) {
							return ba_ErrorIMArgs("SHR", 2);
						}
						u64 imm = im->vals[3];
						u8 r0 = im->vals[1] - BA_IM_RAX;
						u8 b0 = 0x48, b2 = 0xe8;

						b0 |= (r0 >= 8);
						b2 |= (r0 & 7);

						// Shift of 1
						if (imm == 1) {
							code->cnt += 3;
							if (code->cnt > code->cap) {
								ba_ResizeDynArr8(code);
							}

							code->arr[code->cnt-3] = b0;
							code->arr[code->cnt-2] = 0xd1;
							code->arr[code->cnt-1] = b2;
						}
						// 8 bits
						else if (imm < 0x100) {
							code->cnt += 4;
							if (code->cnt > code->cap) {
								ba_ResizeDynArr8(code);
							}

							u8 b3 = imm & 0xff;

							code->arr[code->cnt-4] = b0;
							code->arr[code->cnt-3] = 0xc1;
							code->arr[code->cnt-2] = b2;
							code->arr[code->cnt-1] = b3;
						}
						else {
							printf("Error: Cannot SHR a register by more than 8 bytes");
							exit(-1);
						}
					}

					// TODO: else
				}

				else {
					printf("Error: invalid set of arguments to SHR instruction\n");
					exit(-1);
				}

				break;

			case BA_IM_MUL:
				if (im->count < 2) {
					return ba_ErrorIMArgs("MUL", 1);
				}

				// First arg GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					u8 r0 = im->vals[1] - BA_IM_RAX;
					u8 b0 = 0x48, b2 = 0xe0;

					b0 |= (r0 >= 8);
					b2 |= (r0 & 7);

					code->cnt += 3;
					if (code->cnt > code->cap) {
						ba_ResizeDynArr8(code);
					}

					code->arr[code->cnt-3] = b0;
					code->arr[code->cnt-2] = 0xf7;
					code->arr[code->cnt-1] = b2;
				}

				else {
					printf("Error: invalid set of arguments to MUL instruction\n");
					exit(-1);
				}

				break;

			case BA_IM_SYSCALL:
				code->cnt += 2;
				if (code->cnt > code->cap) {
					ba_ResizeDynArr8(code);
				}

				code->arr[code->cnt-2] = 0xf;
				code->arr[code->cnt-1] = 0x5;

				break;

			case BA_IM_LABELJMP:
			{
				if (im->count < 2) {
					return ba_ErrorIMArgs("LABELJMP", 1);
				}

				u64 labelID = im->vals[1];
				struct ba_IMLabel* lbl = &labels[labelID];

				if (labelID >= ctr->labelCnt) {
					printf("Error: cannot find intermediate label %lld\n", 
						labelID);
					exit(-1);
				}

				// Label appears before jmp
				if (lbl->addr) {
					// Assumes this instruction will be 2 bytes initially
					i64 relAddr = lbl->addr - (code->cnt + 2);

					// 2 byte jmp
					if ((relAddr >= 0ll && relAddr < 0x80ll) || 
						(relAddr < 0ll && relAddr >= -0x80ll))
					{
						code->cnt += 2;
						if (code->cnt > code->cap) {
							ba_ResizeDynArr8(code);
						}
						code->arr[code->cnt-2] = 0xeb;
						code->arr[code->cnt-1] = (u8)(relAddr & 0xff);
					}
					// 5 byte jmp
					else {
						code->cnt += 5;
						if (code->cnt > code->cap) {
							ba_ResizeDynArr8(code);
						}
						relAddr -= 3; // Accounts for the instr. being 5 bytes
						code->arr[code->cnt-5] = 0xe9;
						code->arr[code->cnt-4] = relAddr & 0xff;
						relAddr >>= 8;
						code->arr[code->cnt-3] = relAddr & 0xff;
						relAddr >>= 8;
						code->arr[code->cnt-2] = relAddr & 0xff;
						relAddr >>= 8;
						code->arr[code->cnt-1] = relAddr & 0xff;
					}
				}
				// Label appears after jmp
				else {
					struct ba_IM* tmpIM = im;
					u64 labelDistance = 0;
					while (tmpIM && tmpIM->count) {
						labelDistance += ba_PessimalInstrSize(tmpIM);
						tmpIM = tmpIM->next;

						// 5 byte jmp
						if (labelDistance >= 0x80) {
							break;
						}
						// 2 byte jmp
						else if (tmpIM->vals[0] == BA_IM_LABEL && 
							tmpIM->vals[1] == labelID)
						{
							break;
						}
					}

					// Don't check for lbl->jmpOfstSizes, they are allocated together
					if (!lbl->jmpOfsts) {
						lbl->jmpOfsts = ba_NewDynArr64(0x100);
						lbl->jmpOfstSizes = ba_NewDynArr8(0x100);
					}

					if (++lbl->jmpOfsts->cnt > lbl->jmpOfsts->cap) {
						ba_ResizeDynArr64(lbl->jmpOfsts);
					}
					if (++lbl->jmpOfstSizes->cnt > lbl->jmpOfsts->cap) {
						ba_ResizeDynArr8(lbl->jmpOfstSizes);
					}

					*ba_TopDynArr64(lbl->jmpOfsts) = code->cnt+1;
					
					if (labelDistance >= 0x80) {
						code->cnt += 5;
						if (code->cnt > code->cap) {
							ba_ResizeDynArr8(code);
						}
						*ba_TopDynArr8(lbl->jmpOfstSizes) = 4;
						code->arr[code->cnt-5] = 0xe9;
					}
					else {
						code->cnt += 2;
						if (code->cnt > code->cap) {
							ba_ResizeDynArr8(code);
						}
						*ba_TopDynArr8(lbl->jmpOfstSizes) = 1;
						code->arr[code->cnt-2] = 0xeb;
					}
				}

				break;
			}

			default:
				printf("Error: unrecognized intermediate instruction: %#llx\n",
					im->vals[0]);
				exit(-1);
		}

		im = ba_DelIM(im);
	}

	// Data segment
	dataSgmtAddr = 0x1000 + code->cnt;
	tmp = dataSgmtAddr & 0xfff;
	// Round up to nearest 0x1000
	if (tmp) {
		dataSgmtAddr ^= tmp;
		dataSgmtAddr += 0x1000;
	}

	// Fix RIP relative addresses
	for (u64 i = 0; i < relDSRips->cnt; i++) {
		u64 codeLoc = relDSOffsets->arr[i];
		// Subtract 0x1000 because code starts at file byte 0x1000
		tmp = dataSgmtAddr - relDSRips->arr[i] - 0x1000 + 
			code->arr[codeLoc] + (code->arr[codeLoc+1] << 8) +
			(code->arr[codeLoc+2] << 16) + (code->arr[codeLoc+3] << 24);
		code->arr[codeLoc] = tmp & 0xff;
		tmp >>= 8;
		code->arr[codeLoc+1] = tmp & 0xff;
		tmp >>= 8;
		code->arr[codeLoc+2] = tmp & 0xff;
		tmp >>= 8;
		code->arr[codeLoc+3] = tmp & 0xff;
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
		0,    0,    0,    0,    0,    0,    0,    0,    // {code->cnt}
		0,    0,    0,    0,    0,    0,    0,    0,    // "
		0,    0x10, 0,    0,    0,    0,    0,    0,    // 0x1000 offset

		1,    0,    0,    0,    6,    0,    0,    0,    // LOAD, Read+Write
		0,    0,    0,    0,    0,    0,    0,    0,    // {dataSgmtAddr}
		0,    0,    0,    0,    0,    0,    0,    0,    // {0x400000+dataSgmtAddr} in mem
		0,    0,    0,    0,    0,    0,    0,    0,    // "
		0,    0,    0,    0,    0,    0,    0,    0,    // {dataSgmt->cnt}
		0,    0,    0,    0,    0,    0,    0,    0,    // "
		0,    0x10, 0,    0,    0,    0,    0,    0,    // 0x1000 offset
	};

	// {code->cnt} in second header (code)
	for (u64 i = 0; i < 16; i += 8) {
		tmp = code->cnt;
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

	// {dataSgmt->cnt} in third header (data)
	for (u64 i = 0; i < 16; i += 8) {
		tmp = dataSgmt->cnt;
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
	u64 bufCodeSize = ( 0x1000 + code->cnt ) >= BA_FILE_BUF_SIZE ? 
		BA_FILE_BUF_SIZE-0x1000 : code->cnt;
	memcpy(0x1000+buf, code->arr, bufCodeSize);
	fwrite(buf, sizeof(u8), 0x1000+bufCodeSize, file);
	
	// Write more code, if not everything can fit in the buffer
	u8* codePtr = code->arr + BA_FILE_BUF_SIZE - 0x1000;
	while (codePtr - code->arr < code->cnt) {
		fwrite(codePtr, sizeof(u8), code->cnt >= BA_FILE_BUF_SIZE ? 
			BA_FILE_BUF_SIZE : code->cnt, file);
		codePtr += BA_FILE_BUF_SIZE;
	}
	// Zeros are written at the end if it doesn't align to 0x1000
	memset(buf, 0, 0x1000);
	tmp = code->cnt & 0xfff;
	if (tmp) {
		fwrite(buf, sizeof(u8), 0x1000-tmp, file);
	}

	// Write data segment
	u64 bufDataSgmtSize = dataSgmt->cnt >= BA_FILE_BUF_SIZE ? 
		BA_FILE_BUF_SIZE : dataSgmt->cnt;
	fwrite(dataSgmt->arr, sizeof(u8), bufDataSgmtSize, file);

	// Write more data segment, if not everything can fit in the buffer
	u8* dataSgmtPtr = dataSgmt->arr + BA_FILE_BUF_SIZE;
	while (dataSgmtPtr - dataSgmt->arr < dataSgmt->cnt) {
		fwrite(dataSgmtPtr, sizeof(u8), dataSgmt->cnt >= BA_FILE_BUF_SIZE ? 
			BA_FILE_BUF_SIZE : dataSgmt->cnt, file);
	}

	fclose(file);
	
	chmod(fileName, 0755);

	ba_DelDynArr8(dataSgmt);
	ba_DelDynArr64(relDSOffsets);
	ba_DelDynArr64(relDSRips);
	ba_DelDynArr8(code);
	if (labels->jmpOfsts) {
		ba_DelDynArr64(labels->jmpOfsts);
		ba_DelDynArr8(labels->jmpOfstSizes);
	}
	free(labels);

	return 1;
}

/* Pessimistically estimates instruction size, for working out size of jmp
 * instructions.
 */
u8 ba_PessimalInstrSize(struct ba_IM* im) {
	switch (im->vals[0]) {
		case BA_IM_LABEL:
			return 0;

		case BA_IM_MOV:
			// Into GPR
			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				// GPR, GPR
				if ((BA_IM_RAX <= im->vals[2]) && (BA_IM_R15 >= im->vals[2])) {
					return 3;
				}
				// GPR, DATASGMT
				else if (im->vals[2] == BA_IM_DATASGMT) {
					return 7;
				}
				// GPR, IMM
				else if (im->vals[2] == BA_IM_IMM) {
					u64 imm = im->vals[3];
					// 32 bits
					if (imm < (1llu << 32)) {
						u8 r0 = im->vals[1] - BA_IM_RAX;
						return 5 + (r0 >= 8);
					}
					// 64 bits
					else {
						return 10;
					}
				}
				
				// TODO: else
			}
			// Into ADRADD/ADRSUB GPR effective address
			else if ((im->vals[1] == BA_IM_ADRADD) || (im->vals[1] == BA_IM_ADRSUB)) {
				// From GPR
				if ((BA_IM_RAX <= im->vals[4]) && (BA_IM_R15 >= im->vals[4])) {
					u8 r0 = im->vals[1] - BA_IM_RAX;
					if (im->vals[3] < 0x80) {
						return 4 + ((r0 & 7) == 4);
					}
					else if (im->vals[3] < (1llu << 31)) {
						return 7 + ((r0 & 7) == 4);
					}
				}
				
				// TODO: else
			}

			break;

		case BA_IM_ADD:
		case BA_IM_SUB:
			// First arg GPR
			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				// GPR, GPR
				if ((BA_IM_RAX <= im->vals[2]) && (BA_IM_R15 >= im->vals[2])) {
					return 3;
				}
				// GPR, IMM
				else if (im->vals[2] == BA_IM_IMM) {
					return 4 + (im->vals[3] >= 0x80) * 2;
				}
			}
			// Into ADRADD/ADRSUB GPR effective address
			else if ((im->vals[1] == BA_IM_ADRADD) || (im->vals[1] == BA_IM_ADRSUB)) {
				// From GPR
				if ((BA_IM_RAX <= im->vals[4]) && (BA_IM_R15 >= im->vals[4])) {
					u8 r0 = im->vals[1] - BA_IM_RAX;
					if (im->vals[3] < 0x80) {
						return 4 + ((r0 & 7) == 4);
					}
					else if (im->vals[3] < (1llu << 31)) {
						return 7 + ((r0 & 7) == 4);
					}
				}
				
				// TODO: else
			}

			break;

		case BA_IM_INC:
		case BA_IM_NOT:
			// GPR
			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				return 3;
			}

			break;

		case BA_IM_TEST:
			// First arg GPR
			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				// GPR, GPR
				if ((BA_IM_RAX <= im->vals[2]) && (BA_IM_R15 >= im->vals[2])) {
					return 3;
				}

				// TODO: else
			}
			// First arg GPRb
			else if ((BA_IM_AL <= im->vals[1]) && (BA_IM_R15B >= im->vals[1])) {
				// GPRb, GPRb
				if ((BA_IM_AL <= im->vals[2]) && (BA_IM_R15B >= im->vals[2])) {
					u8 tmp = ((BA_IM_SPL <= im->vals[1]) | 
						(BA_IM_SPL <= im->vals[2]));

					return 2 + tmp;
				}

				// TODO: else
			}

			break;

		case BA_IM_AND:
			// First arg GPR
			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				// GPR, GPR
				if ((BA_IM_RAX <= im->vals[2]) && (BA_IM_R15 >= im->vals[2])) {
					return 3;
				}

				// TODO: else
			}

			break;

		case BA_IM_XOR:
			// First arg GPR
			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				// GPR, GPR
				if ((BA_IM_RAX <= im->vals[2]) && (BA_IM_R15 >= im->vals[2])) {
					return 3;
				}

				// GPR, IMM
				else if (im->vals[2] == BA_IM_IMM) {
					return 4 + (im->vals[3] >= 0x80) * 2;
				}

				// TODO: else
			}

			break;

		case BA_IM_ROR:
			// First arg GPR
			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				// GPR, CL
				if (im->vals[2] == BA_IM_CL) {
					return 3;
				}

				// TODO: else
			}

			break;

		case BA_IM_SHL:
			// First arg GPR
			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				// GPR, IMM
				if (im->vals[2] == BA_IM_IMM) {
					// Shift of 1
					if (im->vals[3] == 1) {
						return 3;
					}
					// 8 bits
					else if (im->vals[3] < 0x100) {
						return 4;
					}
				}

				// TODO: else
			}

			break;

		case BA_IM_SHR:
			// First arg GPR
			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				// GPR, CL
				if (im->vals[2] == BA_IM_CL) {
					return 3;
				}
				// GPR, IMM
				else if (im->vals[2] == BA_IM_IMM) {
					// Shift of 1
					if (im->vals[3] == 1) {
						return 3;
					}
					// 8 bits
					else if (im->vals[3] < 0x100) {
						return 4;
					}
				}

				// TODO: else
			}

			break;

		case BA_IM_MUL:
			// First arg GPR
			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				return 3;
			}

			break;

		case BA_IM_SYSCALL:
			return 2;

		case BA_IM_LABELJMP:
			// Pessimal
			return 5;

		default:
			printf("Error: unrecognized intermediate instruction: %#llx\n",
				im->vals[0]);
			exit(-1);
	}

	// Maximum x86_64 instruction size
	return 15;
}

#endif
