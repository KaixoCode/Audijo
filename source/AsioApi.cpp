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
	AsioApi::AsioApi()
		: ApiBase()
	{   // Load devices
		Devices();
	}

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
					_device.defaultDevice = i == 0; // No default in ASIO, so just id == 0

					// Erase from the toDelete, since we've still found the device!
					_toDelete.erase(std::find(_toDelete.begin(), _toDelete.end(), _index));
					break;
				}
				_index++;
			}

			if (!_found) // No default in ASIO, so just id == 0
				m_Devices.push_back(DeviceInfo{ i, _name, _in, _out, _srates, i == 0 });
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

		// Make sure to free buffers first
		FreeBuffers();

		// Store stream settings
		m_Settings = settings;

		// Check device ids
		{
			// If only one of the device ids has been set, set other one to the same one.
			if (m_Settings.input.deviceId == NoDevice && m_Settings.output.deviceId != NoDevice)
				m_Settings.input.deviceId = m_Settings.output.deviceId;

			if (m_Settings.input.deviceId != NoDevice && m_Settings.output.deviceId == NoDevice)
				m_Settings.output.deviceId = m_Settings.input.deviceId;

			// For an ASIO the input and output device need to be the same.
			if (m_Settings.input.deviceId != m_Settings.output.deviceId)
			{
				LOGL("The input and output device should be the same when creating a duplex ASIO stream.");
				return InvalidDuplex;
			}

			// Since ASIO doesn't have a 'default' device, just set it to 0 if none is selected
			if (m_Settings.input.deviceId == DefaultDevice)
				m_Settings.input.deviceId = m_Settings.output.deviceId = 0;
		}

		// Set default channel count
		{
			// If nmr of channels is not set, set it to max
			if (m_Settings.input.channels <= -1)
				m_Settings.input.channels = m_Devices[m_Settings.input.deviceId].inputChannels;

			if (m_Settings.output.channels <= -1)
				m_Settings.output.channels = m_Devices[m_Settings.output.deviceId].outputChannels;
		}

		// Retrieve necessary settings;
		int _deviceId = m_Settings.input.deviceId;
		int _nInChannels = m_Settings.input.channels;
		int _nOutChannels = m_Settings.output.channels;
		int _nChannels = _nInChannels + _nOutChannels;
		int _bufferSize = m_Settings.bufferSize;
		int _sampleRate = m_Settings.sampleRate;

		// Open the driver
		{
			drivers.asioOpenDriver(_deviceId, (void**)&theAsioDriver);

			// Init the ASIO
			driverInfo.asioVersion = 2;
			driverInfo.sysRef = GetForegroundWindow();
			auto _error = ASIOInit(&driverInfo);
			if (_error != ASE_OK)
			{
				LOGL("Failed to initialize ASIO: " << getAsioErrorString(_error));

				// Either the hardware failed or there is no input/output present
				return _error == ASE_HWMalfunction ? Fail : NotPresent;
			}
			m_State = Initialized;
		}

		// SampleRate
		{
			// If no samplerate has been selected, pick one.
			if (_sampleRate == -1)
			{
				int _index = 0;
				while (_index < sizeof(m_SampleRates) / sizeof(double) && ASIOCanSampleRate(m_SampleRates[_index]) != ASE_OK)
					_index++;
				_sampleRate = m_SampleRates[_index];
				m_Settings.sampleRate = _sampleRate; // Also update in settings
			}

			// Set samplerate
			auto _error = ASIOSetSampleRate(_sampleRate);
			if (_error != ASE_OK)
			{
				LOGL("Failed to set sample rate to " << _sampleRate << ": " << getAsioErrorString(_error));
				m_State = Loaded; // Reset the state to loaded

				// Either it's an invalid sample rate or there is no input/output present
				return _error == ASE_InvalidMode ? InvalidSampleRate : NotPresent;
			}
		}

		// Get channel formats
		{
			// Input channels
			ASIOChannelInfo _inChannelInfo;
			_inChannelInfo.channel = 0;
			_inChannelInfo.isInput = true;
			auto _error = ASIOGetChannelInfo(&_inChannelInfo);
			if (_error != ASE_OK) 
			{
				LOGL("Failed to collect channel info: " << getAsioErrorString(_error));
				m_State = Loaded;  // Reset the state to loaded
				return NotPresent;
			}

			m_Settings.m_InByteSwap = false;
			m_Settings.m_DeviceInFormat = Float32;
			switch (_inChannelInfo.type)
			{
			case ASIOSTInt16MSB: m_Settings.m_InByteSwap = true;
			case ASIOSTInt16LSB: m_Settings.m_DeviceInFormat = Int16;
				break;		
			case ASIOSTInt24MSB:
			case ASIOSTInt24LSB:
				return UnsupportedSampleFormat;
			case ASIOSTInt32MSB: m_Settings.m_InByteSwap = true;
			case ASIOSTInt32LSB: m_Settings.m_DeviceInFormat = Int32;
				break;			
			case ASIOSTFloat32MSB: m_Settings.m_InByteSwap = true;
			case ASIOSTFloat32LSB: m_Settings.m_DeviceInFormat = Float32;
				break;			
			case ASIOSTFloat64MSB: m_Settings.m_InByteSwap = true;
			case ASIOSTFloat64LSB: m_Settings.m_DeviceInFormat = Float64;
				break;
			}

			// Output channels
			ASIOChannelInfo _outChannelInfo;
			_outChannelInfo.channel = 0;
			_outChannelInfo.isInput = true;
			_error = ASIOGetChannelInfo(&_outChannelInfo);
			if (_error != ASE_OK)
			{
				LOGL("Failed to collect channel info: " << getAsioErrorString(_error));
				m_State = Loaded;  // Reset the state to loaded
				return NotPresent;
			}

			m_Settings.m_OutByteSwap = false;
			m_Settings.m_DeviceOutFormat = Float32;
			switch (_outChannelInfo.type)
			{
			case ASIOSTInt16MSB: m_Settings.m_OutByteSwap = true;
			case ASIOSTInt16LSB: m_Settings.m_DeviceOutFormat = Int16;
				break;			
			case ASIOSTInt24MSB: 
			case ASIOSTInt24LSB: 
				return UnsupportedSampleFormat;
			case ASIOSTInt32MSB: m_Settings.m_OutByteSwap = true;
			case ASIOSTInt32LSB: m_Settings.m_DeviceOutFormat = Int32;
				break;
			case ASIOSTFloat32MSB: m_Settings.m_OutByteSwap = true;
			case ASIOSTFloat32LSB: m_Settings.m_DeviceOutFormat = Float32;
				break;
			case ASIOSTFloat64MSB: m_Settings.m_OutByteSwap = true;
			case ASIOSTFloat64LSB: m_Settings.m_DeviceOutFormat = Float64;
				break;
			}

			// If callback has been set, deduce format type
			if (m_Callback)
			{
				int bytes = m_Callback->Bytes();
				bool floating = m_Callback->Floating();

				if (floating)
					m_Settings.m_Format = bytes == 4 ? Float32 : Float64;
				else
					m_Settings.m_Format = bytes == 1 ? Int8 : bytes == 2 ? Int16 : Int32;
			}
			else
			{
				LOGL("Failed to deduce sample format, no callback was set.");
				return NoCallback;
			}
		}

		// Create the buffer
		{
			// First clean up any previous buffer infos
			if (m_BufferInfos)
				delete[] m_BufferInfos;

			// Allocate new buffer info objects
			m_BufferInfos = new ASIOBufferInfo[_nChannels];
			for (int i = 0; i < _nInChannels; i++)
				m_BufferInfos[i].isInput = true,
				m_BufferInfos[i].channelNum = i;
			for (int i = _nInChannels; i < _nChannels; i++)
				m_BufferInfos[i].isInput = false,
				m_BufferInfos[i].channelNum = i - _nInChannels;

			// Create the buffers
			auto _error = ASIOCreateBuffers(m_BufferInfos, _nChannels, _bufferSize, &m_Callbacks);
			if (_error != ASE_OK)
			{
				LOGL("Failed to create ASIO buffers: " << getAsioErrorString(_error));

				// Either it failed to allocate memory, the buffersize is invalid or the input/output aren't present
				return _error == ASE_NoMemory ? NoMemory : _error == ASE_InvalidMode ? InvalidBufferSize : NotPresent;
			}
			m_State = Prepared;

			// Allocate the user callback buffers
			AllocateBuffers();
		}

		// Since we can only have a single asio instance open at any time, set 
		// a static object to 'this' for access in the callbacks.
		m_AsioApi = this;
		return NoError;
	}

	Error AsioApi::StartStream()
	{
		if (m_State == Loaded)
			return NotOpen;

		if (m_State == Running)
			return AlreadyRunning;

		auto error = ASIOStart();
		if (error != ASE_OK)
			return Fail;

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
			return Fail;

		m_State = Prepared;
		return NoError;
	};

	Error AsioApi::CloseStream()
	{
		if (m_State == Loaded)
			return NotOpen;

		if (m_State == Running)
		{
			auto error = ASIOStop();
			if (error != ASE_OK)
				return Fail;
		}

		auto error = ASIODisposeBuffers();
		if (error != ASE_OK)
			return Fail;

		m_State = Loaded;
	};

	Error AsioApi::OpenControlPanel()
	{ 
		if (m_State == Loaded)
			return NotOpen;

		ASIOControlPanel(); 
		return NoError;
	}

	void AsioApi::SampleRateDidChange(ASIOSampleRate sRate)
	{
		m_AsioApi->m_Settings.sampleRate = sRate;
	};

	long AsioApi::AsioMessage(long selector, long value, void* message, double* opt)
	{
		switch (selector)
		{
		case kAsioSupportsTimeInfo: return true;
		case kAsioSupportsTimeCode: return true;
		case kAsioBufferSizeChange: 
		{
			// When the buffer size changes, free the current buffers
			// and then allocate new buffers for the user callback
			m_AsioApi->FreeBuffers();
			m_AsioApi->m_Settings.bufferSize = value;
			m_AsioApi->AllocateBuffers();
			return true;
		}
		case kAsioResetRequest:
		{
			return true;
		}
		case kAsioEngineVersion: return 2L;
		}
		return 0;
	};

	ASIOTime* AsioApi::BufferSwitchTimeInfo(ASIOTime* params, long doubleBufferIndex, ASIOBool directProcess)
	{
		// Retrieve information from object
		int _nInChannels  = m_AsioApi->m_Settings.input.channels;
		int _nOutChannels = m_AsioApi->m_Settings.output.channels;
		int _bufferSize   = m_AsioApi->m_Settings.bufferSize;
		auto _sampleRate  = m_AsioApi->m_Settings.sampleRate;
		auto _inFormat    = m_AsioApi->m_Settings.m_DeviceInFormat;
		bool _inSwap      = m_AsioApi->m_Settings.m_InByteSwap;
		auto _outFormat   = m_AsioApi->m_Settings.m_DeviceOutFormat;
		bool _outSwap     = m_AsioApi->m_Settings.m_OutByteSwap;
		auto _format      = m_AsioApi->m_Settings.m_Format;
		char** _inputs    = m_AsioApi->m_InputBuffers;
		char** _outputs   = m_AsioApi->m_OutputBuffers;

		// Prepare the input buffer
		for (int i = 0; i < _nInChannels; i++)
		{
			char* _temp = (char*)m_BufferInfos[i].buffers[doubleBufferIndex];
			if (_inSwap)
				m_AsioApi->ByteSwapBuffer(_temp, _bufferSize, _inFormat);
			m_AsioApi->ConvertBuffer(_inputs[i], _temp, _bufferSize, _inFormat, _format);
		}

		// usercallback
		m_AsioApi->m_Callback->Call((void**)_inputs, (void**)_outputs, CallbackInfo{
			_nInChannels, _nOutChannels, _bufferSize, _sampleRate
			}, m_AsioApi->m_UserData);

		// Convert the output buffer
		for (int i = 0; i < _nOutChannels; i++)
		{
			char* _temp = (char*)m_BufferInfos[i + _nOutChannels].buffers[doubleBufferIndex];
			m_AsioApi->ConvertBuffer(_temp, _outputs[i], _bufferSize, _outFormat, _format);
			if (_outSwap)
				m_AsioApi->ByteSwapBuffer(_temp, _bufferSize, _outFormat);
		}

		return params;
	}

	ASIOCallbacks AsioApi::m_Callbacks = { nullptr, SampleRateDidChange, AsioMessage, BufferSwitchTimeInfo, };
	ASIOBufferInfo* AsioApi::m_BufferInfos = nullptr;
	AsioApi* AsioApi::m_AsioApi = nullptr;
	AsioApi::State AsioApi::m_State = Loaded;
}