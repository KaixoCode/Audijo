#pragma once
#include "ApiBase.hpp"
#include "Audijo.hpp"
#include "pch.hpp"
#include "asiosys.h"
#include "asio.h"
#include "iasiodrv.h"
#include "asiodrivers.h"

static const char* getAsioErrorString(ASIOError result)
{
	struct Messages
	{
		ASIOError value;
		const char* message;
	};

	static const Messages m[] =
	{
		{ ASE_NotPresent, "Hardware input or output is not present or available." },
		{ ASE_HWMalfunction, "Hardware is malfunctioning." },
		{ ASE_InvalidParameter, "Invalid input parameter." },
		{ ASE_InvalidMode, "Invalid mode." },
		{ ASE_SPNotAdvancing, "Sample position not advancing." },
		{ ASE_NoClock, "Sample clock or rate cannot be determined or is not present." },
		{ ASE_NoMemory, "Not enough memory to complete the request." }
	};

	for (unsigned int i = 0; i < sizeof(m) / sizeof(m[0]); ++i)
		if (m[i].value == result) return m[i].message;

	return "Unknown error.";
}

static AsioDrivers drivers;
static ASIODriverInfo driverInfo;
extern IASIO* theAsioDriver;

namespace Audijo
{
	class AsioApi : public ApiBase
	{
		enum State { Loaded, Initialized, Prepared, Running };
	public:
		const std::vector<DeviceInfo>& Devices() override
		{
			m_Devices.clear();
			for (int i = 0; i < drivers.asioGetNumDev(); i++)
			{
				// Get device name
				char name[32];
				auto error = drivers.asioGetDriverName(i, name, 32);
				if (error != ASE_OK)
				{
					LOGL("Failed to load device " << name << " (" << getAsioErrorString(error) << ")");
					continue;
				}

				// Load the driver to collect further information
				if (!drivers.loadDriver(name)) 
				{
					LOGL("Failed to load device: " << name);
					continue;
				}

				// Init the asio
				error = ASIOInit(&driverInfo);
				if (error != ASE_OK) 
				{
					LOGL("Failed to load device " << name << " (" << getAsioErrorString(error) << ")");
					continue;
				}

				// Get channel counts
				long in, out;
				error = ASIOGetChannels(&in, &out);
				if (error != ASE_OK)
				{
					drivers.removeCurrentDriver();
					LOGL("Failed to load device " << name << " (" << getAsioErrorString(error) << ")");
					continue;
				}

				// Determine samplerates
				std::vector<double> srates;
				for (auto& srate : m_SampleRates)
					if (ASIOCanSampleRate(srate)) srates.push_back(srate);

				// Make sure to remove the current driver once we're done querying
				drivers.removeCurrentDriver();

				// Add device
				m_Devices.push_back(DeviceInfo{ i, name, in, out, srates });
			}
			return m_Devices;
		}

		Error OpenStream(const StreamSettings& settings = StreamSettings{}) override
		{
			// The ASIO state is global, since ASIO only allows a single driver to be opened per program,
			// so if the state is not 'Loaded' we can't open another stream.
			if (m_State != Loaded)
				return AlreadyOpen;

			// Store stream settings
			m_Settings = settings;

			// For an ASIO the input and output device need to be the same.
			if (m_Settings.input.deviceId != m_Settings.output.deviceId)
			{
				LOGL("The input and output device should be the same when creating a duplex ASIO stream.");
				return InvalidDuplex;
			}

			// Retrieve necessary settings;
			int _deviceId = m_Settings.input.deviceId;
			int _nInChannels = m_Settings.input.channels;
			int _nOutChannels = m_Settings.output.channels;
			int _nChannels = _nInChannels + _nOutChannels;
			int _bufferSize = m_Settings.bufferSize;
			int _sampleRate = m_Settings.sampleRate;

			// Since ASIO doesn't have a 'default' device, just set it to 0 if none is selected
			if (_deviceId == -1)
			{
				_deviceId = 0;
				m_Settings.input.deviceId = 0; // Also update in settings
			}
			
			// Open the driver
			drivers.asioOpenDriver(_deviceId, (void**)&theAsioDriver);
			
			// If nmr of channels is not set, set it to max
			long _in, _out;
			theAsioDriver->getChannels(&_in, &_out);
			if (_nInChannels == -1)
			{
				_nInChannels = _in;
				_nChannels = _nInChannels + _nOutChannels;
				m_Settings.input.channels = _in; // Also update in settings
			}

			if (_nOutChannels == -1)
			{
				_nOutChannels = _out;
				_nChannels = _nInChannels + _nOutChannels;
				m_Settings.output.channels = _out; // Also update in settings
			}

			// Init the ASIO
			driverInfo.asioVersion = 2;
			driverInfo.sysRef = GetForegroundWindow();
			auto error = ASIOInit(&driverInfo);
			if (error != ASE_OK)
			{
				LOGL(getAsioErrorString(error));

				// Either the hardware failed
				if (error == ASE_HWMalfunction)
					return HardwareFail;

				// Or there is no input/output present
				else
					return NotPresent;
			}
			m_State = Initialized;

			// If no samplerate has been selected, pick one.
			if (_sampleRate == -1)
			{
				int _index = 0;
				while (ASIOCanSampleRate(m_SampleRates[_index]) != ASE_OK) 
					_index++;
				_sampleRate = m_SampleRates[_index];
				m_Settings.sampleRate = _sampleRate; // Also update in settings
			}

			// Set samplerate
			error = ASIOSetSampleRate(_sampleRate);
			if (error != ASE_OK)
			{
				ASIOExit();
				m_State = Loaded;
				LOGL(getAsioErrorString(error));

				// Either it's an invalid sample rate
				if (error == ASE_InvalidMode)
					return InvalidSampleRate;
				
				// Or there is no input/output present
				else
					return NotPresent;
			}

			// Create the buffer
			if (m_BufferInfos)
				delete[] m_BufferInfos;
			m_BufferInfos = new ASIOBufferInfo[_nChannels];
			for (int i = 0; i < _nInChannels; i++)
				m_BufferInfos[i].isInput = ASIOTrue,
				m_BufferInfos[i].channelNum = i;
			for (int i = _nInChannels; i < _nChannels; i++)
				m_BufferInfos[i].isInput = ASIOFalse,
				m_BufferInfos[i].channelNum = i - _nInChannels;
			error = ASIOCreateBuffers(m_BufferInfos, _nChannels, _bufferSize, &m_Callbacks);
			if (error != ASE_OK)
			{
				ASIOExit();
				LOGL(getAsioErrorString(error));

				// Either it failed to allocate memory
				if (error == ASE_NoMemory)
					return NoMemory;

				// The buffersize is invalid
				else if (error == ASE_InvalidMode)
					return InvalidBufferSize;

				// Or the input/output aren't supported
				else
					return NotPresent;
			}
			m_State = Prepared;

			// Since we can only have a single asio instance open at any time, set 
			// a static object to 'this' for access in the callbacks.
			m_AsioApi = this;
		}

		Error StartStream() override 
		{ 
			if (m_State == Loaded)
				return NotOpen;

			if (m_State == Running)
				return AlreadyRunning;

			auto error = ASIOStart();
			if (error != ASE_OK)
				return HardwareFail;

			m_State = Running; 
			return NoError; 
		};

		Error StopStream() override
		{
			if (m_State == Loaded)
				return NotOpen;

			if (m_State == Prepared)
				return NotRunning;

			auto error = ASIOStop();  
			if (error != ASE_OK)
				return HardwareFail;

			m_State = Prepared;
			return NoError;
		};

		Error CloseStream() override 
		{
			if (m_State == Loaded)
				return NotOpen;

			auto error = ASIODisposeBuffers();
			if (error != ASE_OK)
				return HardwareFail;
			
			ASIOExit();
			m_State = Loaded; 
		};

		void OpenControlPanel() 
		{
			ASIOControlPanel();
		}

	protected:
		StreamSettings m_Settings;
		
		static void m_SampleRateDidChange(ASIOSampleRate sRate)
		{

		};

		static long m_AsioMessage(long selector, long value, void* message, double* opt)
		{ 
			switch (selector)
			{
			case kAsioSupportsTimeInfo: return true;
			case kAsioBufferSizeChange: m_AsioApi->m_Settings.bufferSize = value; return true;
			}
			return 0; 
		};

		static ASIOTime* m_BufferSwitchTimeInfo (ASIOTime* params, long doubleBufferIndex, ASIOBool directProcess)
		{
			// Retrieve information from object
			int _nInChannels = m_AsioApi->m_Settings.input.channels;
			int _nOutChannels = m_AsioApi->m_Settings.output.channels;
			int _nChannels = _nInChannels + _nOutChannels;
			int _bufferSize = m_AsioApi->m_Settings.bufferSize;
			double _sampleRate = m_AsioApi->m_Settings.sampleRate;

			// Create array of buffers
			void** _inputs = new void* [_nInChannels];
			void** _outputs = new void* [_nOutChannels];
			for (int i = 0; i < _nInChannels; i++)
				_inputs[i] = m_BufferInfos[i].buffers[doubleBufferIndex];

			for (int i = 0; i < _nOutChannels; i++)
				_outputs[i] = m_BufferInfos[i + _nInChannels].buffers[doubleBufferIndex];

			// Call user callback
			m_AsioApi->m_Callback->Call(_inputs, _outputs, CallbackInfo{
				_nInChannels, _nOutChannels, _bufferSize, _sampleRate
				}, m_AsioApi->m_UserData);
			return params;
		}

		static inline ASIOCallbacks m_Callbacks = { nullptr, m_SampleRateDidChange, m_AsioMessage, m_BufferSwitchTimeInfo, };
		static inline ASIOBufferInfo* m_BufferInfos = nullptr;
		static inline AsioApi* m_AsioApi = nullptr;
		static inline State m_State = Loaded;
	};
}