
#pragma	once

#include <atomic>
#include <condition_variable>
#include <chrono>

struct PulseSignal
{
	PulseSignal()
	{
	}

	~PulseSignal()
	{
	}

	__inline void Set()
	{
		cv.notify_all();
	}

	__inline bool Wait(unsigned aTimeout)
	{
		std::unique_lock<std::mutex> l(lock);
		auto now = std::chrono::system_clock::now();
		return std::cv_status::timeout != cv.wait_until(l, now + std::chrono::milliseconds(aTimeout));
	}

	std::mutex				lock;
	std::condition_variable	cv;
};

struct StaticSignal
{
	StaticSignal() : signaled(false)
	{
	}

	~StaticSignal()
	{
	}

	__inline void Reset()
	{
		signaled = false;
	}

	__inline void Set()
	{
		signaled = true;
		cv.notify_all();
	}

	__inline bool Wait(unsigned aTimeout)
	{
		std::unique_lock<std::mutex> l(lock);
		auto now = std::chrono::system_clock::now();
		return cv.wait_until(l, now + std::chrono::milliseconds(aTimeout), [this]() {return (bool)this->signaled; });
	}

	std::mutex				lock;
	std::condition_variable	cv;
	std::atomic_bool		signaled;
};
