#pragma	once

#include "base/Dynamic.hpp"
#include "base/Reference.hpp"

namespace core
{
	class Transport : public Dynamic
	{
	public:
		typedef Reference<Transport> Ref;

	public:
		virtual bool IsReady() const = 0;
		virtual bool Open() = 0;
		virtual void Close() = 0;
		virtual int GetLine(std::string &aLine, DWORD aTimeout = INFINITE) = 0;
		virtual int PutLine(const std::string &aLine) = 0;
	};
}
