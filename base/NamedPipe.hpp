#pragma	once

class NamedPipe
{
protected:
	HANDLE			_hPipe = INVALID_HANDLE_VALUE;
	std::wstring	_Name;
	HRESULT			_hResult = ERROR_SUCCESS;
	bool			_bIsConnected = false;

public:
	NamedPipe();
	~NamedPipe();

	void close();

	SECURITY_ATTRIBUTES saPipeSecurity;
	SECURITY_DESCRIPTOR sdPipeDescriptor;

	bool create (
		LPCTSTR lpName,										// pipe name
		DWORD dwOpenMode = PIPE_ACCESS_DUPLEX,				// pipe open mode
		DWORD dwPipeMode = PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE,	// pipe-specific modes
		DWORD nMaxInstances = 1,							// maximum number of instances
		DWORD nOutBufferSize = 4096,						// output buffer size
		DWORD nInBufferSize = 4096,							// input buffer size
		DWORD nDefaultTimeOut = INFINITE,					// time-out interval (milliseconds)
		LPSECURITY_ATTRIBUTES lpSecurityAttributes = NULL	// SD
	);

	operator HANDLE() { return _hPipe; }

	bool connect(LPOVERLAPPED aOverlapped);
	bool disconnect();
	bool open (
		LPCTSTR lpName,
		DWORD dwPipeMode = PIPE_READMODE_MESSAGE
	);

	__inline HRESULT getErrorCode() const { return _hResult; }
	__inline bool isConnected() const { return _bIsConnected; }

	bool peek (
		LPVOID lpBuffer,						// data buffer
		DWORD nBufferSize,						// size of data buffer
		LPDWORD lpBytesRead,					// number of bytes read
		LPDWORD lpTotalBytesAvail = NULL,		// number of bytes available
		LPDWORD lpBytesLeftThisMessage = NULL	// unread bytes
	);

	bool transact (
		LPVOID lpInBuffer,					// write buffer
		DWORD nInBufferSize,				// size of write buffer
		LPVOID lpOutBuffer,					// read buffer
		DWORD nOutBufferSize,				// size of read buffer
		LPDWORD lpBytesRead,				// bytes read
		LPOVERLAPPED lpOverlapped = NULL	// overlapped structure
	);

	int read (
		LPVOID lpBuffer,			// data buffer
		DWORD nNumberOfBytesToRead	// number of bytes to read
	);

	int write (
		LPCVOID lpBuffer,			// data buffer
		DWORD nNumberOfBytesToWrite	// number of bytes to write
	);

	static bool wait (
		LPCTSTR lpNamedPipeName,	// pipe name
		DWORD nTimeOut				// time-out interval (milliseconds)
	);
};
