#pragma	once

class BigInteger
{
public:
	BigInteger() {}
	BigInteger(unsigned nBytes, bool aBigEndian) : _value(nBytes), _isBigEndian(aBigEndian) {}
	BigInteger(const void *aData, unsigned nBytes, bool aBigEndian) : _value(nBytes), _isBigEndian(aBigEndian)
	{
		memcpy(GetBytes(), aData, nBytes);
	}
	BigInteger(const std::string &aHex, bool aBigEndian, int *aErrorAtPos = nullptr);

	unsigned GetBytesLength() const { return (unsigned)_value.size(); }

	unsigned char * GetBytes() { return &_value.front(); }
	const unsigned char * GetBytes() const { return &_value.front(); }

	bool IsBigEndian() const { return _isBigEndian; }
	bool IsNull() const { return _value.empty(); }
	void SetNull() { _value.clear(); }

	void SetBigEndian(bool aBigEndian);

	bool Import(const std::string &aHex, bool aBigEndian, int *aErrorAtPos = nullptr);

	int GetLower(unsigned char *aDst, unsigned aLength, bool aDstIsBigEndian);
	int GetUpper(unsigned char *aDst, unsigned aLength, bool aDstIsBigEndian);

	void ToString(std::string &aDst, bool aBigEndian) const;
	std::string ToString(bool aBigEndian) const
	{
		std::string s;
		ToString(s, aBigEndian);
		return s;
	}

	bool Equals(const BigInteger &aRight) const;

	BigInteger & operator = (const BigInteger &aRight);
	bool operator < (const BigInteger &aRight) const;
	bool operator <= (const BigInteger &aRight) const;
	bool operator == (const BigInteger &aRight) const;
	bool operator != (const BigInteger &aRight) const;

protected:
	std::vector<unsigned char>	_value;
	bool						_isBigEndian = false;
};
