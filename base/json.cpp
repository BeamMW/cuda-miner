#include "pch.hpp"
#include "json.hpp"

inline const char * EatEmpty(const char *str)
{
    while (*str==' ' || *str=='\t' || *str=='\r' || *str=='\n') str++;
    return str;
}

bool JSonVar::ParseJSon(const char *aBuffer, JSonVar &aResult)
{
    size_t len = strlen(aBuffer);
    const char *begin = aBuffer;
    const char *res = aResult.ParseYourVar(aBuffer);
    if (res) {
        //printf("Some error near %lu (%20s)\n",res-Begin,res);
        printf("Some JSon error near pos %u, total len %zu, from text [%.20s]\n",(int)(res-begin),len,res);
        return false;
    }
    return true;
}

const char* JSonVar::ParseString(const char *&aBuffer, std::string &Var, char aQuotes)
{
	if (*aBuffer != aQuotes) {
        return aBuffer;
    }
    aBuffer++;
    Var = "";
	while (*aBuffer != aQuotes && *aBuffer != '\0') {
        Var += *aBuffer++;
    }
	if (*aBuffer == aQuotes) {
        aBuffer++;
    } else {
        return aBuffer;
    }
    return nullptr;
}

const char* JSonVar::ParseNumberValue(const char *&aBuffer)
{
	if (!mValue.empty()) {
		return aBuffer;
	}
	int points = 0;
	if ('+' == *aBuffer) {
		mValue = *aBuffer++;
	}
	else if ('-' == *aBuffer) {
		mValue = *aBuffer++;
	}
	else if ('.' == *aBuffer) {
		points = 1;
		mValue = *aBuffer++;
	}
    while (*aBuffer!='\0')
    {
        if (*aBuffer==',' || *aBuffer==']' || *aBuffer=='}' || isspace(*aBuffer)) {
            mType = jsonNumber;
            break;
        }
		if ('.' == *aBuffer) {
			points++;
			if (points > 1) {
				return aBuffer;
			}
		}
		else if (*aBuffer < '0' || *aBuffer > '9') {
			return aBuffer;
		}
		mValue += *aBuffer++;
    }
    if (*aBuffer=='\0') {
        return aBuffer;
    }
    return nullptr;
}

const char* JSonVar::ParseVar(const char *&aBuffer)
{
    if (*aBuffer!='"') {
        return aBuffer;
    }
	const char* res = ParseString(aBuffer, mName, '"');
    if (res) {
        return res;
    }
    aBuffer = EatEmpty(aBuffer);
    if (*aBuffer!=':') {
        return aBuffer;
    }
    aBuffer++;
    aBuffer = EatEmpty(aBuffer);
    if (*aBuffer=='{') {
        return ParseObject(aBuffer);
    }
    if (*aBuffer=='"') {
		res = ParseString(aBuffer, mValue, '"');
        if (res) {
            return res;
        }
        mType = jsonString;
        return nullptr;
    }
    if (*aBuffer=='[') {
		res = ParseArray(aBuffer);
        if (res) {
            return res;
        }
        return nullptr;
    }
    if (strncmp(aBuffer,"null",4)==0) {
        aBuffer += 4;
        aBuffer = EatEmpty(aBuffer);
        mType = jsonNull;
        return nullptr;
    }
    if (strncmp(aBuffer,"false",5)==0) {
        aBuffer += 5;
        aBuffer = EatEmpty(aBuffer);
        mType = jsonString;
        mValue = "false";
        return nullptr;
    }
    if(strncmp(aBuffer,"true",4)==0) {
        aBuffer += 4;
        aBuffer = EatEmpty(aBuffer);
		mType = jsonString;
		mValue = "true";
        return nullptr;
    }
    if (*aBuffer=='-' || *aBuffer=='+' || *aBuffer == '.' || (*aBuffer>='0' && *aBuffer<='9')) {
		res = ParseNumberValue(aBuffer);
        if (res) {
            return res;
        }
        return nullptr;
    }
    return aBuffer;
}

const char* JSonVar::ParseArray(const char *&aBuffer)
{
    mInternalVars.clear();
    if (*aBuffer!='[') {
        return aBuffer;
    }
    aBuffer++;
    mType = jsonArray;
    const char* res;
    aBuffer = EatEmpty(aBuffer);
    while(*aBuffer!=']' && *aBuffer!='\0')
    {
        mInternalVars.push_back(JSonVar());
		res = mInternalVars.back().ParseYourArrayValue(aBuffer);
        if (res) {
            return res;
        }
        aBuffer = EatEmpty(aBuffer);
        if (*aBuffer==',') {
            aBuffer++;
            aBuffer = EatEmpty(aBuffer);
            if (*aBuffer=='\0') {
                return aBuffer;
            }
            continue;
        }
        aBuffer = EatEmpty(aBuffer);
        if (*aBuffer!=']') {
            return aBuffer;
        }
    }
    aBuffer = EatEmpty(aBuffer);
    if (*aBuffer==']') {
        aBuffer++;
    } else {
        return aBuffer;
    }
    return nullptr;
}

const char* JSonVar::ParseObject(const char *&aBuffer)
{
    if (*aBuffer!='{') {
        printf("'{' expected, found '%c'\n", *aBuffer);
        return aBuffer;
    }
    aBuffer++;
    mType = jsonObject;
    const char* res;
    mInternalVars.clear();
    while (*aBuffer!='}' && *aBuffer!='\0')
    {
        aBuffer=EatEmpty(aBuffer);
        if (*aBuffer!='"') {//name var expected
            printf("'\"' expected, found '%c'\n", *aBuffer);
            return aBuffer;
        }
        mInternalVars.push_back(JSonVar());
		res = mInternalVars.back().ParseYourVar(aBuffer);
        if (res) {
            return res;
        }
        aBuffer = EatEmpty(aBuffer);
        if (*aBuffer==',') {
            aBuffer++;
            aBuffer = EatEmpty(aBuffer);
            if (*aBuffer=='\0') {
                return aBuffer;
            }
            continue;
        }
        aBuffer = EatEmpty(aBuffer);
        if (*aBuffer!='}') {
            printf("'}' expected, found '%c'\n", *aBuffer);
            return aBuffer;
        }
    }
    aBuffer = EatEmpty(aBuffer);
    if (*aBuffer=='}') {
        aBuffer++;
    } else {
        return aBuffer;
    }
    return nullptr;
}

const char* JSonVar::ParseYourVar(const char *&aBuffer)
{
    aBuffer = EatEmpty(aBuffer);
    if (*aBuffer=='{') {
        return ParseObject(aBuffer);
    }
    if (*aBuffer=='"') {
        return ParseVar(aBuffer);
    }
    if (*aBuffer=='[') {
        return ParseArray(aBuffer);
    }
    return aBuffer;
}

const char* JSonVar::ParseYourArrayValue(const char *&aBuffer)
{
    aBuffer = EatEmpty(aBuffer);
    if (*aBuffer=='{') {
        return ParseObject(aBuffer);
    }
    if (*aBuffer=='"') {
		const char *res = ParseString(aBuffer, mValue, '"');
		if (nullptr == res) {
			mType = JSonVar::jsonString;
		}
        return res;
    }
	if (*aBuffer == '\'') {
		const char *res = ParseString(aBuffer, mValue, '\'');
		if (nullptr == res) {
			mType = JSonVar::jsonString;
		}
		return res;
	}
	if (*aBuffer == '[') {
        return ParseArray(aBuffer);
    }
    if (strncmp(aBuffer,"null",4)==0) {
        aBuffer+=4;
        aBuffer=EatEmpty(aBuffer);
        mType = jsonNull;
        return nullptr;
    }
    if (strncmp(aBuffer,"false",5)==0) {
        aBuffer += 5;
        aBuffer = EatEmpty(aBuffer);
		mType = jsonString;
		mValue = "false";
        return nullptr;
    }
    if (strncmp(aBuffer,"true",4)==0) {
        aBuffer += 4;
        aBuffer = EatEmpty(aBuffer);
		mType = jsonString;
		mValue = "true";
        return nullptr;
    }
    if (*aBuffer=='-' || *aBuffer=='+' || *aBuffer == '.' || (*aBuffer>='0' && *aBuffer<='9')) {
		const char *res = ParseNumberValue(aBuffer);
        if (res) {
            return res;
        }
        return nullptr;
    }
    return aBuffer;
}

JSonVar::JSonVar() : mType(jsonNull)
{
}

JSonVar::JSonVar(const JSonVar &aObj) = default;

JSonVar::~JSonVar()
{
}

const JSonVar * JSonVar::GetByIndex(unsigned int Index) const
{
    for (JSonVars::const_iterator obj = mInternalVars.begin(); mInternalVars.end() != obj; obj++, Index--) {
        if (0 == Index) {
            return &*obj;
        }
    }
    return nullptr;
}

const JSonVar * JSonVar::Search(const char *aName)const
{
    for (JSonVars::const_iterator obj = mInternalVars.begin(); mInternalVars.end() != obj; obj++) {
        if (obj->GetName() == aName) {
            return &*obj;
        }
    }
    return nullptr;
}
