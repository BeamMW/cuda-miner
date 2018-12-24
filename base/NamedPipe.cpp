#include "pch.hpp"
#include "NamedPipe.hpp"

NamedPipe::NamedPipe()
{
}

NamedPipe::~NamedPipe()
{
	close();
}

void NamedPipe::close()
{
	if (INVALID_HANDLE_VALUE != _hPipe) {
		CloseHandle(_hPipe);
		_hPipe = INVALID_HANDLE_VALUE;
		_bIsConnected = false;
	}
}

bool NamedPipe::create (
	LPCTSTR lpName,
	DWORD dwOpenMode,
	DWORD dwPipeMode,
	DWORD nMaxInstances,
	DWORD nOutBufferSize,
	DWORD nInBufferSize,
	DWORD nDefaultTimeOut,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes
)
{
	close();

	/* create named pipe */
	ZeroMemory(&saPipeSecurity, sizeof(saPipeSecurity));
	ZeroMemory(&sdPipeDescriptor, sizeof(sdPipeDescriptor));
	if (!InitializeSecurityDescriptor(&sdPipeDescriptor, SECURITY_DESCRIPTOR_REVISION)) {
		_hResult = GetLastError();
		return false;
	}
	if (!SetSecurityDescriptorDacl(&sdPipeDescriptor, TRUE, NULL, FALSE)) {
		_hResult = GetLastError();
		return false;
	}

	saPipeSecurity.nLength = sizeof(SECURITY_ATTRIBUTES);
	saPipeSecurity.lpSecurityDescriptor = &sdPipeDescriptor;
	saPipeSecurity.bInheritHandle = FALSE;

	_Name = _T("\\\\.\\pipe\\");
	_Name += lpName;
	_hPipe = CreateNamedPipe(
		_Name.c_str(), dwOpenMode, dwPipeMode,
		nMaxInstances, nOutBufferSize, nInBufferSize,
		nDefaultTimeOut, &saPipeSecurity);
	if (INVALID_HANDLE_VALUE == _hPipe) {
		_hResult = GetLastError();
	} else {
		_hResult = ERROR_SUCCESS;
	}
	return ERROR_SUCCESS == _hResult;
}

bool NamedPipe::connect(LPOVERLAPPED aOverlapped)
{
	if (ConnectNamedPipe(_hPipe, aOverlapped)) {
		_hResult = ERROR_SUCCESS;
		_bIsConnected = true;
		return true;
	} else {
		_hResult = GetLastError();
		if (ERROR_PIPE_CONNECTED == _hResult) {
			_bIsConnected = true;
			return true;
		}
		if (nullptr != aOverlapped) {
			return _bIsConnected = (ERROR_IO_PENDING == _hResult);
		}
		return false;
	}
}

bool NamedPipe::disconnect()
{
	if (DisconnectNamedPipe(_hPipe)) {
		_bIsConnected = false;
		_hResult = ERROR_SUCCESS;
		return true;
	} else {
		_hResult = GetLastError();
		return false;
	}
}

bool NamedPipe::open(
	LPCTSTR lpName,
	DWORD dwPipeMode
)
{
	close();
	_Name = _T("\\\\.\\pipe\\");
	_Name += lpName;
	_hPipe = CreateFile(_Name.c_str(),GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
	if (INVALID_HANDLE_VALUE == _hPipe) {
		_hResult = GetLastError();
	} else {
		if (SetNamedPipeHandleState(_hPipe,&dwPipeMode,NULL,NULL)) {
			_hResult = ERROR_SUCCESS;
			_bIsConnected = true;
		} else {
			_hResult = GetLastError();
		}
	}
	return ERROR_SUCCESS == _hResult;
}

bool NamedPipe::peek (
	LPVOID lpBuffer,
	DWORD nBufferSize,
	LPDWORD lpBytesRead,
	LPDWORD lpTotalBytesAvail,
	LPDWORD lpBytesLeftThisMessage
)
{
	if (PeekNamedPipe(_hPipe,lpBuffer,nBufferSize,lpBytesRead,lpTotalBytesAvail,lpBytesLeftThisMessage)) {
		_hResult = ERROR_SUCCESS;
	} else {
		_hResult = GetLastError();
	}
	return ERROR_SUCCESS == _hResult;
}

bool NamedPipe::transact (
	LPVOID lpInBuffer,
	DWORD nInBufferSize,
	LPVOID lpOutBuffer,
	DWORD nOutBufferSize,
	LPDWORD lpBytesRead,
	LPOVERLAPPED lpOverlapped
)
{
	if (TransactNamedPipe(_hPipe,lpInBuffer,nInBufferSize,lpOutBuffer,nOutBufferSize,lpBytesRead,lpOverlapped)) {
		_hResult = ERROR_SUCCESS;
	} else {
		_hResult = GetLastError();
	}
	return ERROR_SUCCESS == _hResult;
}

int NamedPipe::read (
	LPVOID lpBuffer,
	DWORD nNumberOfBytesToRead
)
{
	DWORD dwNumberOfBytesRead;
	if (ReadFile(_hPipe,lpBuffer,nNumberOfBytesToRead,&dwNumberOfBytesRead,NULL)) {
		_hResult = ERROR_SUCCESS;
		return dwNumberOfBytesRead;
	} else {
		_hResult = GetLastError();
		return 0;
	}
}

int NamedPipe::write (
	LPCVOID lpBuffer,
	DWORD nNumberOfBytesToWrite
)
{
	DWORD dwNumberOfBytesWritten;
	if (WriteFile(_hPipe,lpBuffer,nNumberOfBytesToWrite,&dwNumberOfBytesWritten,NULL)) {
		_hResult = ERROR_SUCCESS;
		return dwNumberOfBytesWritten;
	} else {
		_hResult = GetLastError();
		return 0;
	}
}

bool NamedPipe::wait (
	LPCTSTR lpNamedPipeName,
	DWORD nTimeOut
)
{
	std::wstring name;
	name = _T("\\\\.\\pipe\\");
	name += lpNamedPipeName;
	return 0 != WaitNamedPipe(name.c_str(),nTimeOut);
}
