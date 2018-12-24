#pragma	once

class CommandLineReader
{
public:
	struct Token
	{
		Token() : _str(nullptr), _length(0) {}
		Token(const char *aStr, unsigned aLength) : _str(aStr), _length(aLength) {}

		void clear()
		{
			_str = nullptr;
			_length = 0;
		}

		void reset(const char *aStr, unsigned aLength)
		{
			_str = aStr;
			_length = aLength;
		}

		std::string toString() const
		{
			return _length ? std::string(_str, _length) : std::string(_str);
		}

		std::string & toString(std::string &aStr) const
		{
			if (_length) {
				aStr.assign(_str, _length);
			}
			else {
				aStr.assign(_str);
			}
			return aStr;
		}

		bool beginWith(const char *aStr) const
		{
			unsigned len = aStr ? (unsigned)strlen(aStr) : 0;
			if (len > _length) {
				return false;
			}
			return 0 == strncmp(_str, aStr, len);
		}

		operator bool() const { return nullptr != _str; }

		bool operator == (const char *aRight) const
		{
			if (nullptr == _str || nullptr == aRight) {
				return false;
			}
			if (0 == _length) {
				return 0 == strcmp(_str, aRight);
			}
			for (unsigned i = 0; i < _length; i++, aRight++) {
				if (_str[i] != *aRight) {
					return false;
				}
			}
			return 0 == *aRight;
		}

		const char	*_str;
		unsigned	_length;
	};

public:
	CommandLineReader(int argc, char **argv) : _argc(argc), _argv(argv)
	{
		if (_argc) {
			_argc--;
			_argv++;
		}
		parse();
	}

	bool hasToken() const { return 0 != _argc; }
	bool isOption() const { return (0 != _argc) && ('-' == _argv[0][0]); }
	bool isLongOption() const { return (0 != _argc) && ('-' == _argv[0][0]) && ('-' == _argv[0][1]); }
	bool hashValue() const { return _value; }
	bool pop()
	{
		if (_argc) {
			_argc--;
			_argv++;
			parse();
		}
		return 0 != _argc;
	}

	const Token & getName() const { return _name; }
	const Token & getValue() const { return _value; }
	const char * getRaw() const { return _argv[0]; }

	bool operator == (const char *aRight) const { return _name == aRight; }

protected:
	void parse()
	{
		if (*_argv) {
			if (const char *eq = strchr(_argv[0], '=')) {
				_name.reset(_argv[0], unsigned(eq - _argv[0]));
				eq++;
				if ('"' == *eq || '\'' == *eq) {
					if (const char *ep = strchr(eq + 1, *eq)) {
						_value.reset(eq + 1, unsigned(ep - (eq + 1)));
					}
					else {
						_value.reset(eq, 0);
					}
				}
				else {
					_value.reset(eq, 0);
				}
			}
			else {
				_name.reset(_argv[0], 0);
				_value.clear();
			}
		}
	}

protected:
	int		_argc;
	char	**_argv;
	Token	_name;
	Token	_value;
};
