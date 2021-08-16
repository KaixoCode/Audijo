#pragma once
#include "ApiBase.hpp"
#include "Audijo.hpp"
#include "pch.hpp"
#include "asiosys.h"
#include "asio.h"
#define interface struct
#include "iasiodrv.h"
#include "asiodrivers.h"
#include <cassert>

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
			// For an ASIO the input and output device need to be the same.
			assert(settings.input.deviceId == settings.output.deviceId);

			// Retrieve necessary settings;
			int _deviceId = settings.input.deviceId;
			int _nInChannels = settings.input.channels;
			int _nOutChannels = settings.output.channels;
			int _nChannels = _nInChannels + _nOutChannels;
			int _bufferSize = settings.bufferSize;
			
			// Open the driver
			drivers.asioOpenDriver(_deviceId, (void**)&theAsioDriver);

			// Init the ASIO
			driverInfo.asioVersion = 2;
			driverInfo.sysRef = GetForegroundWindow();
			auto error = ASIOInit(&driverInfo);
			if (error != ASE_OK)
			{
				LOGL(getAsioErrorString(error));
				LOGL("No");
				return;
			}

			// Create the buffer
			if (m_BufferInfos)
				delete m_BufferInfos;
			m_BufferInfos = new ASIOBufferInfo[_nChannels];
			for (int i = 0; i < _nInChannels; i++)
				m_BufferInfos[i].isInput = true, 
				m_BufferInfos[i].channelNum = i;
			for (int i = _nInChannels; i < _nChannels; i++)
				m_BufferInfos[i].isInput = false, 
				m_BufferInfos[i].channelNum = i - _nInChannels;
			error = ASIOCreateBuffers(m_BufferInfos, _nChannels, _bufferSize, &m_Callbacks);
			if (error != ASE_OK)
			{
				LOGL(getAsioErrorString(error));
				LOGL("No");
				return;
			}
		}

		void StartStream() override { ASIOStart(); };
		void StopStream() override { ASIOStop(); };
		void CloseStream() override {};

	protected:
		ASIOCallbacks m_Callbacks{ m_BufferSwitch, m_SampleRateDidChange, m_AsioMessage, m_BufferSwitchTimeInfo, };
		ASIOBufferInfo* m_BufferInfos = nullptr;

		static void m_BufferSwitch(long doubleBufferIndex, ASIOBool directProcess) 
		{
			
		};

		static void m_SampleRateDidChange(ASIOSampleRate sRate)
		{
		};

		static long m_AsioMessage(long selector, long value, void* message, double* opt)
		{ 
			return 0; 
		};

		static ASIOTime* m_BufferSwitchTimeInfo (ASIOTime* params, long doubleBufferIndex, ASIOBool directProcess)
		{
			return params;
		}
	};
}