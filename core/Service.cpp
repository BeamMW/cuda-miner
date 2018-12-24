#include "pch.hpp"
#include "Service.hpp"

using namespace core;

volatile bool Service::_exit = false;
std::thread Service::_thread;
Service::Watcher Service::_watcher;

#ifdef _WIN32
Service::Watcher::Watcher()
{
	_handlers.reserve(8);
	_tasks.reserve(8);
	_handlers.push_back(_update.GetNativeHandle());
}

void Service::Watcher::Add(Task aTask)
{
	if (aTask) {
		if (Handle handle = aTask->GetHandle()) {
			if (true) {
				std::unique_lock<std::mutex> lock(_cs);
				_toAdd.push_back(aTask);
			}
			_update.SetEvent();
		}
	}
}

void Service::Watcher::Remove(Task aTask)
{
	if (aTask) {
		if (Handle handle = aTask->GetHandle()) {
			if (true) {
				std::unique_lock<std::mutex> lock(_cs);
				_toRemove.push_back(aTask);
			}
			_update.SetEvent();
		}
	}
}

void Service::Watcher::Process(volatile bool &aExit)
{
	DWORD result = WaitForMultipleObjects((DWORD)_handlers.size(), _handlers.data(), FALSE, INFINITE);
	if (aExit) {
		return;
	}
	if (WAIT_OBJECT_0 == result) {
		std::unique_lock<std::mutex> lock(_cs);
		while (_toRemove.size()) {
			Task task = _toRemove.front();
			_toRemove.pop_front();
			auto h = _handlers.begin();
			for (auto t = _tasks.begin(); _tasks.end() != t; t++) {
				if (t->get() == task.get()) {
					_handlers.erase(h);
					_tasks.erase(t);
					break;
				}
				h++;
			}
		}
		while (_toAdd.size()) {
			Task task = _toAdd.front();
			_toAdd.pop_front();
			_tasks.push_back(task);
			_handlers.push_back(task->GetHandle());
		}
	}
	else if (WAIT_TIMEOUT == result) {
	}
	else if (result <= (WAIT_OBJECT_0 + (DWORD)_handlers.size())) {
		_tasks[result - WAIT_OBJECT_0 - 1]->Execute();
	}
}

void Service::Watcher::Clear()
{
	_toAdd.clear();
	_toRemove.clear();
	_handlers.clear();
	_tasks.clear();
}
#else
Service::Watcher::Watcher()
{
	_descriptors.reserve(8);
	_tasks.reserve(8);
	struct pollfd pfd;
	pfd.fd = _update.getReadFd();
	pfd.events = POLLIN;
	_descriptors.push_back(pfd);
}

void Service::Watcher::Add(Task aTask)
{
	if (aTask) {
		if (aTask->GetHandle() > 0) {
			if (true) {
				std::unique_lock<std::mutex> lock(_cs);
				_toAdd.push_back(aTask);
			}
			_update.notify();
		}
	}
}

void Service::Watcher::Remove(Task aTask)
{
	if (aTask) {
		if (aTask->GetHandle() > 0) {
			if (true) {
				std::unique_lock<std::mutex> lock(_cs);
				_toRemove.push_back(aTask);
			}
			_update.notify();
		}
	}
}

void Service::Watcher::Process(volatile bool &aExit)
{
	int result = poll(_descriptors.data(), _descriptors.size(), INFINITE);
	if (aExit || result <= 0) {
		return;
	}
	uint64_t dummy;
	if (_descriptors[0].revents & POLLIN) {
		std::unique_lock<std::mutex> lock(_cs);
		result--;
		read(_descriptors[0].fd, &dummy, sizeof(dummy));
		while (_toRemove.size()) {
			Task task = _toRemove.front();
			_toRemove.pop_front();
			auto h = _descriptors.begin();
			for (auto t = _tasks.begin(); _tasks.end() != t; t++) {
				if (t->get() == task.get()) {
					_descriptors.erase(h);
					_tasks.erase(t);
					break;
				}
				h++;
			}
		}
		while (_toAdd.size()) {
			Task task = _toAdd.front();
			_toAdd.pop_front();
			_tasks.push_back(task);
			struct pollfd pfd;
			pfd.fd = task->GetHandle();
			pfd.events = POLLIN;
			_descriptors.push_back(pfd);
		}
	}
	for (unsigned i = 1; (result > 0) && (i < _descriptors.size()); i++) {
		if (_descriptors[i].revents & POLLIN) {
			read(_descriptors[i].fd, &dummy, sizeof(dummy));
			_tasks[i - 1]->Execute();
		}
	}
}

void Service::Watcher::Clear()
{
	_toAdd.clear();
	_toRemove.clear();
	_descriptors.clear();
	_tasks.clear();
}
#endif

bool Service::Init()
{
	_thread = std::thread(&Service::Run);
	return std::thread::id() != _thread.get_id();
}

void Service::Run()
{
	while (!_exit)
	{
		_watcher.Process(_exit);
	}
	_watcher.Clear();
}

void Service::Done()
{
	_exit = true;
	_watcher.RequestUpdate();
	_thread.join();
}

void Service::AddTask(Task aTask)
{
	_watcher.Add(aTask);
}

void Service::RemoveTask(Task aTask)
{
	_watcher.Remove(aTask);
}
