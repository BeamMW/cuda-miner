#pragma once

inline std::wstring IntToWstring(const int _num)
{
	return std::to_wstring(_num);
}

inline std::string UintToString(const unsigned int _num)
{
	return std::to_string(_num);
}

inline std::wstring UintToWstring(const unsigned int _num)
{
	return std::to_wstring(_num);
}

inline std::string IntToString(const int _num)
{
	return std::to_string(_num);
}

inline std::string DoubleToString(const double _dbl)
{
	return std::to_string((double)_dbl);
}

inline std::wstring DoubleToWstring(const double _dbl)
{
	return std::to_wstring((double)_dbl);
}

inline std::string WstringToString (const std::wstring& _wstr)
{	
	return std::string(_wstr.begin(), _wstr.end());
}

inline std::wstring StringToWstring (const std::string& _str)
{	
	return std::wstring(_str.begin(), _str.end());
}

inline int StringToInt(const std::string& _str)
{ 
	return std::stoi(_str);
}

inline int WstringToInt(const std::wstring& _str)
{
	return std::stoi(_str);
}

inline double StringToDouble(const std::string& _str)
{
	return std::stod(_str);
}

inline double WstringToDouble(const std::wstring& _str)
{
	return std::stod(_str);
}

int HexToBin(const std::string &aSrc, unsigned char *aDst, size_t aDstLen);
