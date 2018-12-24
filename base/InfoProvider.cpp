#include "pch.hpp"
#include <stdio.h>
#include <stdlib.h>
#include "InfoProvider.hpp"

int InfoProvider::getGpuCount() const
{
	return (int)_gpus.size();
}

const InfoProvider::AdapterInfo & InfoProvider::getAdapterInfo(int aGpuIndex) const
{
	static AdapterInfo sEmptyInfo = { 0 };
	if (aGpuIndex >= 0 && aGpuIndex < getGpuCount()) {
		return _gpus[aGpuIndex];
	}
	return sEmptyInfo;
}
