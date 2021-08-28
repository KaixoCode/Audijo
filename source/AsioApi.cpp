#ifdef AUDIJO_ASIO
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
#define CHECK(x, msg, type) if (auto _error = x) { LOGL(msg << " (" << getAsioErrorString(_error) << ")"); type; }
	/*
	 * API Specific DeviceInfo object
	 */
	DeviceInfo<Asio>::DeviceInfo(DeviceInfo<>&& d)
		: DeviceInfo<>{ std::forward<DeviceInfo<>>(d) }
	{
		Channels();
	}

	ChannelInfo& DeviceInfo<Asio>::Channel(int index, bool input) const
	{
		return input ? Channels()[index] : Channels()[inputChannels + index];
	};

	std::vector<ChannelInfo>& DeviceInfo<Asio>::Channels() const
	{
		// Only probe information if it's the same id, or when there's not an opened asio driver
		if (m_Channels.empty() && ((AsioApi::m_AsioApi && AsioApi::m_AsioApi->m_Information.input == id) || AsioApi::m_State == AsioApi::Loaded))
		{
			// Only open driver if not currently opened
			if (!AsioApi::m_AsioApi || AsioApi::m_AsioApi->m_Information.input != id)
			{
				char _name[32];
				drivers.asioGetDriverName(id, _name, 32);
				drivers.loadDriver(_name);
				ASIOInit(&driverInfo);
			}

			// Load inputs
			for (int i = 0; i < inputChannels; i++)
			{
				ASIOChannelInfo info;
				info.channel = i;
				info.isInput = true;
				ASIOGetChannelInfo(&info);
				m_Channels.emplace_back(info.name, info.channelGroup, (bool)info.isActive, true);
			}

			// Load outputs
			for (int i = 0; i < outputChannels; i++)
			{
				ASIOChannelInfo info;
				info.channel = i;
				info.isInput = false;
				ASIOGetChannelInfo(&info);
				m_Channels.emplace_back(info.name, info.channelGroup, (bool)info.isActive, false);
			}

			// Remove current driver if it's not the opened one
			if (!AsioApi::m_AsioApi || AsioApi::m_AsioApi->m_Information.input != id)
				drivers.removeCurrentDriver();

			return m_Channels;
		}
		else
			return m_Channels;
	}

	/*
	 * API
	 */

	AsioApi::AsioApi()
		: ApiBase()
	{   // Load devices
		Devices();
	}

	AsioApi::~AsioApi()
	{   
		Close();
	}

	const std::vector<DeviceInfo<Asio>>& AsioApi::Devices()
	{
		// Can't probe new info when a stream has been opened
		if (m_State != Loaded)
			return m_Devices;

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
			CHECK(drivers.asioGetDriverName(i, _name, 32), "Failed to load device " << _name, continue);

			// Load the driver to collect further information
			if (!drivers.loadDriver(_name))
			{
				LOGL("Failed to load device: " << _name);
				continue;
			}

			// Init the asio
			CHECK(ASIOInit(&driverInfo), "Failed to load device " << _name, continue);

			// Get channel counts
			long _in, _out;
			CHECK(ASIOGetChannels(&_in, &_out), "Failed to load device " << _name, drivers.removeCurrentDriver(); continue);

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
				m_Devices.push_back(DeviceInfo<Asio>{{ i, _name, _in, _out, _srates, i == 0, Asio }});
		}

		// Delete all devices we couldn't find again.
		for (auto& i : _toDelete)
			m_Devices.erase(m_Devices.begin() + i);

		return m_Devices;
	}

	Error AsioApi::Open(const StreamParameters& settings)
	{
		// The ASIO state is global, since ASIO only allows a single driver to be opened per program,
		// so if the state is not 'Loaded' we can't open another stream.
		if (m_State != Loaded)
			return AlreadyOpen;

		// Make sure to free buffers first
		FreeBuffers();

		// Store stream settings
		m_Information = settings;

		// Check device ids
		{
			// If only one of the device ids has been set, set other one to the same one.
			if (m_Information.input == NoDevice && m_Information.output != NoDevice)
				m_Information.input = m_Information.output;

			if (m_Information.input != NoDevice && m_Information.output == NoDevice)
				m_Information.output = m_Information.input;

			// For an ASIO the input and output device need to be the same.
			if (m_Information.input != m_Information.output)
			{
				LOGL("The input and output device should be the same when creating a duplex ASIO stream.");
				return InvalidDuplex;
			}

			// Since ASIO doesn't have a 'default' device, just set it to 0 if none is selected
			if (m_Information.input == Default)
				m_Information.input = m_Information.output = 0;
		}

		// Set default channel count
		{
			// If nmr of channels is not set, set it to max
			m_Information.inputChannels = m_Devices[m_Information.input].inputChannels;
			m_Information.outputChannels = m_Devices[m_Information.output].outputChannels;
		}

		// Retrieve necessary settings;
		int _deviceId = m_Information.input;
		int _nInChannels = m_Information.inputChannels;
		int _nOutChannels = m_Information.outputChannels;
		int _nChannels = _nInChannels + _nOutChannels;
		int _bufferSize = m_Information.bufferSize;
		int _sampleRate = m_Information.sampleRate;

		// Open the driver
		{
			drivers.asioOpenDriver(_deviceId, (void**)&theAsioDriver);

			// Init the ASIO
			driverInfo.asioVersion = 2;
			driverInfo.sysRef = GetForegroundWindow();
			CHECK(ASIOInit(&driverInfo), "Failed to initialize ASIO: ", return _error == ASE_HWMalfunction ? Fail : NotPresent);
			m_State = Initialized;
		}

		// SampleRate
		{
			// If no samplerate has been selected, pick one.
			if (_sampleRate == Default)
			{
				int _index = 0;
				while (_index < sizeof(m_SampleRates) / sizeof(double) && ASIOCanSampleRate(m_SampleRates[_index]) != ASE_OK)
					_index++;
				_sampleRate = m_SampleRates[_index];
				m_Information.sampleRate = _sampleRate; // Also update in settings
			}

			// Set samplerate
			CHECK(ASIOSetSampleRate(_sampleRate), "Failed to set sample rate to " << _sampleRate << ": ",
				m_State = Loaded; return _error == ASE_InvalidMode ? InvalidSampleRate : NotPresent);
		}

		// Get channel formats
		{
			// Input channels
			ASIOChannelInfo _inChannelInfo;
			_inChannelInfo.channel = 0;
			_inChannelInfo.isInput = true;
			CHECK(ASIOGetChannelInfo(&_inChannelInfo), "Failed to collect channel info: ",
				m_State = Loaded; return NotPresent);

			m_Information.deviceInFormat = Float32;
			switch (_inChannelInfo.type)
			{
			case ASIOSTInt16MSB: m_Information.deviceInFormat = m_SInt16; break;
			case ASIOSTInt16LSB: m_Information.deviceInFormat = Int16; break;
			case ASIOSTInt24MSB:
			case ASIOSTInt24LSB:
				return UnsupportedSampleFormat;
			case ASIOSTInt32MSB: m_Information.deviceInFormat = m_SInt32; break;
			case ASIOSTInt32LSB: m_Information.deviceInFormat = Int32; break;
			case ASIOSTFloat32MSB: m_Information.deviceInFormat = m_SFloat32; break;
			case ASIOSTFloat32LSB: m_Information.deviceInFormat = Float32; break;
			case ASIOSTFloat64MSB: m_Information.deviceInFormat = m_SFloat64; break;
			case ASIOSTFloat64LSB: m_Information.deviceInFormat = Float64; break;
			}

			// Output channels
			ASIOChannelInfo _outChannelInfo;
			_outChannelInfo.channel = 0;
			_outChannelInfo.isInput = true;
			CHECK(ASIOGetChannelInfo(&_outChannelInfo), "Failed to collect channel info: ",
				m_State = Loaded; return NotPresent);

			m_Information.deviceOutFormat = Float32;
			switch (_outChannelInfo.type)
			{
			case ASIOSTInt16MSB: m_Information.deviceOutFormat = m_SInt16; break;
			case ASIOSTInt16LSB: m_Information.deviceOutFormat = Int16; break;
			case ASIOSTInt24MSB:
			case ASIOSTInt24LSB:
				return UnsupportedSampleFormat;
			case ASIOSTInt32MSB: m_Information.deviceOutFormat = m_SInt32; break;
			case ASIOSTInt32LSB: m_Information.deviceOutFormat = Int32; break;
			case ASIOSTFloat32MSB: m_Information.deviceOutFormat = m_SFloat32; break;
			case ASIOSTFloat32LSB: m_Information.deviceOutFormat = Float32; break;
			case ASIOSTFloat64MSB: m_Information.deviceOutFormat = m_SFloat64; break;
			case ASIOSTFloat64LSB: m_Information.deviceOutFormat = Float64; break;
			}

			// If callback has been set, deduce format type
			if (m_Callback)
			{
				m_Information.inFormat = (SampleFormat)m_Callback->InFormat();
				m_Information.outFormat = (SampleFormat)m_Callback->OutFormat();
			}
			else
			{
				LOGL("Failed to deduce sample format, no callback was set.");
				return NoCallback;
			}
		}

		// Create the buffer
		{

			// Find prefered buffer size of device
			if (m_Information.bufferSize == Default)
			{
				long a, b, _prefered, d;
				ASIOGetBufferSize(&a, &b, &_prefered, &d);
				m_Information.bufferSize = _bufferSize = _prefered;
			}

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
			CHECK(ASIOCreateBuffers(m_BufferInfos, _nChannels, _bufferSize, &m_Callbacks), "Failed to create ASIO buffers: ",
				return _error == ASE_NoMemory ? NoMemory : _error == ASE_InvalidMode ? InvalidBufferSize : NotPresent);
			
			m_State = Prepared;

			// Allocate the user callback buffers
			AllocateBuffers();
		}

		// Since we can only have a single asio instance open at any time, set 
		// a static object to 'this' for access in the callbacks.
		m_AsioApi = this;
		return NoError;
	}

	Error AsioApi::Start()
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

	Error AsioApi::Stop()
	{
		if (m_State == Loaded)
			return NotOpen;

		if (m_State == Prepared)
			return NotRunning;

		CHECK(ASIOStop(), "Failed to stop the stream.", return Fail);

		m_State = Prepared;
		return NoError;
	};

	Error AsioApi::Close()
	{
		if (m_State == Loaded)
			return NotOpen;

		if (m_State == Running)
			CHECK(ASIOStop(), "Failed to stop the stream.", return Fail);

		CHECK(ASIODisposeBuffers(), "Failed to dispose buffers", return Fail);

		m_State = Loaded;
		return NoError;
	};	
	
	Error AsioApi::SampleRate(double srate)
	{
		if (m_State == Loaded)
			return NotOpen;

		CHECK(ASIOSetSampleRate(srate), "Failed to set sample rate", 
			return _error == ASE_InvalidMode ? InvalidSampleRate : Fail);

		return NoError;
	};

	Error AsioApi::OpenControlPanel()
	{ 
		if (m_State == Loaded)
			return NotOpen;

		CHECK(ASIOControlPanel(), "Failed to open control panel", return Fail);
		return NoError;
	}

	void AsioApi::SampleRateDidChange(ASIOSampleRate sRate)
	{
		m_AsioApi->m_Information.sampleRate = sRate;
	};

	long AsioApi::AsioMessage(long selector, long value, void* message, double* opt)
	{
		switch (selector)
		{
		case kAsioSelectorSupported:
			if (value == kAsioResetRequest
				|| value == kAsioEngineVersion
				|| value == kAsioResyncRequest
				|| value == kAsioLatenciesChanged
				|| value == kAsioBufferSizeChange
				|| value == kAsioSupportsTimeInfo
				|| value == kAsioSupportsTimeCode
				|| value == kAsioSupportsInputMonitor)
				return 1L;
			return 0L;
		case kAsioSupportsTimeInfo: return 1L;
		case kAsioSupportsTimeCode: return 0L;
		case kAsioBufferSizeChange: 
		{
			// When the buffer size changes, free the current buffers
			// and then allocate new buffers for the user callback
			m_AsioApi->FreeBuffers();
			m_AsioApi->m_Information.bufferSize = value;
			m_AsioApi->AllocateBuffers();
			return 1L;
		}
		case kAsioResetRequest:
		{
			// Set new buffersize
			//long a, b, pSize, c;
			//ASIOGetBufferSize(&a, &b, &pSize, &c);
			//m_AsioApi->m_Parameters.bufferSize = pSize;

			// Notify that we need to reset
			m_State = Reset;
			return 1L;
		}
		case kAsioEngineVersion: return 2L;
		case kAsioResyncRequest: return 1L;
		case kAsioLatenciesChanged: return 1L;
		}
		return 0;
	};

	ASIOTime* AsioApi::BufferSwitchTimeInfo(ASIOTime* params, long doubleBufferIndex, ASIOBool directProcess)
	{
		// Retrieve information from object
		int _nInChannels      = m_AsioApi->m_Information.inputChannels;
		int _nOutChannels     = m_AsioApi->m_Information.outputChannels;
		int _bufferSize       = m_AsioApi->m_Information.bufferSize;
		auto _sampleRate      = m_AsioApi->m_Information.sampleRate;
		auto _deviceInFormat  = m_AsioApi->m_Information.deviceInFormat;
		bool _inSwap          = m_AsioApi->m_Information.deviceInFormat & Swap;
		auto _deviceOutFormat = m_AsioApi->m_Information.deviceOutFormat;
		bool _outSwap         = m_AsioApi->m_Information.deviceOutFormat & Swap;
		auto _inFormat        = m_AsioApi->m_Information.inFormat;
		auto _outFormat       = m_AsioApi->m_Information.outFormat;
		char** _inputs        = m_AsioApi->m_InputBuffers;
		char** _outputs       = m_AsioApi->m_OutputBuffers;

		// Prepare the input buffer
		for (int i = 0; i < _nInChannels; i++)
		{
			char* _temp = (char*)m_BufferInfos[i].buffers[doubleBufferIndex];
			if (_inSwap)
				m_AsioApi->ByteSwapBuffer(_temp, _bufferSize, _deviceInFormat);
			m_AsioApi->ConvertBuffer(_inputs[i], _temp, _bufferSize, _deviceInFormat, _inFormat);
		}

		// usercallback
		m_AsioApi->m_Callback->Call((void**)_inputs, (void**)_outputs, CallbackInfo{
			_nInChannels, _nOutChannels, _bufferSize, _sampleRate
			}, m_AsioApi->m_UserData);

		// Convert the output buffer
		for (int i = 0; i < _nOutChannels; i++)
		{
			char* _temp = (char*)m_BufferInfos[i + _nOutChannels].buffers[doubleBufferIndex];
			m_AsioApi->ConvertBuffer(_temp, _outputs[i], _bufferSize, _deviceOutFormat, _outFormat);
			if (_outSwap)
				m_AsioApi->ByteSwapBuffer(_temp, _bufferSize, _deviceOutFormat);
		}
		ASIOOutputReady();

		return params;
	}

	ASIOCallbacks AsioApi::m_Callbacks = { nullptr, SampleRateDidChange, AsioMessage, BufferSwitchTimeInfo, };
	ASIOBufferInfo* AsioApi::m_BufferInfos = nullptr;
	AsioApi* AsioApi::m_AsioApi = nullptr;
	AsioApi::State AsioApi::m_State = Loaded;
}
#endif