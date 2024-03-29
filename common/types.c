// See LICENSE for copyright/license information

#include "types.h"
#include "func.h"

bool ba_IsTypeUnsigned(struct ba_Type type) {
	return (type.type == BA_TYPE_U64) || (type.type == BA_TYPE_U8) || 
		(type.type == BA_TYPE_BOOL) || (type.type == BA_TYPE_PTR);
}

bool ba_IsTypeSigned(struct ba_Type type) {
	return (type.type == BA_TYPE_I64) || (type.type == BA_TYPE_I8);
}

bool ba_IsTypeInt(struct ba_Type type) {
	return ba_IsTypeUnsigned(type) || ba_IsTypeSigned(type);
}

bool ba_IsTypeNum(struct ba_Type type) {
	return ba_IsTypeUnsigned(type) || ba_IsTypeSigned(type);
}

u64 ba_GetSizeOfType(struct ba_Type type) {
	switch (type.type) {
		case BA_TYPE_U64:
		case BA_TYPE_I64:
		case BA_TYPE_F64:
		case BA_TYPE_PTR:
			return 8;
		case BA_TYPE_F32:
			return 4;
		case BA_TYPE_U8:
		case BA_TYPE_I8:
		case BA_TYPE_BOOL:
			return 1;
		case BA_TYPE_DPTR:
		case BA_TYPE_TYPE:
			return ba_GetSizeOfType(*(struct ba_Type*)type.extraInfo);
		case BA_TYPE_ARR: {
			struct ba_ArrExtraInfo info = 
				*(struct ba_ArrExtraInfo*)type.extraInfo;
			return ba_GetSizeOfType(info.type) * info.cnt;
		}
		case BA_TYPE_STRUCT:
			return ((struct ba_StructExtraInfo*)type.extraInfo)->size;
	}
	return 0;
}

bool ba_AreTypesEqual(struct ba_Type a, struct ba_Type b) {
	if ((a.type == BA_TYPE_PTR && b.type == BA_TYPE_PTR) || 
		(a.type == BA_TYPE_DPTR && b.type == BA_TYPE_DPTR))
	{
		return ba_AreTypesEqual(*(struct ba_Type*)a.extraInfo, 
			*(struct ba_Type*)b.extraInfo);
	}
	if (a.type == BA_TYPE_ARR && b.type == BA_TYPE_ARR) {
		struct ba_ArrExtraInfo* aInfo = a.extraInfo;
		struct ba_ArrExtraInfo* bInfo = b.extraInfo;
		return aInfo->cnt == bInfo->cnt && 
			ba_AreTypesEqual(aInfo->type, bInfo->type);
	}
	if (a.type == BA_TYPE_FUNC && b.type == BA_TYPE_FUNC) {
		struct ba_Func* aFunc = a.extraInfo;
		struct ba_Func* bFunc = b.extraInfo;
		if (!(aFunc->paramCnt == bFunc->paramCnt) ||
			!ba_AreTypesEqual(aFunc->retType, bFunc->retType))
		{
			return 0;
		}
		struct ba_FuncParam* aParam = aFunc->firstParam;
		struct ba_FuncParam* bParam = bFunc->firstParam;
		while (aParam) {
			if (!ba_AreTypesEqual(aParam->type, bParam->type)) {
				return 0;
			}
			aParam = aParam->next;
			bParam = bParam->next;
		}
		return 1;
	}
	if (a.type == BA_TYPE_STRUCT && b.type == BA_TYPE_STRUCT) {
		// TODO
		return 0;
	}
	return a.type == b.type;
}

/* void* can be converted to any type and vice versa
 * array pointer can be converted to pointer of the array's fundamental type
 * and vice versa */
bool ba_IsPermitConvertPtr(struct ba_Type a, struct ba_Type b) {
	struct ba_Type aExtra = *(struct ba_Type*)a.extraInfo;
	struct ba_Type bExtra = *(struct ba_Type*)b.extraInfo;

	return BA_TYPE_VOID == aExtra.type || BA_TYPE_VOID == bExtra.type || 
		ba_AreTypesEqual(aExtra, bExtra) ||
		(aExtra.type == BA_TYPE_ARR && 
		 	ba_AreTypesEqual(bExtra,
				((struct ba_ArrExtraInfo*)(aExtra.extraInfo))->type)) ||
		(bExtra.type == BA_TYPE_ARR && 
		 	ba_AreTypesEqual(aExtra,
				((struct ba_ArrExtraInfo*)(bExtra.extraInfo))->type));
}

char* ba_GetTypeStr(struct ba_Type type) {
	switch (type.type) {
		case BA_TYPE_VOID: return "void";
		case BA_TYPE_U64:  return "u64";
		case BA_TYPE_I64:  return "i64";
		case BA_TYPE_U8:   return "u8";
		case BA_TYPE_I8:   return "i8";
		case BA_TYPE_BOOL: return "bool";
		case BA_TYPE_F64:  return "f64";
		case BA_TYPE_F32:  return "f32";
		case BA_TYPE_TYPE: return "type";
		case BA_TYPE_PTR: {
			char* pntdStr = ba_GetTypeStr(*(struct ba_Type*)type.extraInfo);
			u64 pntdStrLen = strlen(pntdStr);
			char* typeStr = ba_MAlloc(pntdStrLen+2);
			strcpy(typeStr, pntdStr);
			typeStr[pntdStrLen] = '*';
			typeStr[pntdStrLen+1] = 0;
			return typeStr;
		}
		case BA_TYPE_DPTR:
			return ba_GetTypeStr(*(struct ba_Type*)type.extraInfo);
		case BA_TYPE_ARR: {
			struct ba_ArrExtraInfo* info = type.extraInfo;
			char* pntdTypeStr = ba_GetTypeStr(info->type);
			char sizeStr[20];
			snprintf(sizeStr, 20, "%lld", info->cnt);
			u64 pntdTypeStrLen = strlen(pntdTypeStr);
			u64 sizeStrLen = strlen(sizeStr);
			u64 typeStrSize = pntdTypeStrLen+sizeStrLen+3;
			char* typeStr = ba_MAlloc(typeStrSize);
			strcpy(typeStr, pntdTypeStr);
			typeStr[pntdTypeStrLen] = '[';
			strcpy(typeStr+pntdTypeStrLen+1, sizeStr);
			typeStr[typeStrSize-2] = ']';
			typeStr[typeStrSize-1] = 0;
			return typeStr;
		}
		case BA_TYPE_FUNC: {
			struct ba_Func* func = type.extraInfo;
			char* retTypeStr = ba_GetTypeStr(func->retType);
			
			u64 paramsStrLen = 0;
			u64 paramsStrCap = 20;
			char* paramsStr = ba_MAlloc(paramsStrCap);
			paramsStr[0] = 0;

			struct ba_FuncParam* param = func->firstParam;
			while (func->paramCnt && param) {
				if (param != func->firstParam) {
					paramsStr[paramsStrLen++] = ',';
					paramsStr[paramsStrLen++] = ' ';
				}
				char* pStr = ba_GetTypeStr(param->type);
				u64 pStrLen = strlen(pStr);
				paramsStrLen += pStrLen;
				// The +2 ensures that ", " can be added without overflow
				if (paramsStrCap <= paramsStrLen + 2) {
					paramsStr = ba_Realloc(paramsStr, 2 * paramsStrCap);
				}
				strcpy(paramsStr+paramsStrLen-pStrLen, pStr);
				param = param->next;
			}
			
			u64 bufSize = strlen(retTypeStr)+paramsStrLen+8;
			char* typeStr = ba_MAlloc(bufSize);
			snprintf(typeStr, bufSize, "func %s(%s)", retTypeStr, paramsStr);
			return typeStr;
		}
		case BA_TYPE_STRUCT: {
			// TODO
			return "struct";
		}
	}
	return 0;
}

