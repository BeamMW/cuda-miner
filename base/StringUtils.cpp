#include "pch.hpp"
#include "StringUtils.hpp"

void binToString(std::string &aDst, const unsigned char *aSrc, size_t aLength, bool aDstBigEndian, bool aSrcBigEndian)
{
	static const char hex[16] = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };
	if (aLength) {
		int delta = 1;
		if (aDstBigEndian) {
			if (!aSrcBigEndian) {
				aSrc += aLength - 1;
				delta = -1;
			}
		}
		else {
			if (aSrcBigEndian) {
				aSrc += aLength - 1;
				delta = -1;
			}
		}
		size_t offset = aDst.size();
		aDst.resize(offset + 2 * aLength);
		char *dst = (char*)(aDst.data() + offset);
		while (aLength--) {
			unsigned char c = *aSrc;
			*dst++ = hex[c >> 4];
			*dst++ = hex[c & 0xf];
			aSrc += delta;
		}
		*dst = 0;
	}
}
