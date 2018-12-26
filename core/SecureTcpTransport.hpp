#pragma	once

#include "core/Transport.hpp"
#include "base/SecureTcpClientSocket.hpp"

namespace core
{
	class SecureTcpTransport : public Transport
	{
	public:
		typedef Reference<SecureTcpTransport> Ref;

	public:
		SecureTcpTransport(const std::string &aServer, int aPort);
		~SecureTcpTransport() override;

		bool IsReady() const override;
		bool Open() override;
		void Close() override;
		int GetLine(std::string &aLine, DWORD aTimeout = INFINITE) override;
		int PutLine(const std::string &aLine) override;

	protected:
		std::string				_server;
		int						_port = 0;
		SecureTcpClientSocket	_socket;
		char					_buffer[4096];
		char					*_head = _buffer;
		int						_inBuffer = 0;
	};
}
