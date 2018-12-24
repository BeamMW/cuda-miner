
#pragma	once

#ifdef _WIN32
class Semaphore
{
private:			
	HANDLE handle;			

public:
	Semaphore(int initValue = 0, LONG lMaximumCount = 2000000000)
	{
		handle = CreateSemaphore(NULL, initValue, lMaximumCount, NULL);
	}

	~Semaphore()
	{
		if (NULL != handle) {
			CloseHandle(handle);
			handle = NULL;
		}
	}

	HANDLE GetNativeHandle()
	{
		return handle;
	}

	__inline void unlock(int count)
	{
		::ReleaseSemaphore(handle, count, NULL);
	}

	__inline void unlock()
	{
		::ReleaseSemaphore(handle, 1, NULL);
	}

	__inline bool wait()
	{
		return WAIT_OBJECT_0 == WaitForSingleObject(handle, INFINITE);
	}

	__inline void operator ++ (int)
	{
		::ReleaseSemaphore(handle,1,NULL);
	}
};
#else
#include <semaphore.h>

class Semaphore
{
public:
	Semaphore(int initValue = 0)
	{
		sem_init(&_semaphore, 0, initValue);
	}

	~Semaphore()
	{
	}

	__inline void unlock(int count)
	{
		while (count--) {
			sem_post(&_semaphore);
		}
	}

	__inline void unlock()
	{
		sem_post(&_semaphore);
	}

	__inline bool wait()
	{
		return 0 == sem_wait(&_semaphore);
	}

	__inline void operator ++ (int)
	{
		sem_post(&_semaphore);
	}

private:
	sem_t	_semaphore;
};
#endif
