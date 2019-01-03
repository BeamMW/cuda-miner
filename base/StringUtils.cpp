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

void tokenizeString(const std::string &aStr, char aDelimiter, std::vector<std::string> &aTokens)
{
	// Skip delimiters at beginning.
	std::string::size_type lastPos = aStr.find_first_not_of(aDelimiter, 0);
	// Find first "non-delimiter".
	std::string::size_type pos = aStr.find_first_of(aDelimiter, lastPos);

	while (std::string::npos != pos || std::string::npos != lastPos)
	{  // Found a token, add it to the vector.
		aTokens.push_back(aStr.substr(lastPos, pos - lastPos));
		// Skip delimiters.  Note the "not_of"
		lastPos = aStr.find_first_not_of(aDelimiter, pos);
		// Find next "non-delimiter"
		pos = aStr.find_first_of(aDelimiter, lastPos);
	}
}
