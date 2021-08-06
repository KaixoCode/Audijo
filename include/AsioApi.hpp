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

static const char* getAsioErrorString(ASIOError result)
{
	struct Messages
	{
		ASIOError value;
		const char* message;
	};

	static const Messages m[] =
	{
	  {   ASE_NotPresent,    "Hardware input or output is not present or available." },
	  {   ASE_HWMalfunction,  "Hardware is malfunctioning." },
	  {   ASE_InvalidParameter, "Invalid input parameter." },
	  {   ASE_InvalidMode,      "Invalid mode." },
	  {   ASE_SPNotAdvancing,     "Sample position not advancing." },
	  {   ASE_NoClock,            "Sample clock or rate cannot be determined or is not present." },
	  {   ASE_NoMemory,           "Not enough memory to complete the request." }
	};

	for (unsigned int i = 0; i < sizeof(m) / sizeof(m[0]); ++i)
		if (m[i].value == result) return m[i].message;

	return "Unknown error.";
}

namespace Audijo
{
	class AsioApi : public ApiBase
	{
	public:
		const std::vector<DeviceInfo>& Devices() override
		{
			drivers.removeCurrentDriver();
			for (int i = 0; i < drivers.asioGetNumDev(); i++)
			{
				char name[50];
				drivers.asioGetDriverName(i, name, 50);
				
				// To get access to 'theAsioDriver' you must use
				// drivers.asioOpenDriver(i, (void**)&theAsioDriver);
				// TODO: get all the information here using 'theAsioDriver'
				// - all allowed sample rates of the device (look up often used samplerates and test those.
				// - all allowed buffer sizes (granularity means stepsize between buffersizes, you'll know when you need this information lol, if this is -1 it means it only supports a power of 2.)
				// - the amount of input and output channels.

				m_Devices.push_back({ i, name });
			}
			return m_Devices;
		}

		void OpenStream(const StreamSettings& settings = StreamSettings{}) override
		{
			// TODO: 

			// get device/driver id from settings

			// open the driver

			// initialize asio with theAsioDriver

			// setup all other settings

			// 
			
			
		}

		void StartStream() override {};
		void StopStream() override {};
		void CloseStream() override {};
	};
}