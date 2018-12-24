#include "pch.hpp"
#include "Convertors.hpp"
#include "StringUtils.hpp"

int HexToBin(const std::string &aSrc, unsigned char *aDst, size_t aDstLen)
{
	if (aDstLen >= aSrc.length() / 2) {
		if (aSrc.empty()) {
			return 0;
		}
		aDstLen = 0;
		const char *cp = aSrc.c_str();
		while (*cp) {
			if (!isHexDigits(cp[0]) || !isHexDigits(cp[1])) {
				return -2;
			}
			*aDst = (hexToInt(cp[0]) << 4) | hexToInt(cp[1]);
			aDst++;
			cp += 2;
			aDstLen++;
		}
		return (int)aDstLen;
	}
	return -1;
}