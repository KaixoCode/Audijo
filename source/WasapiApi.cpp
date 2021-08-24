#include "WasapiApi.hpp"

namespace Audijo
{
	#define SAFE_RELEASE( objectPtr )\
	if ( objectPtr )\
	{\
	  objectPtr->Release();\
	  objectPtr = nullptr;\
	}

#define CHECK(x, msg, type) if (FAILED(x)) { LOGL(msg); type; }

	WasapiApi::WasapiApi()
		: ApiBase()
	{
		// WASAPI can run either apartment or multi-threaded
		HRESULT hr = CoInitialize(nullptr);
		if (!FAILED(hr))
			m_CoInitialized = true;

		// Instantiate device enumerator
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
			CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
			(void**)&m_DeviceEnumerator);

		// Load devices once at the start
		Devices();
	}

	const std::vector<DeviceInfo>& WasapiApi::Devices()
	{
		// Make a list of all numbers from 0 to amount of current devices
		// this is used to determine if we need to delete devices from the list after querying.
		std::vector<int> _toDelete;
		for (int i = m_Devices.size() - 1; i >= 0; i--)
			_toDelete.push_back(i);

		// Get default devices
		Pointer<IMMDevice> _defaultIn;
		Pointer<IMMDevice> _defaultOut;
		CHECK(m_DeviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &_defaultIn), "Unable to retrieve the default input device");
		CHECK(m_DeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &_defaultOut), "Unable to retrieve the default output device");

		// Open the property store of the device to retrieve the name
		Pointer<IPropertyStore> _defaultInPropertyStore;
		Pointer<IPropertyStore> _defaultOutPropertyStore;
		CHECK(_defaultIn->OpenPropertyStore(STGM_READ, &_defaultInPropertyStore), "Unable to open default device property store.", return m_Devices);
		CHECK(_defaultOut->OpenPropertyStore(STGM_READ, &_defaultOutPropertyStore), "Unable to open default device property store.", return m_Devices);

		// Collect the name of the default input from the property store
		char _defaultInNameArr[64];
		char _defaultOutNameArr[64];
		Pointer<PROPVARIANT> _defaultInDeviceNameProp = new PROPVARIANT;
		Pointer<PROPVARIANT> _defaultOutDeviceNameProp = new PROPVARIANT;
		PropVariantInit(_defaultInDeviceNameProp);
		PropVariantInit(_defaultOutDeviceNameProp);
		CHECK(_defaultInPropertyStore->GetValue(PKEY_Device_FriendlyName, _defaultInDeviceNameProp), "Unable to retrieve default device name.", return m_Devices);
		CHECK(_defaultOutPropertyStore->GetValue(PKEY_Device_FriendlyName, _defaultOutDeviceNameProp), "Unable to retrieve default device name.", return m_Devices);
		wcstombs(_defaultInNameArr, _defaultInDeviceNameProp->pwszVal, 64); // Copy the wstring to the char array
		wcstombs(_defaultOutNameArr, _defaultOutDeviceNameProp->pwszVal, 64); // Copy the wstring to the char array
		std::string _defaultInName = _defaultInNameArr;
		std::string _defaultOutName = _defaultOutNameArr;

		// Count devices
		unsigned int _count = 0;
		m_WasapiDevices.Release();
		CHECK(m_DeviceEnumerator->EnumAudioEndpoints(eAll, DEVICE_STATE_ACTIVE, &m_WasapiDevices), "Unable to retrieve device collection.", return m_Devices);
		CHECK(m_WasapiDevices->GetCount(&_count), "Unable to retrieve device count.", return m_Devices);
		
		// Go through all devices
		for (int i = 0; i < _count; i++)
		{
			// First get the device from the device collection
			Pointer<IMMDevice> _dev;
			CHECK(m_WasapiDevices->Item(i, &_dev), "Unable to retrieve device handle.", continue);

			// Open the property store of the device to retrieve the name
			Pointer<IPropertyStore> _propertyStore;
			CHECK(_dev->OpenPropertyStore(STGM_READ, &_propertyStore), "Unable to open device property store.", continue);

			// Collect the name from the property store
			char _nameArr[64];
			Pointer<PROPVARIANT> _deviceNameProp = new PROPVARIANT;
			PropVariantInit(_deviceNameProp);
			CHECK(_propertyStore->GetValue(PKEY_Device_FriendlyName, _deviceNameProp), "Unable to retrieve device name.", continue);
			wcstombs(_nameArr, _deviceNameProp->pwszVal, 64); // Copy the wstring to the char array
			std::string _name = _nameArr;

			// Get the audio client from the device
			Pointer<IAudioClient> _audioClient;
			CHECK(_dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&_audioClient), "Unable to retrieve device audio client.", continue);

			// Get the device mix format using the audio client
			Pointer<WAVEFORMATEX> _format;
			CHECK(_audioClient->GetMixFormat(&_format), "Unable to retrieve device mix format.", continue);

			// Get the endpoint object to retrieve the device type
			Pointer<IMMEndpoint> _endpoint;
			CHECK(_dev->QueryInterface(__uuidof(IMMEndpoint), (void**)&_endpoint), "Unable to retrieve the endpoint object", continue);
			EDataFlow _deviceType;
			CHECK(_endpoint->GetDataFlow(&_deviceType), "Unable to get the device type", continue);

			// Set channel count given the device type
			int _in = 0, _out = 0;
			switch(_deviceType) {
			case eAll: _out = _in = _format->nChannels; break;
			case eRender: _out = _format->nChannels; break;
			case eCapture: _in = _format->nChannels; break;
			}

			// Is it a default device? Use device name to check
			bool _default = (_name == _defaultInName && _in > 0) || (_name == _defaultOutName && _out > 0);

			// Wasapi only supports single sample rate
			std::vector<double> _srates;
			_srates.push_back(_format->nSamplesPerSec);

			// Edit an existing device, or if doesn't exist, just add new device.
			bool _found = false;
			int _index = 0;
			for (auto& _device : m_Devices)
			{
				if (_device.name == _name && _device.id == i)
				{
					_found = true;
					_device.id = i;
					_device.inputChannels = _in;
					_device.outputChannels = _out;
					_device.sampleRates = _srates;
					_device.defaultDevice = _default;

					// Erase from the toDelete, since we've still found the device!
					_toDelete.erase(std::find(_toDelete.begin(), _toDelete.end(), _index));
					break;
				}
				_index++;
			}

			if (!_found)
				m_Devices.push_back(DeviceInfo{ i, _name, _in, _out, _srates, _default });
		}

		// Delete all devices we couldn't find again.
		for (auto& i : _toDelete)
			m_Devices.erase(m_Devices.begin() + i);

		return m_Devices;
	}

	Error WasapiApi::OpenStream(const StreamSettings& settings)
	{
		if (m_State != Loaded)
			return AlreadyOpen;

		m_Settings = settings;

		// Check device ids
		{
			if (m_Settings.input.deviceId == DefaultDevice)
				for (auto& i : m_Devices)
					if (i.defaultDevice && i.inputChannels > 0)
						m_Settings.input.deviceId = i.id;
			if (m_Settings.output.deviceId == DefaultDevice)
				for (auto& i : m_Devices)
					if (i.defaultDevice && i.outputChannels > 0)
						m_Settings.output.deviceId = i.id;
		}

		// Set default channel count
		{
			// If nmr of channels is not set, set it to max
			if (m_Settings.input.deviceId != NoDevice && m_Settings.input.channels <= -1)
				m_Settings.input.channels = m_Devices[m_Settings.input.deviceId].inputChannels;

			if (m_Settings.output.deviceId != NoDevice && m_Settings.output.channels <= -1)
				m_Settings.output.channels = m_Devices[m_Settings.output.deviceId].outputChannels;
		}

		// Retrieve necessary settings;
		int _inDeviceId = m_Settings.input.deviceId;
		int _outDeviceId = m_Settings.output.deviceId;
		int _nInChannels = m_Settings.input.channels;
		int _nOutChannels = m_Settings.output.channels;
		int _nChannels = _nInChannels + _nOutChannels;
		int _bufferSize = m_Settings.bufferSize;
		int _sampleRate = m_Settings.sampleRate;

		if (_inDeviceId != NoDevice)
		{
			Pointer<WAVEFORMATEX> _inFormat;
			m_InputDevice.Release();
			m_InputClient.Release();
			m_CaptureClient.Release();
			CHECK(m_WasapiDevices->Item(_inDeviceId, &m_InputDevice), "Unable to retrieve device handle.", return NotPresent);
			CHECK(m_InputDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_InputClient), "Unable to retrieve device audio client.", return Fail);
			CHECK(m_InputClient->GetMixFormat(&_inFormat), "Unable to retrieve device mix format.", return Fail);
			CHECK(m_InputClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, _inFormat, nullptr), "Unable to initialize the input client", return Fail);
			CHECK(m_InputClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_CaptureClient), "Unable to retrieve the capture client.", return Fail);
			
			// If no samplerate, set to supported samplerate
			if (_sampleRate == -1)
				_sampleRate = _inFormat->nSamplesPerSec;

			// Otherwise check samplerate
			else if (_inFormat->nSamplesPerSec != _sampleRate)
				return InvalidSampleRate;

			// Set native sample format
			if (_inFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || (_inFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
				((WAVEFORMATEXTENSIBLE*)_inFormat.get())->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
			{
				if (_inFormat->wBitsPerSample == 32)
					m_Settings.m_DeviceInFormat = Float32;
				else if (_inFormat->wBitsPerSample == 64)
					m_Settings.m_DeviceInFormat = Float64;
			}
			else if (_inFormat->wFormatTag == WAVE_FORMAT_PCM || (_inFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
				((WAVEFORMATEXTENSIBLE*)_inFormat.get())->SubFormat == KSDATAFORMAT_SUBTYPE_PCM))
			{
				if (_inFormat->wBitsPerSample == 8)
					m_Settings.m_DeviceInFormat = Int8;
				else if (_inFormat->wBitsPerSample == 16)
					m_Settings.m_DeviceInFormat = Int16;
				else if (_inFormat->wBitsPerSample == 24)
					return UnsupportedSampleFormat;
				else if (_inFormat->wBitsPerSample == 32)
					m_Settings.m_DeviceInFormat = Int32;
			}
		}

		if (_outDeviceId != NoDevice)
		{
			Pointer<WAVEFORMATEX> _outFormat;
			m_OutputDevice.Release();
			m_OutputClient.Release();
			m_RenderClient.Release();
			CHECK(m_WasapiDevices->Item(_outDeviceId, &m_OutputDevice), "Unable to retrieve device handle.", return NotPresent);
			CHECK(m_OutputDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_OutputClient), "Unable to retrieve device audio client.", return Fail);
			CHECK(m_OutputClient->GetMixFormat(&_outFormat), "Unable to retrieve device mix format.", return Fail);
			CHECK(m_OutputClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, _outFormat, nullptr), "Unable to initialize the output client", return Fail);
			CHECK(m_OutputClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_RenderClient), "Unable to retrieve the render client.", return Fail);

			// If no samplerate, set to supported samplerate
			if (_sampleRate == -1)
				_sampleRate = _outFormat->nSamplesPerSec;

			// If invalid samplerate and was valid for input device, it's invalid duplex because no support for resampling.
			if (_outFormat->nSamplesPerSec != _sampleRate)
				return _inDeviceId != -1 ? InvalidDuplex : InvalidSampleRate;

			// Set native sample format
			if (_outFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || (_outFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
				((WAVEFORMATEXTENSIBLE*)_outFormat.get())->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
			{
				if (_outFormat->wBitsPerSample == 32)
					m_Settings.m_DeviceOutFormat = Float32;
				else if (_outFormat->wBitsPerSample == 64)
					m_Settings.m_DeviceOutFormat = Float64;
			}
			else if (_outFormat->wFormatTag == WAVE_FORMAT_PCM || (_outFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
				((WAVEFORMATEXTENSIBLE*)_outFormat.get())->SubFormat == KSDATAFORMAT_SUBTYPE_PCM))
			{
				if (_outFormat->wBitsPerSample == 8)
					m_Settings.m_DeviceOutFormat = Int8;
				else if (_outFormat->wBitsPerSample == 16)
					m_Settings.m_DeviceOutFormat = Int16;
				else if (_outFormat->wBitsPerSample == 24)
					return UnsupportedSampleFormat;
				else if (_outFormat->wBitsPerSample == 32)
					m_Settings.m_DeviceOutFormat = Int32;
			}
		}
		// TODO check if all settings are correct


		// TODO open stream
		return NoError;
	}

	Error WasapiApi::StartStream()
	{
		if (m_State == Loaded)
			return NotOpen;

		if (m_State == Running)
			return AlreadyRunning;

		// TODO Start stream

		m_State = Running;
		return NoError;
	};

	Error WasapiApi::StopStream()
	{
		if (m_State == Loaded)
			return NotOpen;

		if (m_State == Prepared)
			return NotRunning;

		// TODO stop stream

		m_State = Prepared;
		return NoError;
	};

	Error WasapiApi::CloseStream()
	{
		if (m_State == Loaded)
			return NotOpen;

		if (m_State == Running)
		{
			// TODO stop stream
		}

		// TODO close stream

		m_State = Loaded;
	};
}