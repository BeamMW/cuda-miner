#include "pch.hpp"
#include "EquihashFarm.hpp"
#include "base/Logging.hpp"

EquihashFarm farm;

#ifdef _WIN32
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
	// Handle the CTRL-C signal. 
	case CTRL_C_EVENT:
		printf("Ctrl-C event\n\n");
		ExitProcess(0);
		return TRUE;
	default:
		return FALSE;
	}
}
#else
#include <signal.h>
void signal_handler(int sig)
{
	printf("Ctrl-C event\n\n");
	exit(EXIT_FAILURE);
}
#endif

int main(int argc, char **argv)
{
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

