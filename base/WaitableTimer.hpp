
#pragma	once

#ifdef _WIN32
struct WaitableTimer
{
	HANDLE	m_hEvent;

	WaitableTimer(BOOL bManualReset = FALSE, LPCTSTR lpName = nullptr)
	{
		m_hEvent = ::CreateWaitableTimer(nullptr,bManualReset,lpName);
	}

	~WaitableTimer()
	{
		if (nullptr != m_hEvent) {
			::CloseHandle(m_hEvent);
			m_hEvent = nullptr;
		}
	}

	__inline bool Cancel()
	{
		return 0 != ::CancelWaitableTimer(m_hEvent);
	}
#if 0
	__inline bool SetDueTime(const LARGE_INTEGER &aDueTime)
	{
		return 0 != ::SetWaitableTimer(m_hEvent, &aDueTime, 0, nullptr, nullptr, FALSE);
	}
#endif
	__inline bool SetDueTime(long aMsDelay)
	{
		LARGE_INTEGER liDueTime;
		liDueTime.QuadPart = -(INT64)(aMsDelay * 10000);
		return 0 != ::SetWaitableTimer(m_hEvent, &liDueTime, 0, nullptr, nullptr, FALSE);
	}

	__inline bool SetPeriod(long aMsDelay, long aMsPeriod)
	{
		LARGE_INTEGER liDueTime;
		liDueTime.QuadPart = -(INT64)(aMsDelay * 10000);
		return 0 != ::SetWaitableTimer(m_hEvent, &liDueTime, aMsPeriod, nullptr, nullptr, FALSE);
	}

	__inline HANDLE GetNativeHandle() const
	{
		return m_hEvent;
	}

	__inline operator bool () const
	{
		return nullptr != m_hEvent;
	}

	__inline bool Wait(DWORD aTimeout) const
	{
		DWORD state = WaitForSingleObject(m_hEvent, aTimeout);
		return WAIT_OBJECT_0 == state;
	}
};
#else
#include <sys/timerfd.h>
#include <poll.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

struct WaitableTimer
{
	int	m_fd = -1;

	WaitableTimer()
	{
		m_fd = timerfd_create(CLOCK_MONOTONIC, 0);
	}

	~WaitableTimer()
	{
		if (-1 != m_fd) {
			close(m_fd);
			m_fd = -1;
		}
	}

	inline bool Cancel()
	{
		struct itimerspec ts;
		ts.it_interval.tv_sec = 0;
		ts.it_interval.tv_nsec = 0;
		ts.it_value.tv_sec = 0;
		ts.it_value.tv_nsec = 0;

		return -1 != timerfd_settime(m_fd, 0, &ts, nullptr);
	}

	inline bool SetDueTime(long aMsDelay)
	{
		struct itimerspec ts;
		ts.it_interval.tv_sec = 0;
		ts.it_interval.tv_nsec = 0;
		ts.it_value.tv_sec = aMsDelay / 1000;
		ts.it_value.tv_nsec = (aMsDelay % 1000) * 1000000;

		return -1 != timerfd_settime(m_fd, 0, &ts, nullptr);
	}

	inline bool SetPeriod(long aMsDelay, long aMsPeriod)
	{
		struct itimerspec ts;
		ts.it_interval.tv_sec = aMsPeriod / 1000;
		ts.it_interval.tv_nsec = (aMsPeriod % 1000) * 1000000;
		ts.it_value.tv_sec = aMsDelay / 1000;
		ts.it_value.tv_nsec = (aMsDelay % 1000) * 1000000;

		return -1 != timerfd_settime(m_fd, 0, &ts, nullptr);
	}

	inline bool Wait(DWORD aTimeout) const
	{
		u_int64_t ticks;
		struct pollfd pfd;

		pfd.fd = m_fd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		int ret = poll(&pfd, 1, aTimeout);
		if (ret <= 0) {
			return false;
		}
		if ((pfd.revents & POLLIN) == 0) {
			return false;
		}
		if (read(m_fd, &ticks, sizeof(ticks)) != sizeof(ticks)) {
			return false;
		}
		return true;
	}

	inline int GetNativeHandle() const
	{
		return m_fd;
	}

	inline operator bool () const
	{
		return -1 != m_fd;
	}
};
#endif
