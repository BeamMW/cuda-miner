#include "pch.hpp"
#include "BigInteger.hpp"
#include "base/StringUtils.hpp"

BigInteger::BigInteger(const std::string &aHex, bool aBigEndian, int *aErrorAtPos)
{
	Import(aHex, aBigEndian, aErrorAtPos);
}

bool BigInteger::Import(const std::string &aHex, bool aBigEndian, int *aErrorAtPos)
{
	_isBigEndian = aBigEndian;
	if ((aHex.length() >= 2) && (0 == (aHex.length() & 1)))
	{
		const char *cp = aHex.c_str();
		unsigned length = int(aHex.length() / 2);
		if ('0' == cp[0] && 'x' == cp[1]) {
			_isBigEndian = true;
			cp += 2;
			length--;
		}
		if (length) {
			if (_value.empty()) {
				_value.resize(length);
			}
			else {
				if (length != _value.size()) {
					if (aErrorAtPos) {
						*aErrorAtPos = 0;
					}
					return false;
				}
			}
			for (unsigned char *dst = &_value.front(); length > 0; cp += 2, length--, dst++) {
				if (!isHexDigits(cp[0])) {
					if (aErrorAtPos) {
						*aErrorAtPos = int(cp - aHex.c_str());
					}
					_value.clear();
					return false;
				}
				if (!isHexDigits(cp[1])) {
					if (aErrorAtPos) {
						*aErrorAtPos = 1 + int(cp - aHex.c_str());
					}
					_value.clear();
					return false;
				}
				*dst = (hexToInt(cp[0]) << 4) | hexToInt(cp[1]);
			}
		}
	}
	return true;
}

void BigInteger::SetBigEndian(bool aBigEndian)
{
	if (aBigEndian != _isBigEndian) {
		if (_value.size() > 1) {
			unsigned char *sp = _value.data();
			unsigned char *ep = sp + _value.size() - 1;
			while (sp < ep) {
				unsigned char c = *sp;
				*sp++ = *ep;
				*ep-- = c;
			}
		}
		_isBigEndian = aBigEndian;
	}
}

void BigInteger::ToString(std::string &aDst, bool aBigEndian) const
{
	binToString(aDst, _value.data(), _value.size(), aBigEndian, _isBigEndian);
}

bool BigInteger::operator < (const BigInteger &aRight) const
{
	if (_value.empty()) {
		return !aRight._value.empty();
	}
	if (aRight._value.empty()) {
		return false;
	}
	const unsigned char *left = _value.data();
	int leftDelta = 1;
	if (!_isBigEndian) {
		left += _value.size() - 1;
		leftDelta = -1;
	}
	const unsigned char *right = aRight._value.data();
	int rightDelta = 1;
	if (!aRight._isBigEndian) {
		right += aRight._value.size() - 1;
		rightDelta = -1;
	}
	size_t leftLength = _value.size();
	size_t rightLength = aRight._value.size();
	if (leftLength > rightLength) {
		for (; leftLength > rightLength; left += leftDelta, leftLength--) {
			if (*left) {
				return false;
			}
		}
	} else if (leftLength < rightLength) {
		for (; leftLength < rightLength; right += rightDelta, rightLength--) {
			if (*right) {
				return true;
			}
		}
	}
	for (; leftLength; left += leftDelta, right += rightDelta, leftLength--) {
		if (*left > *right) {
			return false;
		}
		if (*left < *right) {
			return true;
		}
	}
	return false;
}

bool BigInteger::operator <= (const BigInteger &aRight) const
{
	if (_value.empty()) {
		return true;
	}
	if (aRight._value.empty()) {
		return _value.empty();
	}
	const unsigned char *left = _value.data();
	int leftDelta = 1;
	if (!_isBigEndian) {
		left += _value.size() - 1;
		leftDelta = -1;
	}
	const unsigned char *right = aRight._value.data();
	int rightDelta = 1;
	if (!aRight._isBigEndian) {
		right += aRight._value.size() - 1;
		rightDelta = -1;
	}
	size_t leftLength = _value.size();
	size_t rightLength = aRight._value.size();
	if (leftLength > rightLength) {
		for (; leftLength > rightLength; left += leftDelta, leftLength--) {
			if (*left) {
				return false;
			}
		}
	}
	else if (leftLength < rightLength) {
		for (; leftLength < rightLength; right += rightDelta, rightLength--) {
			if (*right) {
				return true;
			}
		}
	}
	for (; leftLength; left += leftDelta, right += rightDelta, leftLength--) {
		if (*left > *right) {
			return false;
		}
		if (*left < *right) {
			return true;
		}
	}
	return true;
}

bool BigInteger::Equals(const BigInteger &aRight) const
{
	if (_value.empty()) {
		return aRight._value.empty();
	}
	if (aRight._value.empty()) {
		return false;
	}
	const unsigned char *left = _value.data();
	int leftDelta = 1;
	if (!_isBigEndian) {
		left += _value.size() - 1;
		leftDelta = -1;
	}
	const unsigned char *right = aRight._value.data();
	int rightDelta = 1;
	if (!aRight._isBigEndian) {
		right += aRight._value.size() - 1;
		rightDelta = -1;
	}
	size_t leftLength = _value.size();
	size_t rightLength = aRight._value.size();
	if (leftLength > rightLength) {
		for (; leftLength > rightLength; left += leftDelta, leftLength--) {
			if (*left) {
				return false;
			}
		}
	}
	else if (leftLength < rightLength) {
		for (; leftLength < rightLength; right += rightDelta, rightLength--) {
			if (*right) {
				return false;
			}
		}
	}
	for (; leftLength; left += leftDelta, right += rightDelta, leftLength--) {
		if (*left != *right) {
			return false;
		}
	}
	return true;
}

bool BigInteger::operator == (const BigInteger &aRight) const
{
	return Equals(aRight);
}

bool BigInteger::operator != (const BigInteger &aRight) const
{
	return !Equals(aRight);
}

int BigInteger::GetLower(unsigned char *aDst, unsigned aLength, bool aDstIsBigEndian)
{
	if (_value.size()) {
		if (aLength > _value.size()) {
			aLength = (unsigned)_value.size();
		}
		if (_isBigEndian && aDstIsBigEndian) {
			unsigned char *src = _value.data() + _value.size() - aLength;
			memcpy(aDst, src, aLength);
		}
		else if (!_isBigEndian && !aDstIsBigEndian) {
			unsigned char *src = _value.data();
			memcpy(aDst, src, aLength);
		}
		else if (_isBigEndian && !aDstIsBigEndian) {
			unsigned char *src = _value.data() + _value.size() - 1;
			for (unsigned i = 0; i < aLength; i++) {
				*aDst++ = *src--;
			}
		}
		else {
			unsigned char *src = _value.data();
			aDst += aLength - 1;
			for (unsigned i = 0; i < aLength; i++) {
				*aDst-- = *src++;
			}
		}
		return aLength;
	}
	return 0;
}

int BigInteger::GetUpper(unsigned char *aDst, unsigned aLength, bool aDstIsBigEndian)
{
	if (_value.size()) {
		if (aLength > _value.size()) {
			aLength = (unsigned)_value.size();
		}
		if (_isBigEndian && aDstIsBigEndian) {
			unsigned char *src = _value.data();
			memcpy(aDst, src, aLength);
		}
		else if (!_isBigEndian && !aDstIsBigEndian) {
			unsigned char *src = _value.data() + _value.size() - aLength;
			memcpy(aDst, src, aLength);
		}
		else if (_isBigEndian && !aDstIsBigEndian) {
			unsigned char *src = _value.data();
			aDst += aLength - 1;
			for (unsigned i = 0; i < aLength; i++) {
				*aDst-- = *src++;
			}
		}
		else {
			unsigned char *src = _value.data() + _value.size() - 1;
			for (unsigned i = 0; i < aLength; i++) {
				*aDst++ = *src--;
			}
		}
		return aLength;
	}
	return 0;
}

BigInteger & BigInteger::operator=(const BigInteger & aRight)
{
	_isBigEndian = aRight._isBigEndian;
	_value = aRight._value;
	return *this;
}
