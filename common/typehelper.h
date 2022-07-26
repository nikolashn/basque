// See LICENSE for copyright/license information

#ifndef BA__COMMON_TYPEHELPER_H
#define BA__COMMON_TYPEHELPER_H

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
			return ba_GetSizeOfType(*(struct ba_Type*)type.extraInfo);
		case BA_TYPE_ARR: {
			struct ba_ArrExtraInfo info = 
				*(struct ba_ArrExtraInfo*)type.extraInfo;
			return ba_GetSizeOfType(info.type) * info.cnt;
		}
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
	return a.type == b.type;
}

char* ba_GetTypeStr(struct ba_Type type) {
	switch (type.type) {
		case BA_TYPE_VOID:
			return "void";
		case BA_TYPE_U64:
			return "u64";
		case BA_TYPE_I64:
			return "i64";
		case BA_TYPE_U8:
			return "u8";
		case BA_TYPE_I8:
			return "i8";
		case BA_TYPE_BOOL:
			return "bool";
		case BA_TYPE_F64:
			return "f64";
		case BA_TYPE_F32:
			return "f32";
		case BA_TYPE_TYPE:
			return "type";
		case BA_TYPE_PTR: {
			char* pntdStr = ba_GetTypeStr(*(struct ba_Type*)type.extraInfo);
			u64 pntdStrLen = strlen(pntdStr);
			char* typeStr = malloc(pntdStrLen+2);
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
			sprintf(sizeStr, "%lld", info->cnt);
			u64 pntdTypeStrLen = strlen(pntdTypeStr);
			u64 sizeStrLen = strlen(sizeStr);
			u64 typeStrSize = pntdTypeStrLen+sizeStrLen+3;
			char* typeStr = malloc(typeStrSize);
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
			char* paramsStr = malloc(paramsStrCap);
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
					paramsStr = realloc(paramsStr, 2 * paramsStrCap);
				}
				strcpy(paramsStr+paramsStrLen-pStrLen, pStr);
				param = param->next;
			}
			
			char* typeStr = malloc(strlen(retTypeStr)+paramsStrLen+8);
			sprintf(typeStr, "func %s(%s)", retTypeStr, paramsStr);
			return typeStr;
		}
	}
	return 0;
}

#endif
