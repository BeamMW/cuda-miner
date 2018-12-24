
#pragma	once

#ifdef _WIN32
namespace win32
{
	struct Event
	{
		HANDLE	m_hEvent;

		Event(BOOL bInitialState = FALSE, BOOL bManualReset = FALSE, LPCTSTR lpName = NULL)
		{
			if (bInitialState) {
				printf(" ");
			}
			m_hEvent = CreateEvent(NULL, bManualReset, bInitialState, lpName);
		}

		~Event()
		{
			if (NULL != m_hEvent) {
				CloseHandle(m_hEvent);
				m_hEvent = NULL;
			}
		}

		__inline void ResetEvent()
		{
			::ResetEvent(m_hEvent);
		}

		__inline void SetEvent()
		{
			::SetEvent(m_hEvent);
		}

		__inline HANDLE GetNativeHandle() const
		{
			return m_hEvent;
		}

		__inline bool Wait(DWORD aTimeout)
		{
			DWORD state = WaitForSingleObject(m_hEvent, aTimeout);
			return WAIT_OBJECT_0 == state;
		}
	};
}
#endif
