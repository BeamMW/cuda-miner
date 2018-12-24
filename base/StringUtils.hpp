#pragma	once

inline bool isWhitespace(char aChar)
{
	return ('\n' == aChar) || (' ' == aChar) || ('\r' == aChar) || ('\t' == aChar);
}

inline bool isDigits(char aChar)
{
	return (aChar >= '0' && aChar <= '9');
}

inline bool isNumber(char aChar)
{
	return (aChar == '.') || (aChar >= '0' && aChar <= '9');
}

inline bool isHexDigits(char aChar)
{
	return (aChar >= '0' && aChar <= '9') || (aChar >= 'a' && aChar <= 'f') || (aChar >= 'A' && aChar <= 'F');
}

inline char hexToInt(char aChar)
{
	if (aChar >= '0' && aChar <= '9') {
		return aChar - '0';
	}
	else if (aChar >= 'a' && aChar <= 'f') {
		return 10 + (aChar - 'a');
	}
	else if (aChar >= 'A' && aChar <= 'F') {
		return 10 + (aChar - 'A');
	}
	return 0;
}

void binToString(std::string &aDst, const unsigned char *aSrc, size_t aLength, bool aDstBigEndian, bool aSrcBigEndian);
