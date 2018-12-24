#include "pch.hpp"
#include "Logging.hpp"
#include <fstream>
#include <iomanip>
#ifndef _WIN32
#include <sys/time.h>
#endif

static struct LogFile {
	void write(const std::string &aMsg)
	{
#if 0
		if (!_file.is_open()) {
			boost::filesystem::path path = MinersManager::GetAppDirectory().append("debug.log");
			_file.open(path.c_str(), std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
		}
		if (_file.is_open()) {
			_file << aMsg;
			_file.flush();
		}
#endif
		std::cout << aMsg << std::flush;
#ifdef _WIN32
		OutputDebugStringA(aMsg.c_str());
#endif
	}
	std::ofstream	_file;
} sLogFile;

LogOutput::LogOutput(unsigned aLevel)
{
	_allow = (sAllow | LogLevel::Error| LogLevel::Fatal) & aLevel;
	if (_allow) {
#ifdef _WIN32
		SYSTEMTIME st;
		GetLocalTime(&st);
		_stream << std::setfill('0') << std::setw(2) << st.wHour << ':';
		_stream << std::setfill('0') << std::setw(2) << st.wMinute << ':';
		_stream << std::setfill('0') << std::setw(2) << st.wSecond << ' ';
#else
		struct timespec ts = { 0,0 };
		struct tm tm = {};
		clock_gettime(CLOCK_REALTIME, &ts);
		time_t tim = ts.tv_sec;
		localtime_r(&tim, &tm);
		_stream << std::setfill('0') << std::setw(2) << tm.tm_hour << ':';
		_stream << std::setfill('0') << std::setw(2) << tm.tm_min << ':';
		_stream << std::setfill('0') << std::setw(2) << tm.tm_sec << ' ';
#endif
		switch (aLevel) {
		case LogLevel::Info:
			_stream << "[Info ]: ";
			break;
		case LogLevel::Debug:
			_stream << "[Debug]: ";
			break;
		case LogLevel::Error:
			_stream << "[Error]: ";
			break;
		case LogLevel::Fatal:
			_stream << "[Fatal]: ";
			break;
		case LogLevel::Trace:
			_stream << "[Trace]: ";
			break;
		case LogLevel::Warning:
			_stream << "[Warn ]: ";
			break;
		}
	}
}

void WriteToLogFile(const std::string &aMsg)
{
	sLogFile.write(aMsg);
}

LogOutput::LogWritter LogOutput::sWriteLog = WriteToLogFile;
unsigned LogOutput::sAllow = LogLevel::Info;

LogOutput::LogWritter LogOutput::GetLogWritter()
{
	return sWriteLog;
}

LogOutput::LogWritter LogOutput::SetLogWritter(LogOutput::LogWritter aLogWritter)
{
	LogWritter oldWritter = sWriteLog;
	sWriteLog = aLogWritter;
	return oldWritter;
}

void LogOutput::Allow(unsigned aLevel)
{
	sAllow |= aLevel;
}

void LogOutput::DisAllow(unsigned aLevel)
{
	sAllow &= ~aLevel;
}

void LogOutput::Set(unsigned aLevel)
{
	sAllow = aLevel;
}
