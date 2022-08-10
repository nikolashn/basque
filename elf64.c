// See LICENSE for copyright/license information

#include "elf64.h"
#include "common/parser.h"
#include "common/func.h"

void ba_EmplaceFuncs(struct ba_Ctr* ctr, struct ba_SymTable* scope) {
	for (u64 i = 0; i < scope->ht->capacity; i++) {
		struct ba_HTEntry e = scope->ht->entries[i];
		struct ba_STVal* val = (struct ba_STVal*)e.val;
		if (val && e.key && val->type.type == BA_TYPE_FUNC) {
			struct ba_Func* func = val->type.extraInfo;
			if (func->isCalled) {
				memcpy(ctr->im, func->imBegin, sizeof(*ctr->im));
				ctr->im = func->imEnd;
			}
		}
	}
	for (u64 i = 0; i < scope->childCnt; ++i) {
		ba_EmplaceFuncs(ctr, scope->children[i]);
	}
}

u8 ba_WriteBinary(char* fileName, struct ba_Ctr* ctr) {
	// Put static segment in its place
	u64 staticSize = 0;
	for (u64 i = 0; i < ctr->statics->cnt; ++i) {
		struct ba_Static* staticPtr = (void*)ctr->statics->arr[i];
		if (staticPtr->isUsed) {
			staticPtr->offset = staticSize;
			staticSize += staticPtr->arr->cnt;
		}
		else {
			free(staticPtr);
		}
	}

	if (!ba_GetPageSize()) {
		ba_SetPageSize(sysconf(_SC_PAGE_SIZE));
	}

	u64 pageSz = ba_GetPageSize();
	u64 memStart = 0x400000;
	u64 phCnt = staticSize ? 3 : 2;
	u64 pHeaderSz = phCnt * 0x38; // Program header size
	u64 fHeaderSz = 0x40 + pHeaderSz; // File header size
	/* Not the final entry point, the actual entry point will 
	 * use this as an offset */
	u64 entryPoint = memStart + pageSz;
	
	// Put functions in their place
	ba_EmplaceFuncs(ctr, ctr->globalST);

	// Addresses are relative to the start of the code segment
	struct ba_IMLabel* labels = ba_CAlloc(ctr->labelCnt, sizeof(*labels));
	struct ba_Stk* movStaticStk = ba_NewStk();

	// Generate binary code
	struct ba_DynArr8* code = ba_NewDynArr8(0x1000);

	struct ba_IM* im = ctr->startIM;
	while (im && im->count) {
		(im == ctr->entryIM) && (entryPoint += code->cnt);

		//printf("%s\n", ba_IMToStr(im)); // DEBUG
		
		BA_LBL_WRITECODELOOPSTART:
		switch (im->vals[0]) {
			case BA_IM_NOP:
			{
				++code->cnt;
				code->arr[code->cnt-1] = 0x90;
				break;
			}

			case BA_IM_LABEL:
			{
				if (im->count < 2) {
					return ba_ErrorIMArgCount(2, im);
				}
				
				u64 labelID = im->vals[1];
				struct ba_IMLabel* lbl = &labels[labelID];
				
				if (labelID >= ctr->labelCnt || lbl->isFound) {
					fprintf(stderr, "Error: cannot generate intermediate "
						"label %lld\n", labelID);
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
					return ba_ErrorIMArgCount(3, im);
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
							return ba_ErrorIMArgCount(4, im);
						}

						if (!(BA_IM_RAX <= im->vals[3]) || 
							!(BA_IM_R15 >= im->vals[3])) 
						{
							return ba_ErrorIMArgInvalid(im);
						}
					
						u8 byte2 = 0;
						u8 reg1 = im->vals[3] - BA_IM_RAX;
						
						byte0 |= ((reg0 >= 8) << 2) | (reg1 >= 8);
						byte2 |= ((reg0 & 7) << 3) | (reg1 & 7);
						
						u64 codeSz = 3 + ((reg1 & 7) == 4) + ((reg1 & 7) == 5);
						code->cnt += codeSz;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						code->arr[code->cnt-codeSz] = byte0;
						code->arr[code->cnt-codeSz+1] = 0x8b;
						if ((reg1 & 7) == 4 || (reg1 & 7) == 5) {
							code->arr[code->cnt-2] = 
								((reg1 & 7) == 5) * 0x40 + byte2;
							code->arr[code->cnt-1] = 
								((reg1 & 7) == 4) * 0x24;
						}
						else {
							code->arr[code->cnt-1] = byte2;
						}
					}

					// GPR, ADRADD/ADRSUB GPR
					else if (im->vals[2] == BA_IM_ADRADD || 
						im->vals[2] == BA_IM_ADRSUB) 
					{
						if (im->count < 5) {
							return ba_ErrorIMArgCount(5, im);
						}

						if (!(BA_IM_RAX <= im->vals[3]) || 
							!(BA_IM_R15 >= im->vals[3])) 
						{
							return ba_ErrorIMArgInvalid(im);
						}

						bool sub = im->vals[2] == BA_IM_ADRSUB;
						u64 offset = im->vals[4];

						u8 reg1 = im->vals[3] - BA_IM_RAX;
						u8 byte2 = (offset != 0 || (reg1 & 7) == 5) * 0x40 + 
							(offset >= 0x80) * 0x40;
						
						byte0 |= ((reg0 >= 8) << 2) | (reg1 >= 8);
						byte2 |= ((reg0 & 7) << 3) | (reg1 & 7);

						bool isReg1Mod4 = (reg1 & 7) == 4; // RSP or R12
						u64 ofstSz = (offset != 0 || (reg1 & 7) == 5) + 
							(offset >= 0x80) * 3;
						
						code->cnt += 3 + ofstSz + isReg1Mod4;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						code->arr[code->cnt-ofstSz-3-isReg1Mod4] = byte0;
						code->arr[code->cnt-ofstSz-2-isReg1Mod4] = 0x8b;
						code->arr[code->cnt-ofstSz-1-isReg1Mod4] = byte2;
						isReg1Mod4 && (code->arr[code->cnt-ofstSz-1] = 0x24);
						
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

					// GPR, ADRADDREGMUL GPR (+) IMM (*) GPR
					else if (im->vals[2] == BA_IM_ADRADDREGMUL) {
						if (im->count < 6) {
							return ba_ErrorIMArgCount(6, im);
						}

						u64 fact = im->vals[4];

						if (!(BA_IM_RAX <= im->vals[3]) || 
							!(BA_IM_R15 >= im->vals[3]) || 
							(im->vals[5] == BA_IM_RSP) ||
							(fact != 1 && fact != 2 && fact != 4 && fact != 8))
						{
							return ba_ErrorIMArgInvalid(im);
						}

						// fact = log2(fact)
						fact = (fact >= 2) + (fact >= 4) + (fact == 8);
						
						u64 reg1 = im->vals[3] - BA_IM_RAX;
						byte0 |= ((reg0 >= 8) << 2) | (reg1 >= 8);

						u64 reg2 = im->vals[5] - BA_IM_RAX;
						
						bool hasExtraByte = (reg1 & 7) == 5;
						code->cnt += 4 + hasExtraByte;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);
						
						u64 byte2 = 0x04 | (hasExtraByte << 6) | 
							((reg0 & 7) << 3);
						u64 byte3 = (fact << 6) | ((reg2 & 7) << 3) |
							(reg1 & 7);

						code->arr[code->cnt-4-hasExtraByte] = byte0;
						code->arr[code->cnt-3-hasExtraByte] = 0x8b;
						code->arr[code->cnt-2-hasExtraByte] = byte2;
						code->arr[code->cnt-1-hasExtraByte] = byte3;
						hasExtraByte && (code->arr[code->cnt-1] = 0);
					}

					// GPR, IMM/STATIC
					else if (im->vals[2] == BA_IM_IMM || 
						im->vals[2] == BA_IM_STATIC) 
					{
						if (im->count < 4) {
							return ba_ErrorIMArgCount(4, im);
						}

						if (im->vals[2] == BA_IM_STATIC) {
							struct ba_StaticAddr* staticAddr = (void*)im->vals[3];
							u64 addr = staticAddr->statObj->offset + staticAddr->index;

							/* Add a bit so that the address is interpreted as 
							 * 64-bit. This caps static segment addresses to 63 
							 * bits, which shouldn't be an issue */
							im->vals[3] = addr | (1llu<<63);
							// Push to a stack used to make the address absolute
							ba_StkPush(movStaticStk, (void*)code->cnt);
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
					u8 reg0 = im->vals[1] - BA_IM_AL;
					u8 byte0 = 0x40;

					// GPRb, ADR GPR
					if (im->vals[2] == BA_IM_ADR) {
						if (im->count < 4) {
							return ba_ErrorIMArgCount(4, im);
						}

						if (!(BA_IM_RAX <= im->vals[3]) || 
							!(BA_IM_R15 >= im->vals[3])) 
						{
							return ba_ErrorIMArgInvalid(im);
						}
					
						u8 reg1 = im->vals[3] - BA_IM_RAX;
						u8 byte2 = ((reg1 & 7) == 5) * 0x40;
						
						byte0 |= ((reg0 >= 8) << 2) | (reg1 >= 8);
						byte2 |= ((reg0 & 7) << 3) | (reg1 & 7);

						bool hasByte0 = (reg0 >= 4) | (reg1 >= 8);
						bool isReg1Mod4 = (reg1 & 7) == 4; // RSP or R12
						bool isReg1Mod5 = (reg1 & 7) == 5; // RBP or R13
						
						code->cnt += 2 + hasByte0 + isReg1Mod4 + isReg1Mod5;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						hasByte0 && (code->arr
							[code->cnt-3-isReg1Mod4-isReg1Mod5] = byte0);
						code->arr[code->cnt-2-isReg1Mod4-isReg1Mod5] = 0x8a;
						code->arr[code->cnt-1-isReg1Mod4-isReg1Mod5] = byte2;
						(isReg1Mod4 && (code->arr[code->cnt-1] = 0x24)) || 
						(isReg1Mod5 && (code->arr[code->cnt-1] = 0));
					}

					// GPRb, ADRADD/ADRSUB GPR
					else if (im->vals[2] == BA_IM_ADRADD || 
						im->vals[2] == BA_IM_ADRSUB) 
					{
						if (im->count < 5) {
							return ba_ErrorIMArgCount(5, im);
						}

						bool sub = im->vals[2] == BA_IM_ADRSUB;
						u64 offset = im->vals[4];

						u8 reg1 = im->vals[3] - BA_IM_RAX;
						u8 byte2 = (offset != 0 || (reg1 & 7) == 5) * 0x40 + 
							(offset >= 0x80) * 0x40;
						
						byte0 |= ((reg0 >= 8) << 2) | (reg1 >= 8);
						byte2 |= ((reg0 & 7) << 3) | (reg1 & 7);

						bool hasByte0 = (reg0 >= 4) | (reg1 >= 4);
						bool isReg1Mod4 = (reg1 & 7) == 4; // RSP or R12
						u64 ofstSz = (offset != 0 || (reg1 & 7) == 5) + 
							(offset >= 0x80) * 3;
						
						code->cnt += 2 + hasByte0 + isReg1Mod4 + ofstSz;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						hasByte0 &&
							(code->arr[code->cnt-ofstSz-3-isReg1Mod4] = byte0);
						code->arr[code->cnt-ofstSz-2-isReg1Mod4] = 0x8a;
						code->arr[code->cnt-ofstSz-1-isReg1Mod4] = byte2;
						isReg1Mod4 && (code->arr[code->cnt-ofstSz-1] = 0x24);
						
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
					// GPRb, IMM
					else if (im->vals[2] == BA_IM_IMM) {
						if (im->count < 4) {
							return ba_ErrorIMArgCount(4, im);
						}
						byte0 |= (reg0 >= 8);
						u8 imm = im->vals[3];
						u8 byte1 = 0xb0 | (reg0 & 7);

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
						return ba_ErrorIMArgCount(4, im);
					}

					if (!(BA_IM_RAX <= im->vals[2]) || 
						!(BA_IM_R15 >= im->vals[2])) 
					{
						return ba_ErrorIMArgInvalid(im);
					}
					
					u8 reg0 = im->vals[2] - BA_IM_RAX;
					
					// From GPR
					if ((BA_IM_RAX <= im->vals[3]) && 
						(BA_IM_R15 >= im->vals[3])) 
					{
						u8 reg1 = im->vals[3] - BA_IM_RAX;
						
						u8 byte0 = 0x48 | (reg0 >= 8) | ((reg1 >= 8) << 2);
						u8 byte2 = (reg0 & 7) | ((reg1 & 7) << 3);

						u8 instrSz = 3 + ((reg0 & 7) == 4) + ((reg0 & 7) == 5);

						code->cnt += instrSz;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						(code->arr[code->cnt-instrSz] = byte0);
						code->arr[code->cnt-instrSz+1] = 0x89;
						code->arr[code->cnt-instrSz+2] = 
							byte2 + ((reg0 & 7) == 5) * 0x40;
						(((reg0 & 7) == 4) && (code->arr[code->cnt-1] = 0x24)) || 
						(((reg0 & 7) == 5) && (code->arr[code->cnt-1] = 0));
					}
					// From GPRb
					else if ((BA_IM_AL <= im->vals[3]) && 
						(BA_IM_R15B >= im->vals[3])) 
					{
						u8 reg1 = im->vals[3] - BA_IM_AL;
						
						u8 byte0 = 0x40 | (reg0 >= 8) | ((reg1 >= 8) << 2);
						u8 byte2 = (reg0 & 7) | ((reg1 & 7) << 3);

						bool hasByte0 = (reg1 >= BA_IM_SPL - BA_IM_AL) || 
							(reg0 >= 8);
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
						return ba_ErrorIMArgCount(5, im);
					}

					if (!(BA_IM_RAX <= im->vals[2]) || !(BA_IM_R15 >= im->vals[2])) {
						return ba_ErrorIMArgInvalid(im);
					}

					u8 reg0 = im->vals[2] - BA_IM_RAX;
					u64 offset = im->vals[3];
					bool sub = im->vals[1] == BA_IM_ADRSUB ||
						im->vals[1] == BA_IM_64ADRSUB;

					
					// From GPR
					if ((BA_IM_RAX <= im->vals[4]) && (BA_IM_R15 >= im->vals[4])) {
						u8 reg1 = im->vals[4] - BA_IM_RAX;
						u8 byte0 = 0x48 | (reg0 >= 8) | ((reg1 >= 8) << 2);

						bool isOffsetOneByte = offset < 0x80;
						if (offset >= (1llu << 31)) {
							fprintf(stderr, "Error: Effective address cannot "
								"have a more than 32 bit offset in "
								"instruction %s\n", ba_IMToStr(im));
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

					// From GPRb
					else if ((BA_IM_AL <= im->vals[4]) && (BA_IM_R15B >= im->vals[4])) {
						u8 reg1 = im->vals[4] - BA_IM_AL;
						u8 byte0 = 0x40 | (reg0 >= 8) | ((reg1 >= 8) << 2);
						bool hasByte0 = (reg0 >= 4) | (reg1 >= 4);

						bool isOffsetOneByte = offset < 0x80;
						if (offset >= (1llu << 31)) {
							fprintf(stderr, "Error: Effective address cannot "
								"have a more than 32 bit offset in "
								"instruction %s\n", ba_IMToStr(im));
							exit(-1);
						}

						sub && (offset = -offset);
						u8 byte2 = (0x40 << (!isOffsetOneByte)) | (reg0 & 7) |
							((reg1 & 7) << 3);

						u8 instrSz = 3 + hasByte0 + 3 * (!isOffsetOneByte) + 
							((reg0 & 7) == 4);
						code->cnt += instrSz;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						hasByte0 && (code->arr[code->cnt-instrSz] = byte0);
						code->arr[code->cnt-instrSz+hasByte0] = 0x88;
						code->arr[code->cnt-instrSz+hasByte0+1] = byte2;
						(reg0 & 7) == 4 && 
							(code->arr[code->cnt-instrSz+hasByte0+2] = 0x24);

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
							fprintf(stderr, "Error: no size specified for MOV "
								"in instruction %s\n", ba_IMToStr(im));
							exit(-1);
						}

						if (im->count < 6) {
							return ba_ErrorIMArgCount(6, im);
						}

						u64 imm = im->vals[5];
						u8 byte0 = 0x48 | (reg0 >= 8);

						bool isOffsetOneByte = offset < 0x80;
						if (offset >= (1llu << 31)) {
							fprintf(stderr, "Error: Effective address cannot "
								"have a more than 32 bit offset " 
								"in instruction %s\n", ba_IMToStr(im));
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

				else {
					return ba_ErrorIMArgInvalid(im);
				}

				break;
			}

			case BA_IM_LEA:
			{
				if (im->count < 3) {
					return ba_ErrorIMArgCount(3, im);
				}

				// Into GPR
				if ((BA_IM_RAX <= im->vals[1]) && (BA_IM_R15 >= im->vals[1])) {
					u8 reg0 = im->vals[1] - BA_IM_RAX;

					// GPR, ADRADD/ADRSUB GPR
					if (im->vals[2] == BA_IM_ADRADD || 
						im->vals[2] == BA_IM_ADRSUB) 
					{
						if (im->count < 5) {
							return ba_ErrorIMArgCount(5, im);
						}

						if (!(BA_IM_RAX <= im->vals[3]) || 
							!(BA_IM_R15 >= im->vals[2])) 
						{
							return ba_ErrorIMArgInvalid(im);
						}

						bool sub = im->vals[2] == BA_IM_ADRSUB;
						u64 offset = im->vals[4];

						u8 reg1 = im->vals[3] - BA_IM_RAX;
						u64 byte0 = 0x48 | ((reg0 >= 8) << 2) | (reg1 >= 8);
						u8 byte2 = ((offset != 0 || (reg1 & 7) == 5) * 0x40 + 
							(offset >= 0x80) * 0x40) | ((reg0 & 7) << 3) | 
							(reg1 & 7);

						bool isReg1Mod4 = (reg1 & 7) == 4; // RSP or R12
						u64 ofstSz = (offset != 0 || (reg1 & 7) == 5) + 
							(offset >= 0x80) * 3;
						
						code->cnt += 3 + ofstSz + isReg1Mod4;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);

						code->arr[code->cnt-ofstSz-3-isReg1Mod4] = byte0;
						code->arr[code->cnt-ofstSz-2-isReg1Mod4] = 0x8d;
						code->arr[code->cnt-ofstSz-1-isReg1Mod4] = byte2;
						isReg1Mod4 && (code->arr[code->cnt-ofstSz-1] = 0x24);
						
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

					// GPR, ADRADDREGMUL GPR (+) IMM (*) GPR
					else if (im->vals[2] == BA_IM_ADRADDREGMUL) {
						if (im->count < 6) {
							return ba_ErrorIMArgCount(6, im);
						}

						u64 fact = im->vals[4];

						if (!(BA_IM_RAX <= im->vals[3]) || 
							!(BA_IM_R15 >= im->vals[3]) || 
							(im->vals[5] == BA_IM_RSP) ||
							(fact != 1 && fact != 2 && fact != 4 && fact != 8))
						{
							return ba_ErrorIMArgInvalid(im);
						}

						// fact = log2(fact)
						fact = (fact >= 2) + (fact >= 4) + (fact == 8);
						
						u64 reg1 = im->vals[3] - BA_IM_RAX;
						u64 byte0 = 0x48 | ((reg0 >= 8) << 2) | (reg1 >= 8);

						u64 reg2 = im->vals[5] - BA_IM_RAX;
						
						bool hasExtraByte = (reg1 & 7) == 5;
						code->cnt += 4 + hasExtraByte;
						(code->cnt > code->cap) && ba_ResizeDynArr8(code);
						
						u64 byte2 = 0x04 | (hasExtraByte << 6) | 
							((reg0 & 7) << 3);
						u64 byte3 = (fact << 6) | ((reg2 & 7) << 3) |
							(reg1 & 7);

						code->arr[code->cnt-4-hasExtraByte] = byte0;
						code->arr[code->cnt-3-hasExtraByte] = 0x8d;
						code->arr[code->cnt-2-hasExtraByte] = byte2;
						code->arr[code->cnt-1-hasExtraByte] = byte3;
						hasExtraByte && (code->arr[code->cnt-1] = 0);
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
					return ba_ErrorIMArgCount(3, im);
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
							return ba_ErrorIMArgCount(4, im);
						}

						u64 imm = im->vals[3];
						u8 byte0 = 0x48 | (reg0 >= 8);
						u8 byte2 = (0xc0 + byte1Offset) | (reg0 & 7);

						// >31 bits
						if (imm >= (1llu << 31)) {
							fprintf(stderr, "Error: Cannot %s more than 31 bits "
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

						bool hasByte0 = ((BA_IM_SPL <= im->vals[1]) | 
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
						return ba_ErrorIMArgCount(5, im);
					}
					
					if (!(BA_IM_RAX <= im->vals[2]) || !(BA_IM_R15 >= im->vals[2])) {
						fprintf(stderr, "Error: first argument for "
							"ADRADD/ADRSUB must be a general purpose register "
							"in instruction %s\n", ba_IMToStr(im));
						exit(-1);
					}
					
					// From GPR
					if ((BA_IM_RAX <= im->vals[4]) && (BA_IM_R15 >= im->vals[4])) {
						u8 sub = im->vals[1] == BA_IM_ADRSUB;

						u8 reg0 = im->vals[2] - BA_IM_RAX;
						u8 reg1 = im->vals[4] - BA_IM_RAX;
						u8 byte0 = 0x48 | (reg0 >= 8) | ((reg1 >= 8) << 3);
						u64 offset = im->vals[3];

						bool isOffsetOneByte = offset < 0x80;
						if (offset >= (1llu << 31)) {
							fprintf(stderr, "Error: Effective address cannot "
								"have a more than 32 bit offset in "
								"instruction %s\n", ba_IMToStr(im));
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
					return ba_ErrorIMArgCount(2, im);
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
					return ba_ErrorIMArgCount(3, im);
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

						bool hasByte0 = ((BA_IM_SPL <= im->vals[1]) | 
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
					return ba_ErrorIMArgCount(3, im);
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
							return ba_ErrorIMArgCount(4, im);
						}

						u64 imm = im->vals[3];
						if (imm >= 0x40) {
							fprintf(stderr, "Error: Cannot SHL a register by "
								"more than 6 bits in instruction %s\n", 
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
					return ba_ErrorIMArgCount(2, im);
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
					return ba_ErrorIMArgCount(2, im);
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

			case BA_IM_GOTO:
			{
				if (im->count < 6) {
					return ba_ErrorIMArgCount(6, im);
				}

				char* lblName = (char*)im->vals[1];
				u64 line = im->vals[3];
				u64 col = im->vals[4];
				char* path = (char*)im->vals[5];
				struct ba_PLabel* label = ba_HTGet(ctr->labelTable, lblName);

				if (label) {
					struct ba_IM* imNext = im->next;
					struct ba_IM* imStart = im;
					ctr->im = imStart;
					free(imStart->vals);

					struct ba_SymTable* scope = (void*)im->vals[2];
					while (scope && scope != label->scope) {
						if (scope->hasFramePtrLink) {
							ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP);
							ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP);
						}
						scope = scope->parent;
					}
					(!scope) && ba_ErrorGoto(line, col, path);

					ctr->im->next = imNext;
					ctr->im->count = 2;
					ctr->im->vals = ba_MAlloc(2 * sizeof(u64));
					ctr->im->vals[0] = BA_IM_LABELJMP;
					ctr->im->vals[1] = label->id;
					
					im = imStart;
					goto BA_LBL_WRITECODELOOPSTART;
				}
				else {
					return ba_ExitMsg(BA_EXIT_ERR, "goto label not found on",
						line, col, path);
				}
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
					return ba_ErrorIMArgCount(2, im);
				}

				u64 labelID = im->vals[1];
				struct ba_IMLabel* lbl = &labels[labelID];

				if (labelID >= ctr->labelCnt) {
					fprintf(stderr, "Error: cannot find intermediate label "
						"%lld in instruction %s\n", labelID, ba_IMToStr(im));
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
						(relAddr < 0ll && relAddr >= -0x80ll)))
					{
						relAddr -= 3 + (instrType == _INSTRTYPE_JCC);
						instrSize += 3 + (instrType == _INSTRTYPE_JCC);
					}

					code->cnt += instrSize;
					(code->cnt > code->cap) && ba_ResizeDynArr8(code);

					(instrType == _INSTRTYPE_JCC && instrSize == 6) && 
						(code->arr[code->cnt-6] = 0x0f);
					code->arr[code->cnt-instrSize + 
						(instrType == _INSTRTYPE_JCC && instrSize == 6)] = 
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
							if (labelDistance >= 0x80 /* near jmp */ || 
								(tmpIM->vals[0] == BA_IM_LABEL &&
								tmpIM->vals[1] == labelID) /* short jmp*/)
							{
								break;
							}
							tmpIM = tmpIM->next;
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
					return ba_ErrorIMArgCount(2, im);
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
				// IMM
				else if ((!isInstrPop) && (im->vals[1] == BA_IM_IMM)) {
					if (im->count < 3) {
						return ba_ErrorIMArgCount(3, im);
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
					return ba_ErrorIMArgCount(3, im);
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
					return ba_ErrorIMArgCount(2, im);
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
				code->cnt += 2;
				(code->cnt > code->cap) && ba_ResizeDynArr8(code);
				code->arr[code->cnt-2] = 0x48;
				code->arr[code->cnt-1] = 0x99;
				break;
			}
			default:
				fprintf(stderr, "Error: unrecognized intermediate "
					"instruction: %#llx\n", im->vals[0]);
				exit(-1);
		}

		im = ba_DelIM(im);
	}

	if (labels->jmpOfsts) {
		ba_DelDynArr64(labels->jmpOfsts);
		ba_DelDynArr8(labels->jmpOfstSizes);
	}
	free(labels);

	// Generate file header
	u8 fileHeader[64] = {
		0x7f, 'E',  'L',  'F',  2,    1,    1,    3,
		0,    0,    0,    0,    0,    0,    0,    0,
		2,    0,    62,   0,    1,    0,    0,    0,
		0,    0,    0,    0,    0,    0,    0,    0,  // {entryPoint}
		0x40, 0,    0,    0,    0,    0,    0,    0,
		0,    0,    0,    0,    0,    0,    0,    0,  // No section headers
		0,    0,    0,    0,    0x40, 0,    0x38, 0,
		phCnt,0,    0,    0,    0,    0,    0,    0,  // 2 or 3 p header entries
	};

	// Set entry point in header
	{
		u64 tmp = entryPoint;
		for (u64 i = 0; i < 8; i++) {
			fileHeader[0x18+i] = tmp & 0xff;
			tmp >>= 8;
		}
	}

	// Generate program headers
	u8 programHeader[] = {
		1,    0,    0,    0,    4,    0,    0,    0,  // LOAD, Read
		0,    0,    0,    0,    0,    0,    0,    0,  // 0x0000 in file
		0,    0,    0,    0,    0,    0,    0,    0,  // {memStart}
		0,    0,    0,    0,    0,    0,    0,    0,  // "
		fHeaderSz,0,0,    0,    0,    0,    0,    0,  // fHeaderSz
		fHeaderSz,0,0,    0,    0,    0,    0,    0,  // "
		0,    0,    0,    0,    0,    0,    0,    0,  // {pageSz}

		1,    0,    0,    0,    5,    0,    0,    0,  // LOAD, Read+Execute
		0,    0,    0,    0,    0,    0,    0,    0,  // {pageSz}
		0,    0,    0,    0,    0,    0,    0,    0,  // {memStart+pageSz}
		0,    0,    0,    0,    0,    0,    0,    0,  // "
		0,    0,    0,    0,    0,    0,    0,    0,  // {code->cnt}
		0,    0,    0,    0,    0,    0,    0,    0,  // "
		0,    0,    0,    0,    0,    0,    0,    0,  // {pageSz}

		1,    0,    0,    0,    6,    0,    0,    0,  // LOAD, Read+Write
		0,    0,    0,    0,    0,    0,    0,    0,  // {(static start)}
		0,    0,    0,    0,    0,    0,    0,    0,  // {(static start mem)}
		0,    0,    0,    0,    0,    0,    0,    0,  // "
		0,    0,    0,    0,    0,    0,    0,    0,  // {staticSize}
		0,    0,    0,    0,    0,    0,    0,    0,  // "
		0,    0,    0,    0,    0,    0,    0,    0,  // {pageSz}
	};

	u64 staticPadding;
	u64 staticStartM;
	{
		u64 tmpPageSz = pageSz;
		u64 tmpStartM = memStart;
		u64 tmpExe = memStart + pageSz;
		u64 tmpCodeSz = code->cnt;

		// Align static header properly
		u64 tmpStatic = pageSz + code->cnt;
		staticPadding = (bool)(tmpStatic & (pageSz-1)) * 
			(pageSz - (tmpStatic & (pageSz-1)));
		tmpStatic += staticPadding;

		u64 tmpStaticM = memStart + tmpStatic;
		staticStartM = tmpStaticM;
		u64 tmpStaticSz = staticSize;

		for (u64 i = 0; i < 8; i++) {
			programHeader[0x10+i] = tmpStartM & 0xff;
			programHeader[0x18+i] = tmpStartM & 0xff;
			programHeader[0x30+i] = tmpPageSz & 0xff;

			programHeader[0x40+i] = tmpPageSz & 0xff;
			programHeader[0x48+i] = tmpExe & 0xff;
			programHeader[0x50+i] = tmpExe & 0xff;
			programHeader[0x58+i] = tmpCodeSz & 0xff;
			programHeader[0x60+i] = tmpCodeSz & 0xff;
			programHeader[0x68+i] = tmpPageSz & 0xff;

			programHeader[0x78+i] = tmpStatic & 0xff;
			programHeader[0x80+i] = tmpStaticM & 0xff;
			programHeader[0x88+i] = tmpStaticM & 0xff;
			programHeader[0x90+i] = tmpStaticSz & 0xff;
			programHeader[0x98+i] = tmpStaticSz & 0xff;
			programHeader[0xa0+i] = tmpPageSz & 0xff;

			tmpStartM >>= 8;
			tmpPageSz >>= 8;
			tmpExe >>= 8;
			tmpCodeSz >>= 8;
			tmpStatic >>= 8;
			tmpStaticM >>= 8;
			tmpStaticSz >>= 8;
		}
	}

	// Correct addresses of MOV STATIC instructions
	while (movStaticStk->count) {
		u64 addr = (u64)ba_StkPop(movStaticStk);
		u64 addend = staticStartM;
		u64 sum = 0;
		bool carry = 0;
		for (u64 i = 2; i < 9; ++i) {
			sum = (u64)code->arr[addr+i] + (u64)(addend & 0xff) + (u64)carry;
			carry = sum & 0x100;
			code->arr[addr+i] = sum & 0xff;
			addend >>= 8;
		}
		code->arr[addr+9] &= ~(1<<7); // Remove technical bit added
	}
	ba_DelStk(movStaticStk);

	// ----------------------------------------
	// --- String this together into a file ---
	// ----------------------------------------
	
	// Create the initial file
	FILE* file = stdout;
	bool isFileStdout = !strcmp(fileName, "-");

	if (!isFileStdout) {
		file = fopen(fileName, "wb");
		if (!file) {
			fprintf(stderr, "Error: Could not open file '%s' for writing\n", 
				fileName);
			exit(-1);
		}
		fclose(file);

		// Create the file for appending
		file = fopen(fileName, "ab");
		if (!file) {
			fprintf(stderr, "Error: Could not open file '%s' for writing\n", 
				fileName);
			exit(-1);
		}
	}
	
	// Write headers and code
	u8 buf[BA_FILE_BUF_SIZE];

	// File header
	memcpy(buf, fileHeader, 0x40);
	memcpy(buf+0x40, programHeader, pHeaderSz);
	memset(buf+fHeaderSz, 0, pageSz-fHeaderSz);

	// Code
	u64 bufSize = (code->cnt+pageSz) > BA_FILE_BUF_SIZE 
		? BA_FILE_BUF_SIZE : code->cnt;
	memcpy(buf+pageSz, code->arr, bufSize);
	fwrite(buf, 1, pageSz + bufSize, file);

	// Write more code, if not everything can fit in the buffer
	u8* codePtr = code->arr + BA_FILE_BUF_SIZE - pageSz;
	while (codePtr - code->arr < code->cnt) {
		fwrite(codePtr, 1, code->cnt > BA_FILE_BUF_SIZE 
			? BA_FILE_BUF_SIZE : code->cnt, file);
		codePtr += BA_FILE_BUF_SIZE;
	}
	memset(buf, 0, staticPadding);
	fwrite(buf, 1, staticPadding, file);

	// Write static segment, if it exists
	if (staticSize) {
		for (u64 i = 0; i < ctr->statics->cnt; ++i) {
			struct ba_Static* statObj = (void*)ctr->statics->arr[i];
			if (statObj->isUsed) {
				u8* ptr = statObj->arr->arr;
				while (ptr - statObj->arr->arr < statObj->arr->cnt) {
					fwrite(ptr, 1, statObj->arr->cnt >= BA_FILE_BUF_SIZE
						? BA_FILE_BUF_SIZE : statObj->arr->cnt, file);
					ptr += BA_FILE_BUF_SIZE;
				}
				free(statObj);
			}
		}
	}

	if (!isFileStdout) {
		fclose(file);
		chmod(fileName, 0755);
	}

	ba_DelDynArr8(code);

	return 1;
}

/* Pessimistically estimates instruction size, for working out size of jmp
 * instructions. */
u8 ba_PessimalInstrSize(struct ba_IM* im) {
	switch (im->vals[0]) {
		case BA_IM_NOP:
			return 1;

		case BA_IM_LABEL:
			return 0;

		case BA_IM_MOV: case BA_IM_LEA:
		{
			u8 adrAddDestSize = 
				0xff * ((im->vals[1] == BA_IM_ADRADD) | 
					(im->vals[1] == BA_IM_ADRSUB)) +
				0x04 * ((im->vals[1] == BA_IM_64ADRADD) | 
					(im->vals[1] == BA_IM_64ADRSUB));

			// Into GPR
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
					u64 ofstSz = (offset != 0 || (reg1 & 7) == 5) + 
						(offset >= 0x80) * 3;
					return 3 + ((reg1 & 7) == 4 || (reg1 & 7) == 5) + ofstSz;
				}
				else if (im->vals[2] == BA_IM_ADRADDREGMUL) {
					u64 reg1 = im->vals[3] - BA_IM_RAX;
					return 4 + ((reg1 & 7) == 5);
				}
				else if (im->vals[2] == BA_IM_IMM) {
					u64 imm = im->vals[3];
					if (imm < (1llu << 32)) {
						u8 reg0 = im->vals[1] - BA_IM_RAX;
						return 5 + (reg0 >= 8);
					}
					return 10;
				}
				else if (im->vals[2] == BA_IM_STATIC) {
					return 10;
				}
			}
			// Into GPRb
			else if ((BA_IM_AL <= im->vals[1]) && (BA_IM_R15B >= im->vals[1])) {
				u8 reg0 = im->vals[1] - BA_IM_AL;
				// GPRb, ADRADD/ADRSUB GPR
				if (im->vals[2] == BA_IM_ADRADD || 
					im->vals[2] == BA_IM_ADRSUB) 
				{
					u64 offset = im->vals[4];
					u8 reg1 = im->vals[3] - BA_IM_RAX;
					bool hasByte0 = (reg0 >= 4) | (reg1 >= 4);
					bool isReg1Mod4 = (reg1 & 7) == 4; // RSP or R12
					u64 ofstSz = (offset != 0 || (reg1 & 7) == 5) + 
						(offset >= 0x80) * 3;
					return 2 + hasByte0 + isReg1Mod4 + ofstSz;
				}
				else if (im->vals[2] == BA_IM_ADR) {
					u8 reg1 = im->vals[3] - BA_IM_RAX;
					bool hasByte0 = (reg0 >= 4) | (reg1 >= 8);
					bool isReg1Mod4 = (reg1 & 7) == 4; // RSP or R12
					bool isReg1Mod5 = (reg1 & 7) == 5; // RBP or R13
					return 2 + hasByte0 + isReg1Mod4 + isReg1Mod5;
				}
				// GPRb, IMM
				else if (im->vals[2] == BA_IM_IMM) {
					return 2 + (reg0 >= 4);
				}

				else {
					return ba_ErrorIMArgInvalid(im);
				}
			}
			// Into ADR GPR effective address
			else if (im->vals[1] == BA_IM_ADR) {
				u8 reg0 = im->vals[2] - BA_IM_RAX;
				if ((BA_IM_RAX <= im->vals[3]) && (BA_IM_R15 >= im->vals[3])) {
					return 3 + ((reg0 & 7) == 4) + ((reg0 & 7) == 5);
				}
				else if ((BA_IM_AL <= im->vals[3]) && (BA_IM_R15B >= im->vals[3])) {
					u8 reg1 = im->vals[3] - BA_IM_AL;
					bool hasByte0 = (reg1 >= BA_IM_SPL - BA_IM_AL) || (reg0 >= 8);
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
				if ((BA_IM_AL <= im->vals[4]) && (BA_IM_R15B >= im->vals[4])) {
					u8 reg1 = im->vals[4] - BA_IM_AL;
					return 3 + ((reg0 >= 4) | (reg1 >= 4)) + 
						3 * (offset >= 0x80) + ((reg0 & 7) == 4);
				}
				else if (im->vals[4] == BA_IM_IMM) {
					return adrAddDestSize + 4 + 3 * (offset >= 0x80) + 
						((reg0 & 7) == 4);
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
		case BA_IM_GOTO:
			return 0xff;
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
			fprintf(stderr, "Error: unrecognized intermediate "
				"instruction: %#llx\n", im->vals[0]);
			exit(-1);
	}

	// Maximum x86_64 instruction size
	return 15;
}

