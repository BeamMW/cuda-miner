#pragma	once

#include <list>

class JSonVar
{
public:
	typedef std::list<JSonVar> JSonVars;
	enum JsonType {
		jsonNull = 0,
		jsonArray,
		jsonObject,
		jsonNumber,
		jsonString
	};

public:
	JSonVar();
	JSonVar(const JSonVar &aObj);
	~JSonVar();

	inline bool IsNull() const { return jsonNull == mType; }
	inline bool IsArray() const { return jsonArray == mType; }
	inline bool IsString() const { return jsonString == mType; }
	inline bool IsObject() const { return jsonObject == mType; }
	inline bool IsNumber() const { return jsonNumber == mType; }
	inline bool GetBoolValue() const { return mValue == "true"; }
	inline long GetLongValue() const { return std::stol(mValue); }
	inline unsigned long GetULongValue() const { return std::stoul(mValue); }
	inline double GetDoubleValue() const { return std::stod(mValue); }
	inline const std::string & GetName() const { return mName; }
	inline const std::string & GetValue() const { return mValue; }
	inline const JSonVar * operator [] (unsigned int Index) const { return GetByIndex(Index); }
	inline const JSonVar * operator [] (const char *Name) const { return Search(Name); }
	inline const JSonVar * operator [] (const std::string &aName) const { return Search(aName); }
	const JSonVar * Search(const char *Name) const;
	const JSonVar * Search(const std::string &aName) const { return Search(aName.c_str()); }
	inline bool Exists(const char *aName) const { return nullptr != Search(aName); }
	inline bool Exists(const std::string &aName) const { return nullptr != Search(aName); }
	const JSonVar * GetByIndex(unsigned int Index) const;
	inline unsigned int GetArrayLen() const { return (unsigned int)mInternalVars.size(); }
	const JSonVars & GetInternal() const { return mInternalVars; }

	inline std::string GetStrValue(const char *aName)
	{
		const JSonVar *var = Search(aName);
		return ((nullptr != var) && var->IsString()) ? var->GetValue() : std::string();
	}

	inline long GetLongValue(const char *aName, long aDefault)
	{
		const JSonVar *var = Search(aName);
		return (nullptr != var) ? var->GetLongValue() : aDefault;
	}

	inline void Clear()
	{
		mName.clear();
		mValue.clear();
		mInternalVars.clear();
		mType = jsonNull;
	}

	static bool ParseJSon(const char *Buffer, JSonVar &aResult);

protected:
	std::string mName;
	std::string mValue;
	JsonType mType;
	JSonVars mInternalVars;

	const char* ParseYourVar(const char *&Buffer);
	const char* ParseObject(const char *&Buffer);
	const char* ParseVar(const char *&Buffer);
	const char* ParseArray(const char *&Buffer);
	const char* ParseString(const char *&Buffer, std::string &Var, char aQuotes);
	const char* ParseNumberValue(const char *&Buffer);
	const char* ParseYourArrayValue(const char *&Buffer);
};
