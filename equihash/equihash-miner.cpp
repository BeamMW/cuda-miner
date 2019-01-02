#include "pch.hpp"
#include "EquihashFarm.hpp"
#include "base/Logging.hpp"

#define	DO_TEST 0

#if DO_TEST
#include "base/Endian.hpp"
#include "base/SHA256.hpp"
#include "base/Convertors.hpp"
#include "core/NullWorker.hpp"
#include "cuda/EquihashCudaMiner.hpp"
#include "BeamWork.hpp"
/*
*/
void doTest()
{
#if USE_CUDA
	class TestSolverListener : public Solver::Listener
	{
	public:
		TestSolverListener() {}
		bool IsCancel(const core::Work &aWork) override { return false; }
		void OnSolution(const BeamWork &aWork, const std::vector<uint32_t> &aIndexVector, size_t aCbitlen) override
		{
		}
		void OnHashDone() override {}
	};

	BeamWork::Ref work = new BeamWork(
		"6b217cd27dc4e18358013469c314dfe2664355ce604e34cdbe81525f00000000",
		"0216b08c9b091b01"
	);

	if (core::NullWorker::Ref worker = new core::NullWorker()) {
		core::CudaMiner::CudaDevice::List devices;
		if (core::CudaMiner::GetDevices(devices) && !devices.empty()) {
			if (Reference<CudaSolver> solver = new CudaSolver("0", devices.front()->_cudaDeviceId)) {
				try {
					solver->Solve(work, TestSolverListener());
				}
				catch (const std::exception &e) {
					LOG(Error) << "Error into Solver: " << e.what();
				}
			}
		}
	}
#endif
}
#endif

EquihashFarm farm;

#ifdef _WIN32
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
		// Handle the CTRL-C signal. 
	case CTRL_C_EVENT:
		printf("Ctrl-C event\n\n");
		farm.Cancel();
		return TRUE;

		// CTRL-CLOSE: confirm that the user wants to exit. 
	case CTRL_CLOSE_EVENT:
//		Beep(600, 200);
		printf("Ctrl-Close event\n\n");
		return TRUE;

		// Pass other signals to the next handler. 
	case CTRL_BREAK_EVENT:
//		Beep(900, 200);
		printf("Ctrl-Break event\n\n");
		return FALSE;

	case CTRL_LOGOFF_EVENT:
//		Beep(1000, 200);
		printf("Ctrl-Logoff event\n\n");
		return FALSE;

	case CTRL_SHUTDOWN_EVENT:
//		Beep(750, 500);
		printf("Ctrl-Shutdown event\n\n");
		return FALSE;

	default:
		return FALSE;
	}
}
#else
#include <signal.h>
void signal_handler(int sig)
{
	printf("Ctrl-C event\n\n");
//	farm.Cancel();
	exit(EXIT_FAILURE);
}
#endif

int main(int argc, char **argv)
{
#if DO_TEST
	doTest();
	return 0;
#endif
#ifdef _WIN32
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#else
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
#endif
	if (farm.Init(argc, argv)) {
		farm.Run();
	}
	farm.Done();

	return 0;
}

