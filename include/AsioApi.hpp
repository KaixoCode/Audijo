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
			for (int i = 0; i < drivers.asioGetNumDev(); i++)
			{
				char name[50];
				drivers.asioGetDriverName(i, name, 50);
				std::vector<double> srates;
				drivers.asioOpenDriver(i, (void**)&theAsioDriver);
				for (auto& srate : m_SampleRates)
					if (theAsioDriver->canSampleRate(srate)) srates.push_back(srate);
				long in, out;
				theAsioDriver->getChannels(&in, &out);
				m_Devices.push_back(DeviceInfo{ i, name, in, out, srates });
				drivers.removeCurrentDriver();
			}
			return m_Devices;
		}

		void OpenStream(const StreamSettings& settings = StreamSettings{}) override
		{
			// TODO: 

			int inid = settings.input.deviceId;
			int outid = settings.output.deviceId;
			drivers.asioOpenDriver(inid, (void**)&theAsioDriver);
			ASIOBufferInfo info;
			info.isInput = true;
			info.channelNum = inid;
			ASIOCallbacks cbacks;
			auto error = theAsioDriver->createBuffers(&info, settings.input.channels, settings.bufferSize, &cbacks);
			if (error != ASE_OK)
			{
				LOGL(getAsioErrorString(error));
				LOGL("No");
				return;
			}
			theAsioDriver->init(GetForegroundWindow());
			theAsioDriver->start();
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