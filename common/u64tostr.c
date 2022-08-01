// See LICENSE for copyright/license information

#include "u64tostr.h"

u64 ba_StrToU64(char* str, u64 line, u64 col, char* path) {
	u64 len = 0;
	u64 finalInt = 0;
	u64 base = 10;

	while (*str) {
		if (*str == '0') {
			goto BA_LBL_STRTOU64_LOOPEND;
		}

		if (base == 10) {
			if ((*str == 'x') || (*str == 'X')) {
				base = 16;
				goto BA_LBL_STRTOU64_LOOPEND;
			}
			else if ((*str == 'o') || (*str == 'O')) {
				base = 8;
				goto BA_LBL_STRTOU64_LOOPEND;
			}
			else if ((*str == 'b') || (*str == 'B')) {
				base = 2;
				goto BA_LBL_STRTOU64_LOOPEND;
			}
		}

		len = strlen(str);
		if (base == 10) {
			if (len > 20) {
				return ba_ErrorIntLitTooLong(line, col, path);
			}
			else if (len == 20) {
				// Maximum 64-bit unsigned integer
				char* mx = "18446744073709551615";
				for (u64 i = 0; i < 20; i++) {
					// Guaranteed greater than the max integer
					if (str[i] > *mx) {
						return ba_ErrorIntLitTooLong(line, col, path);
					}
					// Guaranteed smaller than the max integer
					else if (str[i] < *mx) {
						break;
					}
					++mx;
				}
			}
		}
		else if (base == 16) {
			if (len > 16) {
				return ba_ErrorIntLitTooLong(line, col, path);
			}
		}
		else if (base == 8) {
			if ((len > 21) && (*str >= '2')) {
				return ba_ErrorIntLitTooLong(line, col, path);
			}
		}
		else if (base == 2) {
			if (len > 64) {
				return ba_ErrorIntLitTooLong(line, col, path);
			}
		}

		break;
		
		BA_LBL_STRTOU64_LOOPEND:
		++str;
	}

	if (!len) {
		return finalInt;
	}
	
	if (base == 10) {
		u64 mul = 1;
		for (u64 i = len; i-- > 0; ) {
			if (!((str[i] >= '0') && (str[i] <= '9'))) {
				return ba_ExitMsg(0, "invalid character in integer literal on", 
					line, col, path);
			}
	
			finalInt += mul * (str[i] - '0');
			mul *= 10;
		}
	}
	else if (base == 16) {
		u64 shift = 0;
		for (u64 i = len; i-- > 0; ) {
			if ((str[i] >= '0') && (str[i] <= '9')) {
				finalInt += (u64)(str[i] - '0') << shift;
			}
			else if ((str[i] >= 'a') && (str[i] <= 'f')) {
				finalInt += (u64)(str[i] - 'a' + 10) << shift;
			}
			else if ((str[i] >= 'A') && (str[i] <= 'F')) {
				finalInt += (u64)(str[i] - 'A' + 10) << shift;
			}
			else {
				return ba_ErrorIntLitChar(line, col, path);
			}
			
			shift += 4;
		}
	}
	else if (base == 8) {
		u64 shift = 0;
		for (u64 i = len; i-- > 0; ) {
			if (!((str[i] >= '0') && (str[i] <= '7'))) {
				return ba_ErrorIntLitChar(line, col, path);
			}
			
			finalInt += (str[i] - '0') << shift;
			shift += 3;
		}
	}
	else if (base == 2) {
		u64 shift = 0;
		for (u64 i = len; i-- > 0; ) {
			if (!((str[i] == '0') || (str[i] == '1'))) {
				return ba_ErrorIntLitChar(line, col, path);
			}
			
			finalInt += (str[i] == '1') << shift;
			++shift;
		}
	}

	return finalInt;
}

char* ba_U64ToStr(u64 num) {
	u8 digits[20];
	char* str = ba_MAlloc(20);
	u64 len = 0;

	if (!num) {
		str[0] = '0';
		str[1] = 0;
		return str;
	}
	while (num) {
		digits[len++] = num % 10;
		num /= 10;
	}

	for (u64 i = 0; i < len; i++) {
		str[len-i-1] = '0' + digits[i];
	}
	str[len] = 0;
	
	return str;
}

