#pragma	once

#include <iostream>
#include <sstream>

struct LogLevel
{
	static const unsigned Info = 0x00000001;
	static const unsigned Debug = 0x00000002;
	static const unsigned Error = 0x00000004;
	static const unsigned Fatal = 0x00000008;
	static const unsigned Trace = 0x00000010;
	static const unsigned Warning = 0x00000020;
};

#undef LOG
#define	LOG(a) LogOutput(LogLevel::a).stream()
#define	DLOG() DebugOutput().stream()

class LogOutput
{
public:
	typedef void (*LogWritter)(const std::string &);

public:
	LogOutput(unsigned aLevel);
	~LogOutput()
	{
		if (_allow) {
			_stream << std::endl;
			sWriteLog(_stream.str());
		}
	}
	std::ostream& stream() { return _stream; }

	static LogWritter GetLogWritter();
	static LogWritter SetLogWritter(LogWritter aLogWritter);

	static void Allow(unsigned aLevel);
	static void DisAllow(unsigned aLevel);
	static void Set(unsigned aLevel);

private:
	static LogWritter sWriteLog;
	static unsigned sAllow;

private:
	bool				_allow;
	std::ostringstream	_stream;
};

class DebugOutput
{
public:
	~DebugOutput()
	{
		stream_ << std::endl;
		std::string str_newline(stream_.str());
#ifdef _WIN32
		OutputDebugStringA(str_newline.c_str());
#else
		std::cerr << str_newline;
#endif
	}
	std::ostream& stream() { return stream_; }

private:
	std::ostringstream stream_;
};
