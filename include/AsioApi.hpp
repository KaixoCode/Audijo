#pragma once
#include "ApiBase.hpp"
#include "Audijo.hpp"
#include "pch.hpp"
#include "asiosys.h"
#include "asio.h"
#define interface struct
#include "iasiodrv.h"
#include "asiodrivers.h"

static AsioDrivers drivers;
static ASIODriverInfo driverInfo;
extern IASIO* theAsioDriver;

namespace Audijo
{
	class AsioApi : public ApiBase 
	{
	public:
		const std::vector<DeviceInfo>& GetDevices() 
		{
			drivers.removeCurrentDriver();
			for (int i = 0; i < drivers.asioGetNumDev(); i++)
			{
				char name[50];
				drivers.asioGetDriverName(i, name, 50);
				
				m_Devices.push_back({ i, name });
			}
		}
	};
}