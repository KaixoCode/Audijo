#include "AsioApi.hpp"

/**
 * Quick way to get error string from ASIOError
 * @param error code
 * @return error message
 */
static const char* getAsioErrorString(ASIOError error)
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
		if (m[i].value == error) return m[i].message;

	return "Unknown error.";
}

static AsioDrivers drivers;
static ASIODriverInfo driverInfo;
extern IASIO* theAsioDriver;

namespace Audijo
{
	const std::vector<DeviceInfo>& AsioApi::Devices()
	{
		// Make a list of all numbers from 0 to amount of current devices
		// this is used to determine if we need to delete devices from the list after querying.
		std::vector<int> _toDelete;
		for (int i = m_Devices.size() - 1; i >= 0; i--)
			_toDelete.push_back(i);

		// Go through all devices
		for (int i = 0; i < drivers.asioGetNumDev(); i++)
		{
			// Get device name
			char _name[32];
			auto _error = drivers.asioGetDriverName(i, _name, 32);
			if (_error != ASE_OK)
			{
				LOGL("Failed to load device " << _name << " (" << getAsioErrorString(_error) << ")");
				continue;
			}

			// Load the driver to collect further information
			if (!drivers.loadDriver(_name))
			{
				LOGL("Failed to load device: " << _name);
				continue;
			}

			// Init the asio
			_error = ASIOInit(&driverInfo);
			if (_error != ASE_OK)
			{
				LOGL("Failed to load device " << _name << " (" << getAsioErrorString(_error) << ")");
				continue;
			}

			// Get channel counts
			long _in, _out;
			_error = ASIOGetChannels(&_in, &_out);
			if (_error != ASE_OK)
			{
				drivers.removeCurrentDriver();
				LOGL("Failed to load device " << _name << " (" << getAsioErrorString(_error) << ")");
				continue;
			}

			// Determine samplerates
			std::vector<double> _srates;
			for (auto& _srate : m_SampleRates)
				if (ASIOCanSampleRate(_srate)) _srates.push_back(_srate);

			// Make sure to remove the current driver once we're done querying
			drivers.removeCurrentDriver();

			// Edit an existing device, or if doesn't exist, just add new device.
			bool _found = false;
			int _index = 0;
			for (auto& _device : m_Devices)
			{
				if (_device.name == _name)
				{
					_found = true;
					_device.id = i;
					_device.inputChannels = _in;
					_device.outputChannels = _out;
					_device.sampleRates = _srates;

					// Erase from the toDelete, since we've still found the device!
					_toDelete.erase(std::find(_toDelete.begin(), _toDelete.end(), _index));
					break;
				}
				_index++;
			}

			if (!_found)
				m_Devices.push_back(DeviceInfo{ i, _name, _in, _out, _srates });
		}

		// Delete all devices we couldn't find again.
		for (auto& i : _toDelete)
			m_Devices.erase(m_Devices.begin() + i);

		return m_Devices;
	}

	Error AsioApi::OpenStream(const StreamSettings& settings)
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

	Error AsioApi::StartStream()
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

	Error AsioApi::StopStream()
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

	Error AsioApi::CloseStream()
	{
		if (m_State == Loaded)
			return NotOpen;

		auto error = ASIODisposeBuffers();
		if (error != ASE_OK)
			return HardwareFail;

		ASIOExit();
		m_State = Loaded;
	};

	void AsioApi::SampleRateDidChange(ASIOSampleRate sRate)
	{

	};

	long AsioApi::AsioMessage(long selector, long value, void* message, double* opt)
	{
		switch (selector)
		{
		case kAsioSupportsTimeInfo: return true;
		case kAsioBufferSizeChange: m_AsioApi->m_Settings.bufferSize = value; return true;
		}
		return 0;
	};

	ASIOTime* AsioApi::BufferSwitchTimeInfo(ASIOTime* params, long doubleBufferIndex, ASIOBool directProcess)
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

	ASIOCallbacks AsioApi::m_Callbacks = { nullptr, SampleRateDidChange, AsioMessage, BufferSwitchTimeInfo, };
	ASIOBufferInfo* AsioApi::m_BufferInfos = nullptr;
	AsioApi* AsioApi::m_AsioApi = nullptr;
	AsioApi::State AsioApi::m_State = Loaded;
}