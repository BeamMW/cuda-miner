#pragma	once

#include "Dynamic.hpp"
#include "Reference.hpp"

class InfoProvider : public Dynamic
{
public:
	typedef Reference<InfoProvider> Ref;

	struct AdapterInfo
	{
		int iAdapterIndex;
		int iBusNumber;
		int iDeviceNumber;
		int iFunctionNumber;
		int iVendorID;
		union {
			void *hDevice;
			int id;
		} extra;
		char strAdapterName[128-5*sizeof(int)-sizeof(void*)];
	};

public:
	/*
	* Query the number of GPUs
	*/
	virtual int getGpuCount() const;
	/*
	* query the name of the GPU model
	*
	*/
	virtual bool getGpuName(int aGpuIndex, char *namebuf, int bufsize) = 0;

	/*
	* Query the current GPU temperature (Celsius)
	*/
	virtual bool getTempC(int aGpuIndex, unsigned int *tempC) = 0;

	/*
	* Query the current GPU fan speed (percent)
	*/
	virtual bool getFanPcnt(int aGpuIndex, unsigned int *fanpcnt) = 0;

	/*
	* Query the current GPU power usage in millwatts
	*
	* This feature is only available on recent GPU generations and may be
	* limited in some cases only to Tesla series GPUs.
	* If the query is run on an unsupported GPU, this routine will return -1.
	*/
	virtual bool getPowerUsage(int aGpuIndex, unsigned int *milliwatts) = 0;

	const AdapterInfo & getAdapterInfo(int aGpuIndex) const;

protected:
	std::vector<AdapterInfo>	_gpus;
};

