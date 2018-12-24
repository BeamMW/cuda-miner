#pragma	once

#include "core/Transport.hpp"
#include "base/TcpAsyncClientSocket.hpp"

namespace core
{
	class TcpAsyncTransport : public Transport
	{
	public:
		typedef Reference<TcpAsyncTransport> Ref;

	public:
		TcpAsyncTransport(const std::string &aServer, int aPort);
		~TcpAsyncTransport() override;

		bool IsReady() const override;
		bool Open() override;
		void Close() override;
		int GetLine(std::string &aLine, DWORD aTimeout = INFINITE) override;
		int PutLine(const std::string &aLine) override;

	protected:
		std::string				_server;
		int						_port = 0;
		TcpAsyncClientSocket	_socket;
		char					_buffer[4096];
		char					*_head = _buffer;
		int						_inBuffer = 0;
	};
}
