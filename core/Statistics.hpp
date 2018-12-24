#pragma	once

#include <atomic>
#include <chrono>
#include <mutex>

#define	_PERFORMANCE_THREADSAFE	1

namespace core
{
	class Performance
	{
	public:
		typedef std::chrono::time_point<std::chrono::system_clock> Time;

	public:
		void inc()
		{
#if _PERFORMANCE_THREADSAFE
			std::unique_lock<std::mutex> lock(_cs);
#endif
			_count++;
			_total++;
		}

		void inc(unsigned aCount)
		{
#if _PERFORMANCE_THREADSAFE
			std::unique_lock<std::mutex> lock(_cs);
#endif
			_count += aCount;
			_total += aCount;
		}

		void pop(uint64_t &aPeriod, uint64_t &aCount)
		{
#if _PERFORMANCE_THREADSAFE
			std::unique_lock<std::mutex> lock(_cs);
#endif
			auto now = std::chrono::system_clock::now();
			aPeriod = std::chrono::duration_cast<std::chrono::milliseconds>(now - _start).count();
			_start = std::chrono::system_clock::now();
			aCount = _count;
			_count = 0;
		}
#if 0
		float pop()
		{
			uint64_t period;
			uint64_t count;
			pop(period, count);
			return float(period ? (double(1000*count) / double(period)) : 0);
		}
#endif
		void total(uint64_t &aPeriod, uint64_t &aCount)
		{
#if _PERFORMANCE_THREADSAFE
			std::unique_lock<std::mutex> lock(_cs);
#endif
			auto now = std::chrono::system_clock::now();
			aPeriod = std::chrono::duration_cast<std::chrono::milliseconds>(now - _begin).count();
			aCount = _total;
		}

		float total()
		{
			uint64_t period;
			uint64_t count;
			total(period, count);
			return float(period ? (double(1000*count) / double(period)) : 0);
		}

		uint64_t getTotal() const { return _total; }

		void SetBegin()
		{
			_begin = std::chrono::system_clock::now();
		}

	protected:
#if _PERFORMANCE_THREADSAFE
		std::mutex	_cs;
#endif
		Time		_begin = std::chrono::system_clock::now();
		Time		_start = _begin;
		uint64_t	_count = 0;
		uint64_t	_total = 0;
	};

	struct Statistics
	{
		std::string			name;
		std::atomic_long	accepted;
		std::atomic_long	rejected;
		std::atomic_long	fails;
		Performance			hashes;
		Performance			solutions;
		Statistics() : accepted(0), rejected(0), fails(0) {}
	};

	struct Metrics
	{
		std::string	name;
		unsigned	accepted;
		unsigned	rejected;
		unsigned	fails;
		uint64_t	hashes;
		uint64_t	solutions;
		float		hashRateNow;
		float		hashRateTotal;
		float		solutionRateNow;
		float		solutionRateTotal;

		void Clear()
		{
			name.clear();
			accepted = 0;
			rejected = 0;
			fails = 0;
			hashes = 0;
			solutions = 0;
			hashRateNow = 0;
			hashRateTotal = 0;
			solutionRateNow = 0;
			solutionRateTotal = 0;
		}

		Metrics & operator += (const Metrics &aRight)
		{
			accepted += aRight.accepted;
			rejected += aRight.rejected;
			fails += aRight.fails;
			hashes += aRight.hashes;
			solutions += aRight.solutions;
			hashRateNow += aRight.hashRateNow;
			hashRateTotal += aRight.hashRateTotal;
			solutionRateNow += aRight.solutionRateNow;
			solutionRateTotal += aRight.solutionRateTotal;
			return *this;
		}
	};

	template <int cAvgCount> class Average
	{
		struct Item
		{
			uint64_t	period = 0;
			uint64_t	count = 0;
		};

		Item _items[cAvgCount];
		unsigned _tail = 0;

	public:
		float add(Performance &aSrc)
		{
			unsigned i = _tail++ % cAvgCount;
			aSrc.pop(_items[i].period, _items[i].count);
			return float(_items[i].period ? (double(1000 * _items[i].count) / double(_items[i].period)) : 0);
		}

		float get() const
		{
			uint64_t period = 0;
			uint64_t count = 0;
			for (unsigned i = 0; i < cAvgCount; i++) {
				period += _items[i].period;
				count += _items[i].count;
			}
			return float(period ? (double(1000 * count) / double(period)) : 0);
		}
	};
}
