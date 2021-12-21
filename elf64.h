// See LICENSE for copyright/license information

#ifndef BA__BINARY_H
#define BA__BINARY_H

#include "sys/stat.h"
#include "common/common.h"

u8 ba_PessimalInstrSize(struct ba_IM* im);

u8 ba_WriteBinary(char* fileName, struct ba_Controller* ctr) {
	u64 tmp;

	u64 memStart = 0x400000;
	// Not the final entry point, the actual entry point will 
	// use this as an offset
	u64 entryPoint = memStart+0x1000;
	
	// Generate data segment
	u64 dataSgmtAddr = 0;

	struct ba_DynArr8* dataSgmt = ba_NewDynArr8(ctr->globalST->dataSize);
	dataSgmt->cnt = ctr->globalST->dataSize;

	for (u64 i = 0; i < ctr->globalST->capacity; i++) {
		struct ba_STEntry e = ctr->globalST->entries[i];
		if (e.key) {
			u64 sz = ba_GetSizeOfType(e.val->type);
			u64 initVal = (u64)e.val->initVal;
			for (u64 j = 0; j < sz; j++) {
				dataSgmt->arr[e.val->address+j] = initVal & 0xff;
				initVal >>= 8;
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
		(im == ctr->entryIM) && (entryPoint += code->cnt);

		switch (im->vals[0]) {
			case BA_IM_LABEL:
			{
				if (im->count < 2) {
					return ba_ErrorIMArgCount(1, im);
				}
				
				u64 labelID = im->vals[1];
				struct ba_IMLabel* lbl = &labels[labelID];
				
				if (labelID >= ctr->labelCnt || lbl->isFound) {
					printf("Error: cannot generate intermediate label %lld\n", 
						labelID);
					exit(-1);
				}

				lbl->addr = code->cnt;
				lbl->isFound = 1;
				
				if (lbl->jmpOfsts) {
					for (u64 i = 0; i < lbl->jmpOfsts->cnt; i++) {
						u64 addr = lbl->jmpOfsts->arr[i];
						u8 ripOffsetSize = lbl->jmpOfstSizes->arr[i];

						u64 ripOffset = lbl->addr - (addr + ripOffsetSize);
						for (u64 j = 0; j < ripOffsetSize; j++) {
							code->arr[addr+j] = ripOffset & 0xff;
							ripOffset >>= 8;
						}
					}
				}

				break;
			}

			case BA_IM_MOV:
			{
				if (im->count < 3) {
					return ba_ErrorIMArgCount(2, im);
				}

				u8 adrAddDestSize = 
					0xff * ((im->vals[1] == BA_IM_ADRADD) | 
						(im->vals[1] == BA_IM_ADRSUB)) +
					0x04 * ((im->vals[1] == BA_IM_64ADRADD) | 
						(im->vals[1] == BA_IM_64ADRSUB));

				// Into GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					u8 reg0 = im->vals[1] - BA_IM_RAX;
					u8 byte0 = 0x48;

					// GPR, GPR
					if ((BA_IM_RAX <= im->vals[2]) && (BA_IM_R15 >= im->vals[2])) {
						u8 byte2 = 0xc0;
						u8 reg1 = im->vals[2] - BA_IM_RAX;
						
						byte0 |= ((reg1 >= 8) << 2) | (reg0 >= 8);
						byte2 |= ((reg1 & 7) << 3) | (reg0 & 7);
						
						code->cnt += 3;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						code->arr[code->cnt-3] = byte0;
						code->arr[code->cnt-2] = 0x89;
						code->arr[code->cnt-1] = byte2;
					}

					// GPR, ADR GPR
					else if (im->vals[2] == BA_IM_ADR) {
						if (im->count < 4) {
							return ba_ErrorIMArgCount(2, im);
						}

						if (!(BA_IM_RAX <= im->vals[3]) || 
							!(BA_IM_R15 >= im->vals[2])) 
						{
							return ba_ErrorIMArgInvalid(im);
						}
					
						u8 byte2 = 0;
						u8 reg1 = im->vals[3] - BA_IM_RAX;
						
						byte0 |= ((reg0 >= 8) << 2) | (reg1 >= 8);
						byte2 |= ((reg0 & 7) << 3) | (reg1 & 7);
						
						if ((reg1 & 7) == 4 || (reg1 & 7) == 5) {
							code->cnt += 4;
							(code->cnt > code->cap) && ba_ResizeDynArr8(code);
							code->arr[code->cnt-4] = byte0;
							code->arr[code->cnt-3] = 0x8b;
							code->arr[code->cnt-2] = 
								((reg1 & 7) == 5) * 0x40 + byte2;
							code->arr[code->cnt-1] = 
								((reg1 & 7) == 4) * 0x24;
						}
						else {
							code->cnt += 3;
							(code->cnt > code->cap) && ba_ResizeDynArr8(code);
							code->arr[code->cnt-3] = byte0;
							code->arr[code->cnt-2] = 0x8b;
							code->arr[code->cnt-1] = byte2;
						}
					}

					// GPR, ADRADD/ADRSUB GPR
					else if (im->vals[2] == BA_IM_ADRADD || 
						im->vals[2] == BA_IM_ADRSUB) 
					{
						if (im->count < 5) {
							return ba_ErrorIMArgCount(2, im);
						}

						if (!(BA_IM_RAX <= im->vals[3]) || 
							!(BA_IM_R15 >= im->vals[2])) 
						{
							return ba_ErrorIMArgInvalid(im);
						}

						u8 sub = im->vals[2] == BA_IM_ADRSUB;
						u64 offset = im->vals[4];

						u8 byte2 = (offset != 0) * 0x40 + 
							(offset >= 0x80) * 0x40;
						u8 reg1 = im->vals[3] - BA_IM_RAX;
						
						byte0 |= ((reg0 >= 8) << 2) | (reg1 >= 8);
						byte2 |= ((reg0 & 7) << 3) | (reg1 & 7);

						u8 isReg1Mod4 = (reg1 & 7) == 4; // RSP or R12
						u64 ofstSz = (offset != 0) + (offset >= 0x80) * 3;
						
						code->cnt += 3 + ofstSz + isReg1Mod4;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						code->arr[code->cnt-ofstSz-3-isReg1Mod4] = byte0;
						code->arr[code->cnt-ofstSz-2-isReg1Mod4] = 0x8b;
						code->arr[code->cnt-ofstSz-1-isReg1Mod4] = byte2;
						(isReg1Mod4) && (code->arr[code->cnt-ofstSz-1] = 0x24);
						
						if (offset && offset < 0x80) {
							sub && (offset = -offset);
							code->arr[code->cnt-1] = offset & 0xff;
						}
						else if (offset >= 0x80) {
							sub && (offset = -offset);
							for (u64 i = 4; i > 0; i--) {
								code->arr[code->cnt-i] = offset & 0xff;
								offset >>= 8;
							}
						}
					}

					// GPR, DATASGMT
					else if (im->vals[2] == BA_IM_DATASGMT) {
						if (im->count < 4) {
							return ba_ErrorIMArgCount(2, im);
						}

						// Addr. rel. to start of data segment
						u64 imm = im->vals[3];
						u8 byte2 = 0x5 | ((reg0 & 7) << 3);
						byte0 |= (reg0 >= 8) << 2;

						// Ensure enough space in data sgmt addr related arrays
						(++relDSOffsets->cnt > relDSOffsets->cap) &&
							ba_ResizeDynArr64(relDSOffsets);
						(++relDSRips->cnt > relDSRips->cap) &&
							ba_ResizeDynArr64(relDSRips);

						// Where imm (data location) is stored; instruction ptr
						// (Looks like dereferencing NULL, but shouldn't occur)
						*ba_TopDynArr64(relDSOffsets) = code->cnt + 3;
						*ba_TopDynArr64(relDSRips) = code->cnt + 7;

						code->cnt += 7;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						code->arr[code->cnt-7] = byte0;
						code->arr[code->cnt-6] = 0x8b;
						code->arr[code->cnt-5] = byte2;
						for (u64 i = 4; i > 0; i--) {
							code->arr[code->cnt-i] = imm & 0xff;
							imm >>= 8;
						}
					}

					// GPR, IMM
					else if (im->vals[2] == BA_IM_IMM) {
						if (im->count < 4) {
							return ba_ErrorIMArgCount(2, im);
						}
						u64 imm = im->vals[3];
						u8 byte1 = 0xb8 | (reg0 & 7);

						// 32 bits
						if (imm < (1llu << 32)) {
							code->cnt += 5 + (reg0 >= 8);
							(code->cnt > code->cap) && ba_ResizeDynArr8(code);

							(reg0 >= 8) && (code->arr[code->cnt-6] = 0x41);
							code->arr[code->cnt-5] = byte1;
							for (u64 i = 4; i > 0; i--) {
								code->arr[code->cnt-i] = imm & 0xff;
								imm >>= 8;
							}
						}
						// 64 bits
						else {
							byte0 |= (reg0 >= 8);
							code->cnt += 10;
							(code->cnt > code->cap) && ba_ResizeDynArr8(code);

							code->arr[code->cnt-10] = byte0;
							code->arr[code->cnt-9] = byte1;
							for (u64 i = 8; i > 0; i--) {
								code->arr[code->cnt-i] = imm & 0xff;
								imm >>= 8;
							}
						}
					}

					else {
						return ba_ErrorIMArgInvalid(im);
					}
				}

				// Into GPRb
				else if ((BA_IM_AL <= im->vals[1]) && (BA_IM_R15B >= im->vals[1])) {
					// GPRb, IMM
					if (im->vals[2] == BA_IM_IMM) {
						if (im->count < 4) {
							return ba_ErrorIMArgCount(2, im);
						}
						u8 imm = im->vals[3];

						u8 byte0 = 0x40, byte1 = 0xb0;
						u8 reg0 = im->vals[1] - BA_IM_AL;

						byte0 |= (reg0 >= 8);
						byte1 |= (reg0 & 7);

						code->cnt += 2 + (reg0 >= 4);
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						(reg0 >= 4) && (code->arr[code->cnt-3] = byte0);
						code->arr[code->cnt-2] = byte1;
						code->arr[code->cnt-1] = imm & 0xff;
					}

					else {
						return ba_ErrorIMArgInvalid(im);
					}
				}
				
				// Into ADR GPR effective address
				else if (im->vals[1] == BA_IM_ADR) {
					if (im->count < 4) {
						return ba_ErrorIMArgCount(2, im);
					}

					if (!(BA_IM_RAX <= im->vals[2]) || !(BA_IM_R15 >= im->vals[2])) {
						return ba_ErrorIMArgInvalid(im);
					}
					
					u8 reg0 = im->vals[2] - BA_IM_RAX;
					
					// From GPRb
					if ((BA_IM_AL <= im->vals[3]) && (BA_IM_R15B >= im->vals[3])) {
						u8 reg1 = im->vals[3] - BA_IM_AL;
						
						u8 byte0 = 0x40 | (reg0 >= 8) | ((reg1 >= 8) << 2);
						u8 byte2 = (reg0 & 7) | ((reg1 & 7) << 3);

						u8 hasByte0 = (reg1 >= BA_IM_SPL - BA_IM_AL) || (reg0 >= 8);
						u8 instrSz = 2 + ((reg0 & 7) == 4) + ((reg0 & 7) == 5) + 
							hasByte0;

						code->cnt += instrSz;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						hasByte0 && (code->arr[code->cnt-instrSz] = byte0);
						code->arr[code->cnt-instrSz+hasByte0] = 0x88;
						code->arr[code->cnt-instrSz+hasByte0+1] = 
							byte2 + ((reg0 & 7) == 5) * 0x40;
						(((reg0 & 7) == 4) && (code->arr[code->cnt-1] = 0x24)) ||
						(((reg0 & 7) == 5) && (code->arr[code->cnt-1] = 0));
					}

					else {
						return ba_ErrorIMArgInvalid(im);
					}
				}

				// Into ADRADD/ADRSUB GPR effective address
				else if (adrAddDestSize) {
					if (im->count < 5) {
						return ba_ErrorIMArgCount(2, im);
					}

					if (!(BA_IM_RAX <= im->vals[2]) || !(BA_IM_R15 >= im->vals[2])) {
						return ba_ErrorIMArgInvalid(im);
					}

					u8 reg0 = im->vals[2] - BA_IM_RAX;
					u64 offset = im->vals[3];
					
					// From GPR
					if ((BA_IM_RAX <= im->vals[4]) && (BA_IM_R15 >= im->vals[4])) {
						u8 sub = im->vals[1] == BA_IM_ADRSUB ||
							im->vals[1] == BA_IM_64ADRSUB;

						u8 reg1 = im->vals[4] - BA_IM_RAX;
						u8 byte0 = 0x48 | (reg0 >= 8) | ((reg1 >= 8) << 3);

						u8 isOffsetOneByte = offset < 0x80;
						if (offset >= (1llu << 31)) {
							printf("Error: Effective address cannot have a more "
								"than 32 bit offset in instruction %s\n",
								ba_IMToStr(im));
							exit(-1);
						}

						sub && (offset = -offset);
						u8 byte2 = (0x40 << (!isOffsetOneByte)) | (reg0 & 7) |
							((reg1 & 7) << 3);

						u8 instrSz = 4 + 3 * (!isOffsetOneByte) + 
							((reg0 & 7) == 4);
						code->cnt += instrSz;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						code->arr[code->cnt-instrSz] = byte0;
						code->arr[code->cnt-instrSz+1] = 0x89;
						code->arr[code->cnt-instrSz+2] = byte2;
						((reg0 & 7) == 4) && (code->arr[code->cnt-instrSz+3] = 0x24);

						if (!isOffsetOneByte) {
							for (u64 i = 4; i > 1; i--) {
								code->arr[code->cnt-i] = offset & 0xff;
								offset >>= 8;
							}
						}
						code->arr[code->cnt-1] = offset & 0xff;
					}

					// From IMM
					else if (im->vals[4] == BA_IM_IMM) {
						if (adrAddDestSize == 0xff) {
							printf("Error: no size specified for MOV in "
								"instruction %s\n", ba_IMToStr(im));
							exit(-1);
						}

						if (im->count < 6) {
							return ba_ErrorIMArgCount(2, im);
						}

						u8 sub = im->vals[1] == BA_IM_ADRSUB ||
							im->vals[1] == BA_IM_64ADRSUB;
						u64 imm = im->vals[5];
						u8 byte0 = 0x48 | (reg0 >= 8);

						u8 isOffsetOneByte = offset < 0x80;
						if (offset >= (1llu << 31)) {
							printf("Error: Effective address cannot have a more "
								"than 32 bit offset in instruction %s\n", 
								ba_IMToStr(im));
							exit(-1);
						}

						u8 byte2 = (0x40 << !isOffsetOneByte) | (reg0 & 7);
						sub && (offset = -offset);

						u8 instrSz = 4 + 3 * (!isOffsetOneByte) + 
							adrAddDestSize + ((reg0 & 7) == 4);
						code->cnt += instrSz;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						code->arr[code->cnt-instrSz] = byte0;
						code->arr[code->cnt-instrSz+1] = 0xc7;
						code->arr[code->cnt-instrSz+2] = byte2;
						(reg0 & 7) && (code->arr[code->cnt-instrSz+3] = 0x24);
						
						if (!isOffsetOneByte) {
							for (u64 i = 4; i > 1; i--) {
								code->arr[code->cnt-i-adrAddDestSize] = offset & 0xff;
								offset >>= 8;
							}
						}
						code->arr[code->cnt-1-adrAddDestSize] = offset & 0xff;

						for (u64 i = adrAddDestSize; i-- > 0;) {
							code->arr[code->cnt-1-i] = imm & 0xff;
							imm >>= 8;
						}
					}

					else {
						return ba_ErrorIMArgInvalid(im);
					}
				}

				// Into DATASGMT
				else if (im->vals[1] == BA_IM_DATASGMT) {
					if (im->count < 4) {
						return ba_ErrorIMArgCount(2, im);
					}

					// DATASGMT, GPR
					if ((BA_IM_RAX <= im->vals[3]) && (BA_IM_R15 >= im->vals[3])) {
						// Addr. rel. to start of data segment
						u64 imm = im->vals[2];

						u8 reg0 = im->vals[3] - BA_IM_RAX;
						u8 byte0 = 0x48 | ((reg0 >= 8) << 2);
						u8 byte2 = 0x5 | ((reg0 & 7) << 3);

						// Ensure enough space in data sgmt addr related arrays
						(++relDSOffsets->cnt > relDSOffsets->cap) &&
							ba_ResizeDynArr64(relDSOffsets);
						(++relDSRips->cnt > relDSRips->cap) &&
							ba_ResizeDynArr64(relDSRips);

						// Where imm (data location) is stored; instruction ptr
						// (Looks like dereferencing NULL, but shouldn't occur)
						*ba_TopDynArr64(relDSOffsets) = code->cnt + 3;
						*ba_TopDynArr64(relDSRips) = code->cnt + 7;

						code->cnt += 7;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						code->arr[code->cnt-7] = byte0;
						code->arr[code->cnt-6] = 0x89;
						code->arr[code->cnt-5] = byte2;
						for (u64 i = 4; i > 0; i--) {
							code->arr[code->cnt-i] = imm & 0xff;
							imm >>= 8;
						}
					}
				}
				
				else {
					return ba_ErrorIMArgInvalid(im);
				}

				break;
			}

			case BA_IM_ADD: case BA_IM_SUB: case BA_IM_CMP: case BA_IM_AND: 
			case BA_IM_XOR: case BA_IM_OR:
			{
				char* instrName = 0;
				((im->vals[0] == BA_IM_ADD) && (instrName = "ADD")) ||
				((im->vals[0] == BA_IM_SUB) && (instrName = "SUB")) ||
				((im->vals[0] == BA_IM_CMP) && (instrName = "CMP")) ||
				((im->vals[0] == BA_IM_AND) && (instrName = "AND")) ||
				((im->vals[0] == BA_IM_OR) && (instrName = "OR")) ||
				((im->vals[0] == BA_IM_XOR) && (instrName = "XOR"));

				if (im->count < 3) {
					return ba_ErrorIMArgCount(2, im);
				}

				u8 byte1Offset = 0;
				((im->vals[0] == BA_IM_ADD) && (byte1Offset = 0)) || 
				((im->vals[0] == BA_IM_SUB) && (byte1Offset = 0x28)) || 
				((im->vals[0] == BA_IM_CMP) && (byte1Offset = 0x38)) || 
				((im->vals[0] == BA_IM_AND) && (byte1Offset = 0x20)) || 
				((im->vals[0] == BA_IM_OR) && (byte1Offset = 0x08)) || 
				((im->vals[0] == BA_IM_XOR) && (byte1Offset = 0x30));

				// First arg GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					u8 reg0 = im->vals[1] - BA_IM_RAX;

					// GPR, GPR
					if ((BA_IM_RAX <= im->vals[2]) && (BA_IM_R15 >= im->vals[2])) {
						u8 reg1 = im->vals[2] - BA_IM_RAX;

						u8 byte0 = 0x48 | ((reg1 >= 8) << 2) | (reg0 >= 8);
						u8 byte2 = 0xc0 | ((reg1 & 7) << 3) | (reg0 & 7);
						
						u8 hasRexW = (im->vals[0] != BA_IM_XOR) | 
							(reg0 != reg1) | (reg0 >= 8);
						code->cnt += 2 + hasRexW;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						hasRexW && (code->arr[code->cnt-3] = byte0);
						code->arr[code->cnt-2] = 0x1 + byte1Offset;
						code->arr[code->cnt-1] = byte2;
					}

					// GPR, IMM
					else if (im->vals[2] == BA_IM_IMM) {
						if (im->count < 4) {
							return ba_ErrorIMArgCount(2, im);
						}

						u64 imm = im->vals[3];
						u8 byte0 = 0x48 | (reg0 >= 8);
						u8 byte2 = (0xc0 + byte1Offset) | (reg0 & 7);

						// >31 bits
						if (imm >= (1llu << 31)) {
							printf("Error: Cannot %s more than 31 bits "
								"to a register in instruction %s\n", 
								instrName, ba_IMToStr(im));
							exit(-1);
						}

						code->cnt += 4 + (imm >= 0x80) * 
							(2 + (im->vals[1] != BA_IM_RAX));
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						// 7 bits
						if (imm < 0x80) {
							code->arr[code->cnt-4] = byte0;
							code->arr[code->cnt-3] = 0x83;
							code->arr[code->cnt-2] = byte2;
						}
						// 31 bits
						else {
							if (im->vals[1] == BA_IM_RAX) {
								code->arr[code->cnt-6] = 0x48;
								code->arr[code->cnt-5] = 0x5 + byte1Offset;
							}
							else {
								code->arr[code->cnt-7] = byte0;
								code->arr[code->cnt-6] = 0x81;
								code->arr[code->cnt-5] = byte2;
							}

							for (u64 i = 4; i > 1; i--) {
								code->arr[code->cnt-i] = imm & 0xff;
								imm >>= 8;
							}
						}
						code->arr[code->cnt-1] = imm & 0xff;
					}

					else {
						return ba_ErrorIMArgInvalid(im);
					}
				}

				// First arg GPRb
				else if ((BA_IM_AL <= im->vals[1]) && (BA_IM_R15B >= im->vals[1])) {
					// GPRb, GPRb
					if ((BA_IM_AL <= im->vals[2]) && (BA_IM_R15B >= im->vals[2])) {
						u8 reg0 = im->vals[1] - BA_IM_AL;
						u8 reg1 = im->vals[2] - BA_IM_AL;

						u8 byte0 = 0x40 | ((reg1 >= 8) << 2) | (reg0 >= 8);
						u8 byte2 = 0xc0 | ((reg1 & 7) << 3) | (reg0 & 7);

						u64 hasByte0 = ((BA_IM_SPL <= im->vals[1]) | 
							(BA_IM_SPL <= im->vals[2]));
						code->cnt += 2 + hasByte0;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);
						
						(hasByte0) && (code->arr[code->cnt-3] = byte0);
						code->arr[code->cnt-2] = byte1Offset;
						code->arr[code->cnt-1] = byte2;
					}

					else {
						return ba_ErrorIMArgInvalid(im);
					}
				}

				// Into ADRADD/ADRSUB GPR effective address
				else if ((im->vals[1] == BA_IM_ADRADD) || 
					(im->vals[1] == BA_IM_ADRSUB))
				{
					if (im->count < 5) {
						return ba_ErrorIMArgCount(2, im);
					}
					
					if (!(BA_IM_RAX <= im->vals[2]) || !(BA_IM_R15 >= im->vals[2])) {
						printf("Error: first argument for ADRADD/ADRSUB must be a "
							"general purpose register in instruction %s\n",
							ba_IMToStr(im));
						exit(-1);
					}
					
					// From GPR
					if ((BA_IM_RAX <= im->vals[4]) && (BA_IM_R15 >= im->vals[4])) {
						u8 sub = im->vals[1] == BA_IM_ADRSUB;

						u8 reg0 = im->vals[2] - BA_IM_RAX;
						u8 reg1 = im->vals[4] - BA_IM_RAX;
						u8 byte0 = 0x48 | (reg0 >= 8) | ((reg1 >= 8) << 3);
						u64 offset = im->vals[3];

						u8 isOffsetOneByte = offset < 0x80;
						if (offset >= (1llu << 31)) {
							printf("Error: Effective address cannot have a more "
								"than 32 bit offset in instruction %s\n", 
								ba_IMToStr(im));
							exit(-1);
						}

						u8 instrSz = 4 + 3 * (!isOffsetOneByte) + 
							((reg0 & 7) == 4);
						code->cnt += instrSz;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						sub && (offset = -offset);
						u8 byte2 = (0x40 << (!isOffsetOneByte)) | (reg0 & 7) | 
							((reg1 & 7) << 3);

						code->arr[code->cnt-instrSz] = byte0;
						code->arr[code->cnt-instrSz+1] = 0x01 + byte1Offset;
						code->arr[code->cnt-instrSz+2] = byte2;
						((reg0 & 7) == 4) && (code->arr[code->cnt-instrSz+3] = 0x24);

						if (!isOffsetOneByte) {
							for (u64 i = 4; i > 1; i--) {
								code->arr[code->cnt-i] = offset & 0xff;
								offset >>= 8;
							}
						}
						code->arr[code->cnt-1] = offset & 0xff;
					}

					else {
						return ba_ErrorIMArgInvalid(im);
					}
				}

				else {
					return ba_ErrorIMArgInvalid(im);
				}

				break;
			}

			case BA_IM_INC: case BA_IM_DEC: case BA_IM_NOT: case BA_IM_NEG:
			{
				u8 byte1;
				u8 byte2Offset;

				if (im->vals[0] == BA_IM_INC) {
					byte1 = 0xff;
					byte2Offset = 0;
				}
				else if (im->vals[0] == BA_IM_DEC) {
					byte1 = 0xff;
					byte2Offset = 0x8;
				}
				else if (im->vals[0] == BA_IM_NOT) {
					byte1 = 0xf7;
					byte2Offset = 0x10;
				}
				else if (im->vals[0] == BA_IM_NEG) {
					byte1 = 0xf7;
					byte2Offset = 0x18;
				}

				if (im->count < 2) {
					return ba_ErrorIMArgCount(1, im);
				}

				// GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					u8 reg0 = im->vals[1] - BA_IM_RAX;
					u8 byte0 = 0x48 | (reg0 >= 8);
					u8 byte2 = (0xc0 + byte2Offset) | (reg0 & 7);

					code->cnt += 3;
					(code->cnt > code->cap) && ba_ResizeDynArr8(code);

					code->arr[code->cnt-3] = byte0;
					code->arr[code->cnt-2] = byte1;
					code->arr[code->cnt-1] = byte2;
				}

				else {
					return ba_ErrorIMArgInvalid(im);
				}

				break;
			}

			case BA_IM_TEST:
			{
				if (im->count < 3) {
					return ba_ErrorIMArgCount(2, im);
				}

				// First arg GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					u8 reg0 = im->vals[1] - BA_IM_RAX;

					// GPR, GPR
					if ((BA_IM_RAX <= im->vals[2]) && (BA_IM_R15 >= im->vals[2])) {
						u8 reg1 = im->vals[2] - BA_IM_RAX;

						u8 byte0 = 0x48 | ((reg1 >= 8) << 2) | (reg0 >= 8);
						u8 byte2 = 0xc0 | ((reg1 & 7) << 3) | (reg0 & 7);
						
						code->cnt += 3;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						code->arr[code->cnt-3] = byte0;
						code->arr[code->cnt-2] = 0x85;
						code->arr[code->cnt-1] = byte2;
					}

					else {
						return ba_ErrorIMArgInvalid(im);
					}
				}
				// First arg GPRb
				else if ((BA_IM_AL <= im->vals[1]) && (BA_IM_R15B >= im->vals[1])) {
					// GPRb, GPRb
					if ((BA_IM_AL <= im->vals[2]) && (BA_IM_R15B >= im->vals[2])) {
						u8 reg0 = im->vals[1] - BA_IM_AL;
						u8 reg1 = im->vals[2] - BA_IM_AL;

						u8 byte0 = 0x40 | ((reg1 >= 8) << 2) | (reg0 >= 8);
						u8 byte2 = 0xc0 | ((reg1 & 7) << 3) | (reg0 & 7);

						u64 hasByte0 = ((BA_IM_SPL <= im->vals[1]) | 
							(BA_IM_SPL <= im->vals[2]));
						code->cnt += 2 + hasByte0;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);
						
						(hasByte0) && (code->arr[code->cnt-3] = byte0);
						code->arr[code->cnt-2] = 0x84;
						code->arr[code->cnt-1] = byte2;
					}

					else {
						return ba_ErrorIMArgInvalid(im);
					}
				}

				else {
					return ba_ErrorIMArgInvalid(im);
				}

				break;
			}

			case BA_IM_ROL: case BA_IM_ROR: 
			case BA_IM_SHL: case BA_IM_SHR: case BA_IM_SAR:
			{
				char* instrName = 0;
				((im->vals[0] == BA_IM_ROL) && (instrName = "ROL")) ||
				((im->vals[0] == BA_IM_ROR) && (instrName = "ROR")) ||
				((im->vals[0] == BA_IM_SHL) && (instrName = "SHL")) ||
				((im->vals[0] == BA_IM_SHR) && (instrName = "SHR")) ||
				((im->vals[0] == BA_IM_SAR) && (instrName = "SAR"));

				if (im->count < 3) {
					return ba_ErrorIMArgCount(2, im);
				}

				// First arg GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					u8 reg0 = im->vals[1] - BA_IM_RAX;
					u8 byte0 = 0x48 | (reg0 >= 8);
					u8 byte2 = (reg0 & 7);

					((im->vals[0] == BA_IM_ROL) && (byte2 |= 0xc0)) ||
					((im->vals[0] == BA_IM_ROR) && (byte2 |= 0xc8)) ||
					((im->vals[0] == BA_IM_SHL) && (byte2 |= 0xe0)) ||
					((im->vals[0] == BA_IM_SHR) && (byte2 |= 0xe8)) ||
					((im->vals[0] == BA_IM_SAR) && (byte2 |= 0xf8));

					// GPR, CL
					if (im->vals[2] == BA_IM_CL) {
						code->cnt += 3;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);
						code->arr[code->cnt-3] = byte0;
						code->arr[code->cnt-2] = 0xd3;
						code->arr[code->cnt-1] = byte2;
					}

					// GPR, IMM
					else if (im->vals[2] == BA_IM_IMM) {
						if (im->count < 4) {
							return ba_ErrorIMArgCount(2, im);
						}

						u64 imm = im->vals[3];
						if (imm >= 0x40) {
							printf("Error: Cannot SHL a register by more "
								"than 6 bits in instruction %s\n", 
								ba_IMToStr(im));
							exit(-1);
						}

						u64 instrSize = 3 + (imm != 1);
						code->cnt += instrSize;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						code->arr[code->cnt-instrSize] = byte0;
						code->arr[code->cnt-instrSize+1] = 0xc1 + 
							(imm == 1) * 0x10;
						code->arr[code->cnt-instrSize+2] = byte2;

						(imm != 1) && (code->arr[code->cnt-1] = imm & 0xff);
					}

					else {
						return ba_ErrorIMArgInvalid(im);
					}
				}

				else {
					return ba_ErrorIMArgInvalid(im);
				}

				break;
			}

			case BA_IM_MUL: case BA_IM_DIV: case BA_IM_IDIV:
			{
				if (im->count < 2) {
					return ba_ErrorIMArgCount(1, im);
				}

				// First arg GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					u8 reg0 = im->vals[1] - BA_IM_RAX;
					u8 byte0 = 0x48 | (reg0 >= 8);
					u8 byte2 = (reg0 & 7);

					((im->vals[0] == BA_IM_MUL) && (byte2 |= 0xe0)) ||
					((im->vals[0] == BA_IM_DIV) && (byte2 |= 0xf0)) ||
					((im->vals[0] == BA_IM_IDIV) && (byte2 |= 0xf8));

					code->cnt += 3;
					(code->cnt > code->cap) && ba_ResizeDynArr8(code);
					code->arr[code->cnt-3] = byte0;
					code->arr[code->cnt-2] = 0xf7;
					code->arr[code->cnt-1] = byte2;
				}

				else {
					return ba_ErrorIMArgInvalid(im);
				}

				break;
			}

			case BA_IM_IMUL:
			{
				if (im->count < 2) {
					return ba_ErrorIMArgCount(1, im);
				}

				// First arg GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					u8 reg0 = im->vals[1] - BA_IM_RAX;

					// GPR, GPR
					if ((BA_IM_RAX <= im->vals[2]) && (BA_IM_R15 >= im->vals[2])) {
						u8 reg1 = im->vals[2] - BA_IM_RAX;

						u8 byte0 = 0x48 | ((reg0 >= 8) << 2) | (reg1 >= 8);
						u8 byte2 = 0xc0 | ((reg0 & 7) << 3) | (reg1 & 7);
						
						code->cnt += 4;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						code->arr[code->cnt-4] = byte0;
						code->arr[code->cnt-3] = 0x0f;
						code->arr[code->cnt-2] = 0xaf;
						code->arr[code->cnt-1] = byte2;
					}

					else {
						return ba_ErrorIMArgInvalid(im);
					}
				}

				else {
					return ba_ErrorIMArgInvalid(im);
				}

				break;
			}

			case BA_IM_SYSCALL:
			{
				code->cnt += 2;
				(code->cnt > code->cap) && ba_ResizeDynArr8(code);
				code->arr[code->cnt-2] = 0x0f;
				code->arr[code->cnt-1] = 0x05;
				break;
			}

			case BA_IM_LABELCALL: case BA_IM_LABELJMP: case BA_IM_LABELJZ: 
			case BA_IM_LABELJNZ: case BA_IM_LABELJB: case BA_IM_LABELJBE: 
			case BA_IM_LABELJA: case BA_IM_LABELJAE: case BA_IM_LABELJL: 
			case BA_IM_LABELJLE: case BA_IM_LABELJG: case BA_IM_LABELJGE: 
			{
				enum {
					_INSTRTYPE_CALL,
					_INSTRTYPE_JMP,
					_INSTRTYPE_JCC,
				}
				instrType = _INSTRTYPE_JCC;
				u8 opCodeShort = 0; // Doesn't exist for CALL
				u8 opCodeNear = 0; // Not needed for Jcc, it's just short+0x10

				if (im->vals[0] == BA_IM_LABELCALL) {
					instrType = _INSTRTYPE_CALL;
					opCodeNear = 0xe8;
				}
				else if (im->vals[0] == BA_IM_LABELJMP) {
					instrType = _INSTRTYPE_JMP;
					opCodeShort = 0xeb;
					opCodeNear = 0xe9;
				}
				else {
					(im->vals[0] == BA_IM_LABELJZ && (opCodeShort = 0x74)) ||
					(im->vals[0] == BA_IM_LABELJNZ && (opCodeShort = 0x75)) ||
					(im->vals[0] == BA_IM_LABELJB && (opCodeShort = 0x72)) ||
					(im->vals[0] == BA_IM_LABELJBE && (opCodeShort = 0x76)) ||
					(im->vals[0] == BA_IM_LABELJA && (opCodeShort = 0x77)) ||
					(im->vals[0] == BA_IM_LABELJAE && (opCodeShort = 0x73)) ||
					(im->vals[0] == BA_IM_LABELJL && (opCodeShort = 0x7c)) ||
					(im->vals[0] == BA_IM_LABELJLE && (opCodeShort = 0x7e)) ||
					(im->vals[0] == BA_IM_LABELJG && (opCodeShort = 0x7f)) ||
					(im->vals[0] == BA_IM_LABELJGE && (opCodeShort = 0x7d));
				}

				(instrType == _INSTRTYPE_JCC) && 
					(opCodeNear = opCodeShort + 0x10);

				if (im->count < 2) {
					return ba_ErrorIMArgCount(1, im);
				}

				u64 labelID = im->vals[1];
				struct ba_IMLabel* lbl = &labels[labelID];

				if (labelID >= ctr->labelCnt) {
					printf("Error: cannot find intermediate label %lld "
						"in instruction %s\n", labelID, ba_IMToStr(im));
					exit(-1);
				}

				// Label appears before jmp
				if (lbl->isFound) {
					// Assumes the instruction will be 2 bytes initially
					// (so that the check afterwards works properly)
					i64 relAddr = lbl->addr - (code->cnt + 2);
					u64 instrSize = 2;

					if (instrType == _INSTRTYPE_CALL ||
						!((relAddr >= 0 && relAddr < 0x80) ||
						(relAddr < 0ll && relAddr < -0x80ll)))
					{
						relAddr -= 3 + (instrType == _INSTRTYPE_JCC);
						instrSize += 3 + (instrType == _INSTRTYPE_JCC);
					}

					code->cnt += instrSize;
					(code->cnt > code->cap) && ba_ResizeDynArr8(code);

					(instrType == _INSTRTYPE_JCC && instrSize == 6) && 
						(code->arr[code->cnt-6] = 0x0f);
					code->arr[code->cnt-instrSize + 
						(instrType == _INSTRTYPE_JCC)] = 
						(instrSize == 2) ? opCodeShort : opCodeNear;

					if (instrSize > 2) {
						for (u64 i = 4; i > 1; i--) {
							code->arr[code->cnt-i] = relAddr & 0xff;
							relAddr >>= 8;
						}
					}
					code->arr[code->cnt-1] = relAddr & 0xff;
				}
				// Label appears after jmp
				else {
					u64 labelDistance = 0; // Only used with LABELJMP
					
					if (instrType != _INSTRTYPE_CALL) {
						struct ba_IM* tmpIM = im;
						while (tmpIM && tmpIM->count) {
							labelDistance += ba_PessimalInstrSize(tmpIM);
							tmpIM = tmpIM->next;
							if (labelDistance >= 0x80 /* near jmp */ || 
								(tmpIM->vals[0] == BA_IM_LABEL &&
								tmpIM->vals[1] == labelID) /* short jmp*/)
							{
								break;
							}
						}
					}

					// Don't check for lbl->jmpOfstSizes, they are allocated together
					if (!lbl->jmpOfsts) {
						lbl->jmpOfsts = ba_NewDynArr64(0x40);
						lbl->jmpOfstSizes = ba_NewDynArr8(0x40);
					}

					(++lbl->jmpOfsts->cnt > lbl->jmpOfsts->cap) &&
						ba_ResizeDynArr64(lbl->jmpOfsts);
					(++lbl->jmpOfstSizes->cnt > lbl->jmpOfsts->cap) &&
						ba_ResizeDynArr8(lbl->jmpOfstSizes);

					if (instrType != _INSTRTYPE_CALL && labelDistance < 0x80) {
						*ba_TopDynArr64(lbl->jmpOfsts) = code->cnt + 1;
					
						code->cnt += 2;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						*ba_TopDynArr8(lbl->jmpOfstSizes) = 1;

						code->arr[code->cnt-2] = opCodeShort;
					}
					else {
						*ba_TopDynArr64(lbl->jmpOfsts) = code->cnt + 1 + 
							(instrType == _INSTRTYPE_JCC);
					
						code->cnt += 5 + (instrType == _INSTRTYPE_JCC);
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						*ba_TopDynArr8(lbl->jmpOfstSizes) = 4;

						(instrType == _INSTRTYPE_JCC) && 
							(code->arr[code->cnt-6] = 0x0f);
						code->arr[code->cnt-5] = opCodeNear;
					}
				}

				break;
			}

			case BA_IM_RET:
			{
				code->cnt += 1;
				(code->cnt > code->cap) && ba_ResizeDynArr8(code);
				code->arr[code->cnt-1] = 0xc3;
				break;
			}

			case BA_IM_POP: case BA_IM_PUSH: 
			{
				u8 isInstrPop = im->vals[0] == BA_IM_POP;

				if (im->count < 2) {
					return ba_ErrorIMArgCount(1, im);
				}

				// GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					u8 reg0 = im->vals[1] - BA_IM_RAX;
					u8 byte2 = 0x50 | (isInstrPop * 8) | (reg0 & 7);

					code->cnt += 1 + (reg0 >= 8);
					(code->cnt > code->cap) && ba_ResizeDynArr8(code);
					(reg0 >= 8) && (code->arr[code->cnt-2] = 0x41);
					code->arr[code->cnt-1] = byte2;
				}
				// DATASGMT
				else if (im->vals[1] == BA_IM_DATASGMT) {
					if (im->count < 3) {
						return ba_ErrorIMArgCount(1, im);
					}

					// Addr. rel. to start of data segment
					u64 imm = im->vals[2];

					// Ensure enough space in data sgmt addr related arrays
					(++relDSOffsets->cnt > relDSOffsets->cap) &&
						ba_ResizeDynArr64(relDSOffsets);
					(++relDSRips->cnt > relDSRips->cap) &&
						ba_ResizeDynArr64(relDSRips);

					// Where imm (data location) is stored; instruction ptr
					// (Looks like dereferencing NULL, but shouldn't occur)
					*ba_TopDynArr64(relDSOffsets) = code->cnt + 2;
					*ba_TopDynArr64(relDSRips) = code->cnt + 6;

					code->cnt += 6;
					(code->cnt > code->cap) && ba_ResizeDynArr8(code);

					if (isInstrPop) {
						code->arr[code->cnt-6] = 0x8f;
						code->arr[code->cnt-5] = 0x05;
					}
					else {
						code->arr[code->cnt-6] = 0xff;
						code->arr[code->cnt-5] = 0x35;
					}

					for (u64 i = 4; i > 0; i--) {
						code->arr[code->cnt-i] = imm & 0xff;
						imm >>= 8;
					}
				}
				// IMM
				else if ((!isInstrPop) && (im->vals[1] == BA_IM_IMM)) {
					if (im->count < 3) {
						return ba_ErrorIMArgCount(1, im);
					}

					u64 imm = im->vals[2];
					
					code->cnt += 2 + 3 * (imm >= 0x80);
					(code->cnt > code->cap) && ba_ResizeDynArr8(code);
					if (imm < 0x80) {
						code->arr[code->cnt-2] = 0x6a;
					}
					else {
						code->arr[code->cnt-5] = 0x68;
						for (u64 i = 4; i > 1; i--) {
							code->arr[code->cnt-i] = imm & 0xff;
							imm >>= 8;
						}
					}
					code->arr[code->cnt-1] = imm & 0xff;
				}
				else {
					return ba_ErrorIMArgInvalid(im);
				}

				break;
			}

			case BA_IM_MOVZX:
			{
				if (im->count < 3) {
					return ba_ErrorIMArgCount(2, im);
				}

				// Into GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					// GPR, GPRb
					if ((BA_IM_AL <= im->vals[2]) && (BA_IM_R15B >= im->vals[2])) {
						u8 reg0 = im->vals[1] - BA_IM_RAX;
						u8 reg1 = im->vals[2] - BA_IM_AL;
						
						u8 byte0 = 0x48 | ((reg0 >= 8) << 2) | (reg1 >= 8);
						u8 byte3 = 0xc0 | ((reg0 & 7) << 3) | (reg1 & 7);

						code->cnt += 4;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);
						code->arr[code->cnt-4] = byte0;
						code->arr[code->cnt-3] = 0x0f;
						code->arr[code->cnt-2] = 0xb6;
						code->arr[code->cnt-1] = byte3;
					}
					
					else {
						return ba_ErrorIMArgInvalid(im);
					}
				}
				
				else {
					return ba_ErrorIMArgInvalid(im);
				}

				break;
			}

			case BA_IM_SETS: case BA_IM_SETNS: case BA_IM_SETZ: case BA_IM_SETNZ: 
			case BA_IM_SETB: case BA_IM_SETBE: case BA_IM_SETA: case BA_IM_SETAE: 
			case BA_IM_SETL: case BA_IM_SETLE: case BA_IM_SETG: case BA_IM_SETGE: 
			{
				if (im->count < 2) {
					return ba_ErrorIMArgCount(1, im);
				}

				// GPRb
				if ((BA_IM_AL <= im->vals[1]) && (BA_IM_R15B >= im->vals[1])) {
					u8 reg0 = im->vals[1] - BA_IM_AL;
					u8 byte0 = 0x40 | (reg0 >= 8);
					u8 byte2 = 0xc0 | (reg0 & 7);

					u8 byte1 = 0;
					((im->vals[0] == BA_IM_SETS) && (byte1 = 0x98)) ||
					((im->vals[0] == BA_IM_SETNS) && (byte1 = 0x99)) ||
					((im->vals[0] == BA_IM_SETZ) && (byte1 = 0x94)) ||
					((im->vals[0] == BA_IM_SETNZ) && (byte1 = 0x95)) ||
					((im->vals[0] == BA_IM_SETB) && (byte1 = 0x92)) ||
					((im->vals[0] == BA_IM_SETBE) && (byte1 = 0x96)) ||
					((im->vals[0] == BA_IM_SETA) && (byte1 = 0x97)) ||
					((im->vals[0] == BA_IM_SETAE) && (byte1 = 0x93)) ||
					((im->vals[0] == BA_IM_SETL) && (byte1 = 0x9c)) ||
					((im->vals[0] == BA_IM_SETLE) && (byte1 = 0x9e)) ||
					((im->vals[0] == BA_IM_SETG) && (byte1 = 0x9f)) ||
					((im->vals[0] == BA_IM_SETGE) && (byte1 = 0x9d));

					code->cnt += 3 + (reg0 >= 4);
					(code->cnt > code->cap) && ba_ResizeDynArr8(code);

					(reg0 >= 4) && (code->arr[code->cnt-4] = byte0);
					code->arr[code->cnt-3] = 0x0f;
					code->arr[code->cnt-2] = byte1;
					code->arr[code->cnt-1] = byte2;
				}
				else {
					return ba_ErrorIMArgInvalid(im);
				}

				break;
			}

			case BA_IM_CQO:
			{
				if (im->count < 1) {
					return ba_ErrorIMArgCount(0, im);
				}
				code->cnt += 2;
				(code->cnt > code->cap) && ba_ResizeDynArr8(code);
				code->arr[code->cnt-2] = 0x48;
				code->arr[code->cnt-1] = 0x99;
				break;
			}
			default:
				printf("Error: unrecognized intermediate instruction: %#llx\n",
					im->vals[0]);
				exit(-1);
		}

		im = ba_DelIM(im);
	}

	if (labels->jmpOfsts) {
		ba_DelDynArr64(labels->jmpOfsts);
		ba_DelDynArr8(labels->jmpOfstSizes);
	}
	free(labels);

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
		u64 ripRelAddr = dataSgmtAddr - relDSRips->arr[i] - 0x1000 + 
			code->arr[codeLoc] + (code->arr[codeLoc+1] << 8) +
			(code->arr[codeLoc+2] << 16) + (code->arr[codeLoc+3] << 24);
		for (u64 loc = codeLoc; loc < codeLoc+4; loc++) {
			code->arr[loc] = ripRelAddr & 0xff;
			ripRelAddr >>= 8;
		}
	}

	// Generate file header
	u8 fileHeader[64] = {
		0x7f, 'E',  'L',  'F',  2,    1,    1,    3,
		0,    0,    0,    0,    0,    0,    0,    0,
		2,    0,    62,   0,    1,    0,    0,    0,
		0,    0,    0,    0,    0,    0,    0,    0,    // {entryPoint}
		0x40, 0,    0,    0,    0,    0,    0,    0,
		0,    0,    0,    0,    0,    0,    0,    0,    // No section headers
		0,    0,    0,    0,    0x40, 0,    0x38, 0,
		3,    0,    0,    0,    0,    0,    0,    0,    // 3 p header entries
	};

	// Set entry point in header
	tmp = entryPoint;
	for (u64 i = 0; i < 8; i++) {
		fileHeader[0x18+i] = tmp & 0xff;
		tmp >>= 8;
	}

	// Generate program headers
	enum {
		phSz = 3 * 0x38
	};
	u8 programHeader[phSz] = {
		1,    0,    0,    0,    4,    0,    0,    0,    // LOAD, Read
		0,    0,    0,    0,    0,    0,    0,    0,    // 0x0000 in file
		0,    0,    0,    0,    0,    0,    0,    0,    // {memStart}
		0,    0,    0,    0,    0,    0,    0,    0,    // "
		0xe8, 0,    0,    0,    0,    0,    0,    0,    // f.h.sz (0x40) + p.h.sz (3 * 0x56)
		0xe8, 0,    0,    0,    0,    0,    0,    0,    // "
		0,    0x10, 0,    0,    0,    0,    0,    0,    // 0x1000 offset

		1,    0,    0,    0,    5,    0,    0,    0,    // LOAD, Read+Execute
		0,    0,    0,    0,    0,    0,    0,    0,    // {entryPoint-memStart}
		0,    0,    0,    0,    0,    0,    0,    0,    // {entryPoint}
		0,    0,    0,    0,    0,    0,    0,    0,    // "
		0,    0,    0,    0,    0,    0,    0,    0,    // {code->cnt}
		0,    0,    0,    0,    0,    0,    0,    0,    // "
		0,    0x10, 0,    0,    0,    0,    0,    0,    // 0x1000 offset

		1,    0,    0,    0,    6,    0,    0,    0,    // LOAD, Read+Write
		0,    0,    0,    0,    0,    0,    0,    0,    // {dataSgmtAddr}
		0,    0,    0,    0,    0,    0,    0,    0,    // {memStart+dataSgmtAddr} in mem
		0,    0,    0,    0,    0,    0,    0,    0,    // "
		0,    0,    0,    0,    0,    0,    0,    0,    // {dataSgmt->cnt}
		0,    0,    0,    0,    0,    0,    0,    0,    // "
		0,    0x10, 0,    0,    0,    0,    0,    0,    // 0x1000 offset
	};

	for (u64 i = 0; i < 16; i += 8) {
		// {memStart} in first header
		tmp = memStart;
		for (u64 j = 0; j < 8; j++) {
			programHeader[0x10+i+j] = tmp & 0xff;
			tmp >>= 8;
		}
		// {entryPoint} in second header (code)
		tmp = entryPoint;
		for (u64 j = 0; j < 8; j++) {
			programHeader[0x48+i+j] = tmp & 0xff;
			tmp >>= 8;
		}
		// {code->cnt} in second header (code)
		tmp = code->cnt;
		for (u64 j = 0; j < 8; j++) {
			programHeader[0x58+i+j] = tmp & 0xff;
			tmp >>= 8;
		}
	}

	// {entryPoint-memStart} in second header (code)
	tmp = entryPoint-memStart;
	for (u64 i = 0; i < 8; i++) {
		programHeader[0x40+i] = tmp & 0xff;
		tmp >>= 8;
	}

	// {dataSgmtAddr} in third header (data)
	tmp = dataSgmtAddr;
	for (u64 i = 0; i < 8; i++) {
		programHeader[0x78+i] = tmp & 0xff;
		tmp >>= 8;
	}

	for (u64 i = 0; i < 16; i += 8) {
		// {memStart+dataSgmtAddr} in third header (data)
		tmp = memStart + dataSgmtAddr;
		for (u64 j = 0; j < 8; j++) {
			programHeader[0x80+i+j] = tmp & 0xff;
			tmp >>= 8;
		}
		// {dataSgmt->cnt} in third header (data)
		tmp = dataSgmt->cnt;
		for (u64 j = 0; j < 8; j++) {
			programHeader[0x90+i+j] = tmp & 0xff;
			tmp >>= 8;
		}
	}
	
	// String this together into a file
	
	// Create the initial file
	FILE* file;
	u8 isFileStdout = !strcmp(fileName, "-");

	if (isFileStdout) {
		file = stdout;
	}
	else {
		file = fopen(fileName, "wb");
		if (!file) {
			return 0;
		}
		fclose(file);

		// Create the file for appending
		file = fopen(fileName, "ab");
		if (!file) {
			return 0;
		}
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

	if (!isFileStdout) {
		fclose(file);
		chmod(fileName, 0755);
	}

	ba_DelDynArr8(dataSgmt);
	ba_DelDynArr64(relDSOffsets);
	ba_DelDynArr64(relDSRips);
	ba_DelDynArr8(code);

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
		{
			u8 adrAddDestSize = 
				0xff * ((im->vals[1] == BA_IM_ADRADD) | 
					(im->vals[1] == BA_IM_ADRSUB)) +
				0x04 * ((im->vals[1] == BA_IM_64ADRADD) | 
					(im->vals[1] == BA_IM_64ADRSUB));

			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				if ((BA_IM_RAX <= im->vals[2]) && (BA_IM_R15 >= im->vals[2])) {
					return 3;
				}
				else if (im->vals[2] == BA_IM_ADR) {
					u8 reg1 = im->vals[1] - BA_IM_RAX;
					return 3 + ((reg1 & 7) == 4);
				}
				else if (im->vals[2] == BA_IM_ADRADD || 
					im->vals[2] == BA_IM_ADRSUB) 
				{
					u64 offset = im->vals[4];
					u8 reg1 = im->vals[3] - BA_IM_RAX;
					u64 ofstSz = (offset != 0) + (offset >= 0x80) * 3;
					return 3 + ((reg1 & 7) == 4 || (reg1 & 7) == 5) + ofstSz;
				}
				else if (im->vals[2] == BA_IM_DATASGMT) {
					return 7;
				}
				else if (im->vals[2] == BA_IM_IMM) {
					u64 imm = im->vals[3];
					if (imm < (1llu << 32)) {
						u8 reg0 = im->vals[1] - BA_IM_RAX;
						return 5 + (reg0 >= 8);
					}
					return 10;
				}
			}
			// Into ADR GPR effective address
			else if (im->vals[1] == BA_IM_ADR) {
				u8 reg0 = im->vals[2] - BA_IM_RAX;
				if ((BA_IM_AL <= im->vals[3]) && (BA_IM_R15B >= im->vals[3])) {
					u8 reg1 = im->vals[3] - BA_IM_AL;
					u8 hasByte0 = (reg1 >= BA_IM_SPL - BA_IM_AL) || (reg0 >= 8);
					return 2 + ((reg0 & 7) == 4) + ((reg0 & 7) == 5) + 
						hasByte0;
				}
			}
			else if (adrAddDestSize) {
				u8 reg0 = im->vals[2] - BA_IM_RAX;
				u64 offset = im->vals[3];

				if ((BA_IM_RAX <= im->vals[4]) && (BA_IM_R15 >= im->vals[4])) {
					return 4 + 3 * (offset >= 0x80) + ((reg0 & 7) == 4);
				}

				else if (im->vals[4] == BA_IM_IMM) {
					return adrAddDestSize + 4 + 3 * (offset >= 0x80) + 
						((reg0 & 7) == 4);
				}
			}
			else if (im->vals[1] == BA_IM_DATASGMT) {
				if ((BA_IM_RAX <= im->vals[3]) && (BA_IM_R15 >= im->vals[3])) {
					return 7;
				}
			}

			break;
		}
		case BA_IM_ADD: case BA_IM_SUB: case BA_IM_CMP: case BA_IM_AND: 
		case BA_IM_XOR: case BA_IM_OR:
			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				if ((BA_IM_RAX <= im->vals[2]) && (BA_IM_R15 >= im->vals[2])) {
					return 3;
				}
				else if (im->vals[2] == BA_IM_IMM) {
					return 4 + (im->vals[3] >= 0x80) * 2;
				}
			}
			else if ((im->vals[1] == BA_IM_ADRADD) || (im->vals[1] == BA_IM_ADRSUB)) {
				if ((BA_IM_RAX <= im->vals[4]) && (BA_IM_R15 >= im->vals[4])) {
					u8 reg0 = im->vals[1] - BA_IM_RAX;
					return 4 + 3 * (im->vals[3] >= 0x80) + ((reg0 & 7) == 4);
				}
			}

			break;

		case BA_IM_INC: case BA_IM_DEC: case BA_IM_NOT: case BA_IM_NEG:
			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				return 3;
			}

			break;

		case BA_IM_TEST:
			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				if ((BA_IM_RAX <= im->vals[2]) && (BA_IM_R15 >= im->vals[2])) {
					return 3;
				}
			}
			else if ((BA_IM_AL <= im->vals[1]) && (BA_IM_R15B >= im->vals[1])) {
				if ((BA_IM_AL <= im->vals[2]) && (BA_IM_R15B >= im->vals[2])) {
					return 2 + ((BA_IM_SPL <= im->vals[1]) | 
						(BA_IM_SPL <= im->vals[2]));
				}
			}

			break;

		case BA_IM_ROL: case BA_IM_ROR: 
		case BA_IM_SHL: case BA_IM_SHR: case BA_IM_SAR:
			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				if (im->vals[2] == BA_IM_CL) {
					return 3;
				}
				else if (im->vals[2] == BA_IM_IMM) {
					if (im->vals[3] == 1) {
						return 3;
					}
					return 4;
				}
			}

			break;

		case BA_IM_MUL: case BA_IM_DIV: case BA_IM_IDIV:
			// First arg GPR
			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				return 3;
			}
			break;

		case BA_IM_IMUL:
			// First arg GPR
			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				// GPR, GPR
				if ((BA_IM_RAX <= im->vals[2]) && (BA_IM_R15 >= im->vals[2])) {
					return 4;
				}
			}
			break;

		case BA_IM_SYSCALL:
			return 2;

		case BA_IM_RET:
			return 1;

		case BA_IM_PUSH: case BA_IM_POP:
		{
			// GPR
			if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
				u8 reg0 = im->vals[1] - BA_IM_RAX;
				return 1 + (reg0 >= 8);
			}
			// DATASGMT
			else if (im->vals[1] == BA_IM_DATASGMT) {
				return 6;
			}
			// IMM
			else if (im->vals[1] == BA_IM_IMM) {
				u64 imm = im->vals[2];
				return 2 + (imm >= 0x80llu) * 3;
			}

			break;
		}

		// JMP/Jcc instructions are all calculated pessimally
		case BA_IM_LABELCALL: case BA_IM_LABELJMP:
			return 5;
		case BA_IM_LABELJNZ: case BA_IM_LABELJZ: case BA_IM_LABELJB: 
		case BA_IM_LABELJBE: case BA_IM_LABELJA: case BA_IM_LABELJAE:
		case BA_IM_LABELJL: case BA_IM_LABELJLE: case BA_IM_LABELJG: 
		case BA_IM_LABELJGE:
			return 6;
		
		case BA_IM_MOVZX:
			return 4;

		case BA_IM_SETS: case BA_IM_SETNS: case BA_IM_SETZ: case BA_IM_SETNZ:
		case BA_IM_SETB: case BA_IM_SETBE: case BA_IM_SETA: case BA_IM_SETAE: 
		case BA_IM_SETL: case BA_IM_SETLE: case BA_IM_SETG: case BA_IM_SETGE: 
		{
			// GPRb
			if ((BA_IM_AL <= im->vals[1]) && (BA_IM_R15B >= im->vals[1])) {
				u8 reg0 = im->vals[1] - BA_IM_AL;
				return 3 + (reg0 >= 4);
			}
		}

		case BA_IM_CQO:
			return 2;

		default:
			printf("Error: unrecognized intermediate instruction: %#llx\n",
				im->vals[0]);
			exit(-1);
	}

	// Maximum x86_64 instruction size
	return 15;
}

#endif
