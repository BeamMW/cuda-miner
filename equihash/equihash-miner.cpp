#include "pch.hpp"
#include "EquihashFarm.hpp"
#include "base/Logging.hpp"

#define	DO_TEST 0

#if DO_TEST
#include "base/Endian.hpp"
#include "base/SHA256.hpp"
#include "base/Convertors.hpp"
#include "core/NullWorker.hpp"
#include "cl/EquihashClMiner.hpp"
#include "cuda/EquihashCudaMiner.hpp"
#include "EquihashWork.hpp"
#include "EquihashSolution.hpp"
/*
Input[] = { 1, 2, 3, 4, 56 };
diff=0
Nonce = 0x010204U;

Solution:
2999695
56772901
48407952
64443870
27324866
67050250
29939296
36064312
15721703
39017469
52234690
54368082
41964818
55336114
43556682
66087966
3167761
22207365
7946349
53657833
24078949
41235495
28344398
37831284
7202703
64504755
31925799
43423584
12206181
22900945
36185780
61421076

header.version = 4;
header.nonce[1] = 0x010204U;

Solution:
6336033
54308675
24000659
29140854
17327302
62533223
31841111
54642241
26953655
56320882
52836086
58677453
45058748
67054078
48867695
51981626
13903980
34227861
42704167
62722972
17031240
17673376
56455181
62223170
18473017
45678982
19758901
39804308
36818245
45642469
42588845
56238689
*/
void doTest()
{
#if USE_CUDA
	class TestSolverListener : public Solver::Listener
	{
	public:
		TestSolverListener(EquihashSolution::Ref aSolution) : _solution(aSolution) {}
		bool IsCancel(const core::Work &aWork) override { return false; }
		void OnSolution(const EquihashWork &aWork, const std::vector<uint32_t> &aIndexVector, size_t aCbitlen) override
		{
			if (_solution && _solution->CheckSolution(aIndexVector, aCbitlen)) {
				LOG(Info) << "CheckSolution OK";
				return;
			}
			else {
				return;
			}
		}
		void OnHashDone() override {}

	protected:
		EquihashSolution::Ref	_solution;
	};

	EquihashSolution::Ref solution = new EquihashSolution(
		"",
		4,
#if 1
		"6b217cd27dc4e18358013469c314dfe2664355ce604e34cdbe81525f00000000",
		"1993e160b819d4d1c6133a0c4775b2ecdceb612b3360929e5742690fc3cf4b2e",
		1490799679,
//		"1d0084e1",
		"e184001d",
		"0216b08c9b091b01000000000000000000000000000000000000000000000000",
		"0003e500f8994c11949b902aa84f66bd2e22b2e8aa453eb7c47a12b3aba9b7a54235785161e22a78e4df1875135692d1289b5f0e22e07b3474014242d837ca34112724017c33dbfec7b68414587259da4f381e3d0a55230d54d87a654b3133819ffdf99e439e9558da58ef337c52a2f5f96f5f05b4c35f5f9e60b4b974c31a4d49831dd2a57f1ea505fea546b246606e1a29862692063f1dd09572c441e3f218ff5fc94a0f78f0dd0637736b2ddc73c3e589c1641c8c4d1946c75ae12017739b8c0baa396f946d545a466b38214b664ecd53069977f8421ae3efe926e305c1b609db8d693d9fbb2202db700aa7201f86d199c7e65033ab8ac31fdef32b4984cd12e42065ffae664086bbb1ced8ae5a244d2f3476bf46a3531fefb06436075974fd10feea38b84cc6fcf31b6e6cf5e7110675307492fe9b425959f655ab6f8475de7d3533a3f9a20ce531027ad2960eb202e67c6f24c1882a6f4cf09c81b8b09b254f3a173e1bff8a1965a3a9e7260422c88750002cb90fad84ed3606250b4a6b1cbb769733c7fc2bc62a88145646d54b619476dfe0d8a145ab4501f9f422036f7c9cc31c0342aedc410c2bb4a09fa6138b67d47a742774e4420f6f038ef6adacc99c61f51e63d957b20aa17b72530cb5c6f316535247c5cba578003e3feeabb4fb427b20663a41db2f1b1fb775b3c4b554b9ca76217927fe0842cd99039bb72d4bd4a919b5f3adb719c81ffda017854424c9c6acbca23ef630f95c49eea1cfdba1c90bd830733c76e249d6b6164dace7c8627d5a7cd7ba13f7f1c48e92c0d70425e24b007964f6f04fd8204e0956e75f26d9de0d1af3c27b45554bb639f49595ee16dada5e4a65c5f1ef0c318450405855436b9b7ef715db27973d06caf070f0965f6e551acf20375a1f4f4f875e99675abd7d9b4d595775f4ecd307057b77eb016d39355d896b7b9881566f6dffd2c34ef6dcd37205520fd35291d758e13390a3d3c754061dde577c200d3f20c6ef0a64afd7e4c8ccbe565f2a60be18fe9d1d52cc96304a2df31bdc0221babe6282eebad7e3c0038a68cfb0cf7eeca575125ee8b7f0b8d9750960cb0f2d5e9961d5125fd610946e173dcd4a2f4d96d4d711c4fa085d4b40dbd895f3c868238989cd0c923f7626d8df06f6cd9a1db31033d200bc6f0e75049d744d0335828f83cb6318f56395a3645a1a7570653b1e0821d113cc1f4eb7beba0037a1196f646ac0cfdf7e4607768491bc9f5657db29734281aab8558a17eea2670b34a15e464f31d6bb1061c3bdd764a1087468e27e1dd63ec28f87fa0a6015694369f9ac92d6b19d448d2023af36653349abf62134329e7e5a5df51df184782146712438abc8f3c207b4930fbb333538649f731c3be52a96b01e3c4732a9756861663662585a7f4209018a83a290d384bfdcc682ef1a250331f54fb415a628a92966732b13ebf5f883e243328cca4aac1f2789069ed9899883705cb24ae49da7686c3951655f7c0a10f53e7a530bc42cb0b49133f64b2ae5a94f75782904d68545f2047adee17460d43e25ff90997814d5a606f30937edc645deb4ffe52ad16ba7a17472d9dcc6114fa61ae0a20c1be9521491b5f91d2b378f9f2d1733ec3591f3f0ec979fa233cb24cf2e4756b2dd33a601c79e4baab14c9bff8ee12804d0152c81da2bcd0d10026696e1dd0bd3ad6284170f38a13dc87f753db30b5ccddda9e59491e955b5ba344aa6ea81eef803a123495fdb9126ddcf9aee2235f8fcfb35ed7452670c025fe90b9c9246a2c3d4a1e6fa467bc4c04538cda249ebbd5c59a74d2f4bd2f8206262dfa325c11fd4b20b5dcaddc085e7e2f448714c9a217fd109488b9a1e28a07abdf9106bf74776187b916fec7625b41d76af"
#endif
	);
	BigInteger hash = solution->GetHash();
	solution->ProofOfWork();

	EquihashWork::Ref work = new EquihashWork(
		4,
		"6b217cd27dc4e18358013469c314dfe2664355ce604e34cdbe81525f00000000",
		"1993e160b819d4d1c6133a0c4775b2ecdceb612b3360929e5742690fc3cf4b2e",
		1490799679,
		"e184001d",
		"0216b08c9b091b01000000000000000000000000000000000000000000000000"
	);

	if (core::NullWorker::Ref worker = new core::NullWorker()) {
#if 1
		core::CudaMiner::CudaDevice::List devices;
		if (core::CudaMiner::GetDevices(devices) && !devices.empty()) {
#if 0
			if (EquihashCudaMiner::Ref miner = new EquihashCudaMiner(*worker, devices.front())) {
				miner->Init();
				miner->Search(*work, TestSolverListener(solution));
				miner->Search(*work, TestSolverListener(solution));
				miner->Search(*work, TestSolverListener(solution));
				miner->Search(*work, TestSolverListener(solution));
				miner->Search(*work, TestSolverListener(solution));
				miner->Done();
			}
#else
			if (Reference<CudaSolver> solver = new CudaSolver("0", devices.front()->_cudaDeviceId)) {
#if 0
				solver->Test(*work);
				solver->Test(*work);
				solver->Test(*work);
				solver->Test(*work);
				solver->Test(*work);
#else
				try {
					blake2b_state state;

					uint32_t le_N = 150;
					uint32_t le_K = 5;

					unsigned char personalization[BLAKE2B_PERSONALBYTES] = {};
					memcpy(personalization, "Beam-PoW", 8);
					memcpy(personalization + 8, &le_N, 4);
					memcpy(personalization + 12, &le_K, 4);

					const uint8_t outlen = (512 / 150) * ((150+7)/8);

					static_assert(!((!outlen) || (outlen > BLAKE2B_OUTBYTES)), "!((!outlen) || (outlen > BLAKE2B_OUTBYTES))");

					blake2b_param param = { 0 };
					param.digest_length = outlen;
					param.fanout = 1;
					param.depth = 1;

					memcpy(&param.personal, personalization, BLAKE2B_PERSONALBYTES);

					blake2b_init_param(&state, &param);
#if 1
					uint8_t input[] = { 1, 2, 3, 4, 56 };
					uint8_t nonce[] = { 0, 0, 0, 0, 0, 0x01, 0x02, 0x04 };

					blake2b_update(&state, input, sizeof(input));
					blake2b_update(&state, nonce, sizeof(nonce));
#else
					struct ZcachHeader
					{
						uint32_t	version;
						uint32_t	prevhash[8];
						uint32_t	merkle_root[8];
						uint32_t	reserve[8];
						uint32_t	time;
						uint32_t	nbits;
						uint32_t	nonce[8];

						ZcachHeader() {
							static_assert(140 == sizeof(*this), "Wrong header size");
							memset(this, 0, sizeof(*this));
						}
					};

					ZcachHeader header;
					header.version = 4;
					header.nonce[1] = 0x010204U;

					blake2b_update(&state, (uint8_t*)&header, sizeof(ZcachHeader));
#endif
					solver->Test(state, TestSolverListener(EquihashSolution::Ref()));
					solver->Test(state, TestSolverListener(EquihashSolution::Ref()));
					solver->Test(state, TestSolverListener(EquihashSolution::Ref()));
					solver->Test(state, TestSolverListener(EquihashSolution::Ref()));
					solver->Test(state, TestSolverListener(EquihashSolution::Ref()));
				}
				catch (const std::exception &e) {
					LOG(Error) << "Error into Solver: " << e.what();
				}
#endif
			}
#endif
		}
#else
		core::ClMiner::ClDevice::List devices;
		if (core::ClMiner::GetDevices(devices) && !devices.empty()) {
			if (EquihashClMiner::Ref miner = new EquihashClMiner(*worker, devices.front())) {
				miner->Init();
				miner->Search(*work, TestSolverListener(solution));
				miner->Done();
			}
		}
#endif
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
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

	if (farm.Init(argc, argv)) {
		farm.Run();
	}
	farm.Done();

	return 0;
}

