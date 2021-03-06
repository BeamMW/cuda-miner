#include "pch.hpp"
#include "base/Logging.hpp"
#include "base/WaitableTimer.hpp"
#include "base/Semaphore.hpp"
#include "base/CommandLineReader.hpp"
#include "base/StringUtils.hpp"
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

#define BEAM_EQUIHASH_VERSION "1.0.0.82"

static void PrintVersion()
{
	printf("beam-cuda-miner version " BEAM_EQUIHASH_VERSION "\n");
}

class PrintStatisticsTask : public core::Service::ITask
{
public:
	PrintStatisticsTask(const EquihashFarm &aFarm) : _farm(aFarm)
	{
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
	std::string key;
	std::string allowedDevices;

	for (CommandLineReader cmd(argc, argv); cmd.hasToken(); cmd.pop()) {
		if (cmd.isLongOption()) {
			if (cmd.getName() == "--server") {
				std::string value;
				if (cmd.hashValue()) {
					cmd.getValue().toString(value);
				}
				else if (cmd.pop()) {
					value = cmd.getRaw();
				}
				if (!value.empty()) {
					if (const char *cp = strchr(value.c_str(), ':')) {
						port = cp + 1;
						server.assign(value.c_str(), cp - value.c_str());
					}
					else {
						server = value;
					}
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
			if (cmd.getName() == "--devices") {
				if (cmd.hashValue()) {
					cmd.getValue().toString(allowedDevices);
				}
				else if (cmd.pop()) {
					allowedDevices = cmd.getRaw();
				}
				continue;
			}
			if (cmd.getName() == "--list-devices") {
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

	_worker = new EquihashStratumWorker(key);
	if (!_worker) {
		LOG(Fatal) << "Create worker error.";
		return false;
	}

	if (server.empty()) {
		LOG(Fatal) << "No server address.";
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
#if USE_CUDA
	if (_useNvidia) {
		core::CudaMiner::CudaDevice::List cudaDevices;
		if (core::CudaMiner::GetDevices(cudaDevices) && !cudaDevices.empty()) {
			if (!allowedDevices.empty()) {
				core::CudaMiner::CudaDevice::List devices;
				std::vector<std::string> devicesId;
				tokenizeString(allowedDevices, ',', devicesId);
				for (const auto &id : devicesId) {
					bool unique = true;
					if (std::string::npos != id.find(':')) {
						unsigned busId;
						unsigned deviceId;
						if (2 == std::sscanf(id.c_str(), "%u:%u", &busId, &deviceId)) {
							for (auto device : devices) {
								if (device->_pciBusId == busId && device->_pciDeviceId == deviceId) {
									unique = false;
									break;
								}
							}
							if (unique) {
								for (auto device : cudaDevices) {
									if (device->_pciBusId == busId && device->_pciDeviceId == deviceId) {
										devices.push_back(device);
										break;
									}
								}
							}
						}
					}
					else {
						unsigned cudaId = std::stoul(id);
						for (auto device : devices) {
							if (device->_cudaDeviceId == cudaId) {
								unique = false;
								break;
							}
						}
						if (unique) {
							for (auto device : cudaDevices) {
								if (device->_cudaDeviceId == cudaId) {
									devices.push_back(device);
									break;
								}
							}
						}
					}
				}
				cudaDevices = devices;
			}
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
	if (0 == _miners.size()) {
		LOG(Fatal) << "There are no devices available to start mining.";
		return false;
	}

	core::Service::AddTask(new PrintStatisticsTask(*this));
	core::Service::AddTask(new ASyncLogWritter(LogOutput::GetLogWritter()));

	if (!_worker->Start()) {
		LOG(Fatal) << "Start worker error.";
		return false;
	}

	_started = time(nullptr);

	return true;
}

void EquihashFarm::PrintStatistics() const
{
	if (_worker) {
		if (0 == _worker->GetConnectedCount()) {
			LOG(Info) << "Waiting for server connection ...";
			return;
		}
		if (0 == _worker->GetWorkCount()) {
			LOG(Info) << "Waiting a job from the server ...";
			return;
		}
		core::Metrics metrics;
		core::Metrics total;
		core::HardwareMetrics hardwareMetrics;
		total.Clear();
		for (auto miner : _miners) {
			miner->GetMetrics(metrics);
			miner->GetHardwareMetrics(hardwareMetrics);
			LOG(Info)
				<< "GPU:" << metrics.name
				<< ": A:" << metrics.accepted << "/R:" << metrics.rejected
				<< ", t:" << (unsigned)hardwareMetrics.t
				<< "C P:" << (unsigned)hardwareMetrics.P
				<< "W fan:" << (unsigned)hardwareMetrics.fan
				<< "% " << std::setprecision(3) << float(metrics.solutionRateNow) << "/" << std::setprecision(3) << float(metrics.solutionRateTotal) << " Sol/s";
			total += metrics;
		}
		if (_miners.size() > 1) {
			LOG(Info)
				<< "Total: A:" << total.accepted << "/R:" << total.rejected
				<< " " << std::setprecision(3) << float(total.solutionRateNow) << "/" << std::setprecision(3) << float(total.solutionRateTotal) << " Sol/s ("
				<< std::setprecision(3) << float(total.hashRateNow) << "/" << std::setprecision(3) << float(total.hashRateTotal) << " H/s)";
		}
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
	aMsg += "\"algo\":\"equihash\",";
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
