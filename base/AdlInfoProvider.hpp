/*
* Wrapper for ADL, inspired by wrapnvml from John E. Stone
* 
* By Philipp Andreas - github@smurfy.de
*/
#pragma	once

#if USE_OPENCL
#include "InfoProvider.hpp"
#include "ADL/adl_sdk.h"

typedef enum wrap_adlReturn_enum {
	WRAPADL_OK= 0
} wrap_adlReturn_t;

#define ADL_API_CALL __stdcall

typedef void* (ADL_API_CALL *ADL_MAIN_MALLOC_CALLBACK)(int);

#define ADL_MAX_PATH	256

class AdlInfoProvider : public InfoProvider
{
public:
	typedef Reference<AdlInfoProvider> Ref;

	struct ADLAdapterInfo
	{
		/// Size of the structure.
		int iSize;
		/// The ADL index handle. One GPU may be associated with one or two index handles
		int iAdapterIndex;
		/// The unique device ID associated with this adapter.
		char strUDID[ADL_MAX_PATH];
		/// The BUS number associated with this adapter.
		int iBusNumber;
		/// The driver number associated with this adapter.
		int iDeviceNumber;
		/// The function number.
		int iFunctionNumber;
		/// The vendor ID associated with this adapter.
		int iVendorID;
		/// Adapter name.
		char strAdapterName[ADL_MAX_PATH];
		/// Display name. For example, "\\Display0" for Windows or ":0:0" for Linux.
		char strDisplayName[ADL_MAX_PATH];
		/// Present or not; 1 if present and 0 if not present.It the logical adapter is present, the display name such as \\.\Display1 can be found from OS
		int iPresent;
		// @}

		/// Exist or not; 1 is exist and 0 is not present.
		int iExist;
		/// Driver registry path.
		char strDriverPath[ADL_MAX_PATH];
		/// Driver registry path Ext for.
		char strDriverPathExt[ADL_MAX_PATH];
		/// PNP string from Windows.
		char strPNPString[ADL_MAX_PATH];
		/// It is generated from EnumDisplayDevices.
		int iOSDisplayIndex;
		// @}
	};

	struct ADLTemperature
	{
		/// Must be set to the size of the structure
		int iSize;
		/// Temperature in millidegrees Celsius.
		int iTemperature;
	};

	struct ADLFanSpeedValue
	{
		/// Must be set to the size of the structure
		int iSize;
		/// Possible valies: \ref ADL_DL_FANCTRL_SPEED_TYPE_PERCENT or \ref ADL_DL_FANCTRL_SPEED_TYPE_RPM
		int iSpeedType;
		/// Fan speed value
		int iFanSpeed;
		/// The only flag for now is: \ref ADL_DL_FANCTRL_FLAG_USER_DEFINED_SPEED
		int iFlags;
	};

protected:
	void *adl_dll = nullptr;
	wrap_adlReturn_t(*adlMainControlCreate)(ADL_MAIN_MALLOC_CALLBACK, int) = nullptr;
	wrap_adlReturn_t(*adlAdapterNumberOfAdapters)(int *) = nullptr;
	wrap_adlReturn_t(*adlAdapterAdapterInfoGet)(ADLAdapterInfo*, int) = nullptr;
	wrap_adlReturn_t(*adlAdapterAdapterIdGet)(int, int*) = nullptr;
	wrap_adlReturn_t(*adlOverdrive5TemperatureGet)(int, int, ADLTemperature*) = nullptr;
	wrap_adlReturn_t(*adlOverdrive5FanSpeedGet)(int, int, ADLFanSpeedValue*) = nullptr;
	wrap_adlReturn_t(*adlMainControlRefresh)(void) = nullptr;
	wrap_adlReturn_t(*adlMainControlDestory)(void) = nullptr;

public:
	~AdlInfoProvider() override;

	bool getGpuName(int gpuindex, char *namebuf, int bufsize) override;

	bool getTempC(int gpuindex, unsigned int *tempC) override;

	bool getFanPcnt(int gpuindex, unsigned int *fanpcnt) override;

	bool getPowerUsage(int gpuindex, unsigned int *milliwatts) override;

	static Ref create();
};
#endif
