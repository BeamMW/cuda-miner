#include "pch.hpp"
#include "base/Logging.hpp"
#include "base/WaitableTimer.hpp"
#include "base/Semaphore.hpp"
#include "base/CommandLineReader.hpp"
#include "core/Service.hpp"
#include "EquihashFarm.hpp"
#include "EquihashStratumWorker.hpp"
#if USE_OPENCL
#include "cl/EquihashClMiner.hpp"
#endif
#if USE_CUDA
#include "cuda/EquihashCudaMiner.hpp"
#endif
#include <iomanip>

#define BEAM_EQUIHASH_VERSION "1.0.0.61"

static void PrintVersion()
{
	printf("beam-equihash version " BEAM_EQUIHASH_VERSION "\n");
}

class PrintStatisticsTask : public core::Service::ITask
{
public:
	PrintStatisticsTask(const EquihashFarm &aFarm) : _farm(aFarm)
	{
//		_timer.SetPeriod(30000, 30000);
		_timer.SetPeriod(10000, 10000);
	}

	bool Execute() override
	{
		_farm.PrintStatistics();
		return true;
	}

	core::Service::Handle GetHandle() override
	{
		return _timer.GetNativeHandle();
	}

protected:
	const EquihashFarm	&_farm;
	WaitableTimer		_timer;
};
#ifdef _WIN32
class ASyncLogWritter : public core::Service::ITask
{
public:
	typedef Reference<ASyncLogWritter> Ref;

public:
	ASyncLogWritter(LogOutput::LogWritter aWritter) : _writter(aWritter)
	{
		sInstance = this;
		LogOutput::SetLogWritter(AsyncLogWritter);
	}

	bool Execute() override
	{
		std::string msg;
		if (true) {
			std::unique_lock<std::mutex> lock(_cs);
			if (_queue.size()) {
				msg = _queue.front();
				_queue.pop_front();
			}
		}
		if (!msg.empty()) {
			_writter(msg);
		}
		return true;
	}

	core::Service::Handle GetHandle() override
	{
		return _available.GetNativeHandle();
	}

	void Add(const std::string &aMsg)
	{
		if (true) {
			std::unique_lock<std::mutex> lock(_cs);
			_queue.push_back(aMsg);
		}
		_available++;
	}

	static void AsyncLogWritter(const std::string &aMsg)
	{
		sInstance->Add(aMsg);
	}

protected:
	std::mutex				_cs;
	Semaphore				_available;
	std::list<std::string>	_queue;
	LogOutput::LogWritter	_writter;

public:
	static Ref sInstance;
};
#else
class ASyncLogWritter : public core::Service::ITask
{
public:
	typedef Reference<ASyncLogWritter> Ref;

public:
	ASyncLogWritter(LogOutput::LogWritter aWritter) : _writter(aWritter)
	{
		sInstance = this;
		LogOutput::SetLogWritter(AsyncLogWritter);
	}

	bool Execute() override
	{
		std::string msg;
		while (true) {
			if (true) {
				std::unique_lock<std::mutex> lock(_cs);
				if (_queue.size()) {
					msg = _queue.front();
					_queue.pop_front();
				}
				else {
					break;
				}
			}
			if (!msg.empty()) {
				_writter(msg);
			}
		}
		return true;
	}

	core::Service::Handle GetHandle() override
	{
		return _notifier.getReadFd();
	}

	void Add(const std::string &aMsg)
	{
		if (true) {
			std::unique_lock<std::mutex> lock(_cs);
			_queue.push_back(aMsg);
		}
		_notifier.notify();
	}

	static void AsyncLogWritter(const std::string &aMsg)
	{
		sInstance->Add(aMsg);
	}

protected:
	std::mutex				_cs;
	core::Service::Notifier	_notifier;
	std::list<std::string>	_queue;
	LogOutput::LogWritter	_writter;

public:
	static Ref sInstance;
};
#endif

ASyncLogWritter::Ref ASyncLogWritter::sInstance;

bool EquihashFarm::Init(int argc, char **argv)
{
	if (!Farm::Init(argc, argv)) {
		return false;
	}

	std::string server;
	std::string port;
	std::string user;
	std::string worker;
	std::string password;
	std::string key;
	bool poolMode = false;

	for (CommandLineReader cmd(argc, argv); cmd.hasToken(); cmd.pop()) {
		if (cmd.isLongOption()) {
			if (cmd.getName() == "--server") {
				if (cmd.hashValue()) {
					cmd.getValue().toString(server);
				}
				else if (cmd.pop()) {
					server = cmd.getRaw();
				}
				continue;
			}
			if (cmd.getName() == "--port") {
				if (cmd.hashValue()) {
					cmd.getValue().toString(port);
				}
				else if (cmd.pop()) {
					port = cmd.getRaw();
				}
				continue;
			}
			if (cmd.getName() == "--pool") {
				std::string pool;
				if (cmd.hashValue()) {
					cmd.getValue().toString(pool);
				}
				else if (cmd.pop()) {
					pool = cmd.getRaw();
				}
				if (!pool.empty()) {
					if (const char *cp = strchr(pool.c_str(), ':')) {
						port = cp + 1;
						server.assign(pool.c_str(), cp - pool.c_str());
					}
					poolMode = true;
				}
				continue;
			}
			if (cmd.getName() == "--user") {
				if (cmd.hashValue()) {
					cmd.getValue().toString(user);
				}
				else if (cmd.pop()) {
					user = cmd.getRaw();
				}
				if (!user.empty()) {
					if (const char *cp = strchr(user.c_str(), '.')) {
						int length = (int)(cp - user.c_str());
						worker = cp + 1;
						user.resize(length);
					}
				}
				continue;
			}
			if (cmd.getName() == "--worker") {
				if (cmd.hashValue()) {
					cmd.getValue().toString(worker);
				}
				else if (cmd.pop()) {
					worker = cmd.getRaw();
				}
				continue;
			}
			if (cmd.getName() == "--password") {
				if (cmd.hashValue()) {
					cmd.getValue().toString(password);
				}
				else if (cmd.pop()) {
					password = cmd.getRaw();
				}
				continue;
			}
			if (cmd.getName() == "--key") {
				if (cmd.hashValue()) {
					cmd.getValue().toString(key);
				}
				else if (cmd.pop()) {
					key = cmd.getRaw();
				}
				continue;
			}
			if (cmd.getName() == "--log") {
				std::string allow;
				if (cmd.hashValue()) {
					cmd.getValue().toString(allow);
				}
				else if (cmd.pop()) {
					allow = cmd.getRaw();
				}
				if (allow.empty()) {
					LogOutput::Set(0);
				}
				else {
					for (char a : allow) {
						switch (a) {
						case 'i':
							LogOutput::Allow(LogLevel::Info);
							break;
						case 'd':
							LogOutput::Allow(LogLevel::Debug);
							break;
						case 'e':
							LogOutput::Allow(LogLevel::Error);
							break;
						case 'f':
							LogOutput::Allow(LogLevel::Fatal);
							break;
						case 't':
							LogOutput::Allow(LogLevel::Trace);
							break;
						case 'w':
							LogOutput::Allow(LogLevel::Warning);
							break;
						}
					}
				}
				continue;
			}
			if (cmd.getName() == "--no-shares-timeout") {
				std::string value;
				if (cmd.hashValue()) {
					cmd.getValue().toString(value);
				}
				else if (cmd.pop()) {
					value = cmd.getRaw();
				}
				if (!value.empty()) {
					_checkNoSharesTimeout = std::stoul(value);
				}
				continue;
			}
#if USE_CUDA
			if (cmd.getName() == "--gpu-id-is-cuda-id") {
				core::CudaMiner::sIdIsCudaId = true;
				continue;
			}
			if (cmd.getName() == "--use-nvidia") {
				if (cmd.hashValue()) {
					std::string value;
					cmd.getValue().toString(value);
					if (value == "false" || value == "no" || value == "off") {
						_useNvidia = false;
					}
					else {
						_useNvidia = true;
					}
				}
				else {
					_useNvidia = true;
				}
				continue;
			}
#endif
#if USE_OPENCL
			if (cmd.getName() == "--gpu-id-is-cl-id") {
				core::ClMiner::sIdIsClId = true;
				continue;
			}
			if (cmd.getName() == "--use-amd") {
				if (cmd.hashValue()) {
					std::string value;
					cmd.getValue().toString(value);
					if (value == "false" || value == "no" || value == "off") {
						_useAMD = false;
					}
					else {
						_useAMD = true;
					}
				}
				else {
					_useAMD = true;
				}
				continue;
			}
#endif
			if (cmd.getName() == "--gpu-id-is-pci-id") {
#if USE_CUDA
				core::CudaMiner::sIdIsCudaId = false;
#endif
#if USE_OPENCL
				core::ClMiner::sIdIsClId = false;
#endif
				continue;
			}
			if (cmd.getName() == "--enable-api") {
				std::string value;
				if (cmd.hashValue()) {
					cmd.getValue().toString(value);
				}
				if (value.empty()) {
					_api._port = 4080;
				}
				else {
					_api._port = (unsigned short)std::stoul(value);
				}
				continue;
			}
			if (cmd.getName() == "--version") {
				PrintVersion();
				return false;
			}
			if (cmd.getName() == "--list-gpu") {
#if USE_CUDA
				core::CudaMiner::CudaDevice::List cudaDevices;
				if (core::CudaMiner::GetDevices(cudaDevices) && !cudaDevices.empty()) {
					printf("Found %d CUDA devices:\n", (int)cudaDevices.size());
					for (auto device : cudaDevices) {
						printf("\tID: %d, bus ID: %d, device ID: %d, name: %s\n", device->_cudaDeviceId, device->_pciBusId, device->_pciDeviceId, device->_name.c_str());
					}
				}
#endif
#if USE_OPENCL
				core::ClMiner::ClDevice::List clDevices;
				if (core::ClMiner::GetDevices(clDevices) && !clDevices.empty()) {
					printf("Found %d AMD OpenCL devices:\n", (int)clDevices.size());
					for (auto device : clDevices) {
						printf("\tID: %d, bus ID: %d, device ID: %d, name: %s\n", device->_id, device->_pciBusId, device->_pciDeviceId, device->_name.c_str());
					}
				}
#endif
				return false;
			}
		}
		LOG(Info) << "Unknown option: " << cmd.getRaw();
	}

	PrintVersion();

	if (!_api.Start()) {
		LOG(Info) << "No API started";
	}

	_worker = new EquihashStratumWorker(poolMode, key);
	if (!_worker) {
		LOG(Fatal) << "Create worker error.";
		return false;
	}

	if (server.empty()) {
		LOG(Fatal) << "No pool server address.";
		return false;
	}
	else {
		_worker->SetServer(server);
	}

	if (port.empty()) {
		LOG(Fatal) << "No pool server port.";
		return false;
	}
	else {
		_worker->SetPort(port);
	}

	if (poolMode) {
		if (user.empty()) {
			LOG(Fatal) << "No pool user.";
			return false;
		}
		else {
			_worker->SetUser(user);
		}

		if (worker.empty()) {
			LOG(Fatal) << "No pool worker.";
			return false;
		}
		else {
			_worker->SetWorker(worker);
		}
	}
	if (!password.empty()) {
		_worker->SetPassword(password);
	}
#if USE_CUDA
	if (_useNvidia) {
		core::CudaMiner::CudaDevice::List cudaDevices;
		if (core::CudaMiner::GetDevices(cudaDevices) && !cudaDevices.empty()) {
			for (auto device : cudaDevices) {
				if (EquihashCudaMiner::Ref miner = new EquihashCudaMiner(*_worker, *device)) {
					if (miner->Start()) {
						_miners.push_back(*miner);
					}
				}
			}
		}
	}
#endif
#if USE_OPENCL
	if (_useAMD) {
		core::ClMiner::ClDevice::List clDevices;
		if (core::ClMiner::GetDevices(clDevices) && !clDevices.empty()) {
			for (auto device : clDevices) {
				if (EquihashClMiner::Ref miner = new EquihashClMiner(*_worker, *device)) {
					if (miner->Start()) {
						_miners.push_back(*miner);
					}
				}
			}
		}
	}
#endif
	core::Service::AddTask(new PrintStatisticsTask(*this));
	core::Service::AddTask(new ASyncLogWritter(LogOutput::GetLogWritter()));

	if (!_worker->Start()) {
		LOG(Fatal) << "Start worker error.";
		return false;
	}

	_started = time(nullptr);

	return true;
}
#if 0
void EquihashFarm::Run()
{
	WCHAR  ch = 0;
	DWORD  count;
	HANDLE hstdin = GetStdHandle(STD_INPUT_HANDLE);

	while (ch != 27) {
		WaitForSingleObject(hstdin, INFINITE);
		ReadConsole(hstdin, &ch, 1, &count, NULL);
	}
}
#endif
void EquihashFarm::PrintStatistics() const
{
	core::Metrics metrics;
	core::Metrics total;
	core::HardwareMetrics hardwareMetrics;
	total.Clear();
	for (auto miner : _miners) {
		miner->GetMetrics(metrics);
		miner->GetHardwareMetrics(hardwareMetrics);
		LOG(Info)
			<< "GPU:"   << metrics.name
			<< ": A:"   << metrics.accepted << "/R:" << metrics.rejected
			<< ", t:"   << (unsigned)hardwareMetrics.t
			<< "C P:"   << (unsigned)hardwareMetrics.P
			<< "W fan:" << (unsigned)hardwareMetrics.fan
			<< "% "    << (unsigned)metrics.solutionRateNow << "/" << (unsigned)metrics.solutionRateTotal << " Sol/s";
		total += metrics;
	}
	if (_miners.size() > 1) {
		LOG(Info)
			<< "Total: A:" << total.accepted << "/R:" << total.rejected
			<< " " << (unsigned)total.solutionRateNow << "/" << (unsigned)total.solutionRateTotal << " Sol/s ("
			<< (unsigned)total.hashRateNow << "/" << (unsigned)total.hashRateTotal << " H/s)";
	}
}

void EquihashFarm::BuildStatistics(std::string &aMsg)
{
	/*
	{
	"hs_units": "khs", //Optional: units that are uses for hashes array, "hs", "khs", "mhs", ... Default "khs".
	"hs": [123, 223.3], //array of hashes
	"temp": [60, 63], //array of miner temps
	"fan": [80, 100], //array of miner fans
	"bus_numbers": [0, 1, 12, 13], //Pci buses array in decimal format. E.g. 0a:00.0 is 10
	"ar": [123, 3], //Optional: acceped, rejected shares
	"uptime": 12313232, //seconds elapsed from miner stats
	"ver": "1.2.3.4-beta", //miner version currently run, parsed from it's api or manifest
	"algo": "customalgo" //Optional: algo used by miner, should one of the exiting in Hive
	}
	*/
	std::string hs;
	std::string temp;
	std::string fan;
	std::string bus_numbers;

	core::Metrics metrics;
	core::Metrics total;
	core::HardwareMetrics hardwareMetrics;

	total.Clear();
	for (auto miner : _miners) {
		miner->GetAvgMetrics(metrics);
		miner->GetHardwareMetrics(hardwareMetrics);

		if (!hs.empty()) {
			hs += ',';
			temp += ',';
			fan += ',';
			bus_numbers += ',';
		}
		hs += std::to_string(metrics.hashRateNow / 1000);
		temp += std::to_string((unsigned)hardwareMetrics.t);
		fan += std::to_string((unsigned)hardwareMetrics.fan);
		bus_numbers += std::to_string((unsigned)miner->GetPciBusId());

		total += metrics;
	}

	aMsg += "{";
#if 0
	aMsg += "\"hs_units\":\"khs\",";
#endif

	aMsg += "\"hs\":[";
	aMsg += hs;
	aMsg += "],";
	aMsg += "\"temp\":[";
	aMsg += temp;
	aMsg += "],";
	aMsg += "\"fan\":[";
	aMsg += fan;
	aMsg += "],";
	aMsg += "\"bus_numbers\":[";
	aMsg += bus_numbers;
	aMsg += "],";

	aMsg += "\"ar\":[";
	aMsg += std::to_string(total.accepted);
	aMsg += ", ";
	aMsg += std::to_string(total.rejected);
	aMsg += "],";
	aMsg += "\"uptime\":";
	aMsg += std::to_string(time(nullptr) - _started);
	aMsg += ",";
	aMsg += "\"ver\":\"" BEAM_EQUIHASH_VERSION "\",";
	aMsg += "\"algo\":\"ethash\",";
	aMsg += "\"hs_total\":";
	aMsg += std::to_string(total.hashRateNow / 1000);
	aMsg += "}";
}

void EquihashFarm::Cancel()
{
	if (_worker) {
		_worker->Stop();
	}
}
