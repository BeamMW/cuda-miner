#pragma	once

#include <thread>
#include "base/Dynamic.hpp"
#include "base/Reference.hpp"
#include <mutex>
#ifdef _WIN32
#include "base/Event.hpp"
#else
#include <poll.h>
#include <unistd.h>
#endif

namespace core
{
	class Service
	{
	public:
#ifdef _WIN32
		typedef HANDLE Handle;
#else
		typedef int Handle;
#endif
		class ITask;
		typedef Reference<ITask> Task;
		class ITask : public Dynamic
		{
		public:
			virtual bool Execute() = 0;
			virtual Handle GetHandle() = 0;
		};
#ifdef _WIN32
		class Watcher
		{
		public:
			Watcher();
			void Add(Task aTask);
			void Remove(Task aTask);
			void Process(volatile bool &aExit);
			void Clear();
			void RequestUpdate()
			{
				_update.SetEvent();
			}

		protected:
			std::mutex			_cs;
			win32::Event		_update;
			std::vector<HANDLE>	_handlers;
			std::vector<Task>	_tasks;
			std::list<Task>		_toAdd;
			std::list<Task>		_toRemove;
		};
#else
		class Notifier
		{
		public:
			Notifier()
			{
				if (pipe(_fd) < 0) {
					_fd[0] = _fd[1] = -1;
				}
			}
			~Notifier()
			{
				if (-1 != _fd[0]) {
					close(_fd[0]);
				}
				if (-1 != _fd[1]) {
					close(_fd[1]);
				}
			}
			int read(void *buf, size_t count)
			{
				if (-1 != _fd[0]) {
					return ::read(_fd[0], buf, count);
				}
				return -1;
			}
			int write(void *buf, size_t count)
			{
				if (-1 != _fd[1]) {
					return ::write(_fd[1], buf, count);
				}
				return -1;
			}
			bool notify()
			{
				if (-1 != _fd[1]) {
					uint64_t d = 1;
					return ::write(_fd[1], &d, sizeof(d));
				}
				return false;
			}
			int getReadFd() const { return _fd[0]; }
			int getWriteFd() const { return _fd[1]; }
		protected:
			int		_fd[2];
		};

		class Watcher
		{
		public:
			Watcher();
			void Add(Task aTask);
			void Remove(Task aTask);
			void Process(volatile bool &aExit);
			void Clear();
			void RequestUpdate()
			{
				_update.notify();
			}

		protected:
			std::mutex				_cs;
			Notifier				_update;
			std::vector<struct pollfd>	_descriptors;
			std::vector<Task>		_tasks;
			std::list<Task>			_toAdd;
			std::list<Task>			_toRemove;
		};
#endif
	public:
		static bool Init();
		static void Done();

		static void AddTask(Task aTask);
		static void RemoveTask(Task aTask);

	protected:
		static void Run();

	protected:
		static volatile bool	_exit;
		static std::thread		_thread;
		static Watcher			_watcher;
	};
}

