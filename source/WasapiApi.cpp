#ifdef AUDIJO_WASAPI
#include "WasapiApi.hpp"
#include "RingBuffer.hpp"

namespace Audijo
{
#define CHECK(x, msg, type) if (FAILED(x)) { LOGL(msg); type; }

	WasapiApi::WasapiApi()
		: ApiBase()
	{
		// WASAPI can run either apartment or multi-threaded
		HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		
		if (!FAILED(hr))
			m_CoInitialized = true;

		// Instantiate device enumerator
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
			CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
			(void**)&m_DeviceEnumerator);

		// Load devices once at the start
		Devices();
	}

	const std::vector<DeviceInfo<Wasapi>>& WasapiApi::Devices()
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
				m_Devices.push_back(DeviceInfo<Wasapi>{ { i, _name, _in, _out, _srates, _default, Wasapi } });
		}

		// Delete all devices we couldn't find again.
		for (auto& i : _toDelete)
			m_Devices.erase(m_Devices.begin() + i);

		return m_Devices;
	}

	Error WasapiApi::Open(const StreamParameters& settings)
	{
		if (m_State != Loaded)
			return AlreadyOpen;

		m_Information = settings;

		// Check device ids
		if (m_Information.input == Default)
			for (auto& i : m_Devices)
				if (i.defaultDevice && i.inputChannels > 0)
					m_Information.input = i.id;
		if (m_Information.output == Default)
			for (auto& i : m_Devices)
				if (i.defaultDevice && i.outputChannels > 0)
					m_Information.output = i.id;
		
		// Set channel count
		m_Information.inputChannels = m_Information.input == NoDevice ? 0 : m_Devices[m_Information.input].inputChannels;
		m_Information.outputChannels = m_Information.output == NoDevice ? 0 : m_Devices[m_Information.output].outputChannels;

		// Default buffersize is 256 because why not lol
		if (m_Information.bufferSize == Default)
			m_Information.bufferSize = 256;

		// Retrieve necessary settings;
		int _inDeviceId = m_Information.input;
		int _outDeviceId = m_Information.output;
		int _nInChannels = m_Information.inputChannels;
		int _nOutChannels = m_Information.outputChannels;
		int _nChannels = _nInChannels + _nOutChannels;
		int _bufferSize = m_Information.bufferSize;
		int _sampleRate = m_Information.sampleRate;

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
			CHECK(m_InputClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_CaptureClient), "Unable to retrieve the capture client.", return Fail);
			
			// If no samplerate, set to supported samplerate
			if (_sampleRate == Default)
				m_Information.sampleRate = _sampleRate = _inFormat->nSamplesPerSec;

			// Otherwise check samplerate
			else if (_inFormat->nSamplesPerSec != _sampleRate && !m_Information.resampling)
			{
				LOGL("Invalid sample rate selected");
				return InvalidSampleRate;
			}

			// Set native sample format
			if (_inFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || (_inFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
				((WAVEFORMATEXTENSIBLE*)_inFormat.get())->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
			{
				if (_inFormat->wBitsPerSample == 32)
					m_Information.deviceInFormat = Float32;
				else if (_inFormat->wBitsPerSample == 64)
					m_Information.deviceInFormat = Float64;
			}
			else if (_inFormat->wFormatTag == WAVE_FORMAT_PCM || (_inFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
				((WAVEFORMATEXTENSIBLE*)_inFormat.get())->SubFormat == KSDATAFORMAT_SUBTYPE_PCM))
			{
				if (_inFormat->wBitsPerSample == 8)
					m_Information.deviceInFormat = Int8;
				else if (_inFormat->wBitsPerSample == 16)
					m_Information.deviceInFormat = Int16;
				else if (_inFormat->wBitsPerSample == 24)
					return UnsupportedSampleFormat;
				else if (_inFormat->wBitsPerSample == 32)
					m_Information.deviceInFormat = Int32;
			}
		}

		if (_outDeviceId != NoDevice)
		{
			Pointer<WAVEFORMATEX> _outFormat;
			m_OutputDevice.Release();
			m_OutputClient.Release();
			CHECK(m_WasapiDevices->Item(_outDeviceId, &m_OutputDevice), "Unable to retrieve device handle.", return NotPresent);
			CHECK(m_OutputDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_OutputClient), "Unable to retrieve device audio client.", return Fail);
			CHECK(m_OutputClient->GetMixFormat(&_outFormat), "Unable to retrieve device mix format.", return Fail);
	
			// If no samplerate, set to supported samplerate
			if (_sampleRate == Default)
				m_Information.sampleRate = _sampleRate = _outFormat->nSamplesPerSec;

			// If invalid samplerate and was valid for input device, it's invalid duplex because no support for resampling.
			if (_outFormat->nSamplesPerSec != _sampleRate && !m_Information.resampling)
			{
				LOGL("Invalid sample rate selected");
				return _inDeviceId != NoDevice ? InvalidDuplex : InvalidSampleRate;
			}

			// Set native sample format
			if (_outFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || (_outFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
				((WAVEFORMATEXTENSIBLE*)_outFormat.get())->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
			{
				if (_outFormat->wBitsPerSample == 32)
					m_Information.deviceOutFormat = Float32;
				else if (_outFormat->wBitsPerSample == 64)
					m_Information.deviceOutFormat = Float64;
			}
			else if (_outFormat->wFormatTag == WAVE_FORMAT_PCM || (_outFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
				((WAVEFORMATEXTENSIBLE*)_outFormat.get())->SubFormat == KSDATAFORMAT_SUBTYPE_PCM))
			{
				if (_outFormat->wBitsPerSample == 8)
					m_Information.deviceOutFormat = Int8;
				else if (_outFormat->wBitsPerSample == 16)
					m_Information.deviceOutFormat = Int16;
				else if (_outFormat->wBitsPerSample == 24)
					return UnsupportedSampleFormat;
				else if (_outFormat->wBitsPerSample == 32)
					m_Information.deviceOutFormat = Int32;
			}
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

		// Allocate the user callback buffers
		AllocateBuffers();

		m_State = Prepared;
		return NoError;
	}

	Error WasapiApi::Start()
	{
		if (m_State == Loaded)
			return NotOpen;

		if (m_State == Running)
			return AlreadyRunning;

		m_State = Running;
		m_AudioThread = std::thread{ [this]()
			{
				CHECK(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "Failed to CoInitialize thread.", return);

				HMODULE AvrtDll = LoadLibraryW(L"AVRT.dll");
				if (AvrtDll) {
					typedef HANDLE(__stdcall* TAvSetMmThreadCharacteristicsPtr)(LPCWSTR TaskName, LPDWORD TaskIndex);
					DWORD taskIndex = 0;
					TAvSetMmThreadCharacteristicsPtr AvSetMmThreadCharacteristicsPtr =
						(TAvSetMmThreadCharacteristicsPtr)(void(*)()) GetProcAddress(AvrtDll, "AvSetMmThreadCharacteristicsW");
					AvSetMmThreadCharacteristicsPtr(L"Pro Audio", &taskIndex);
					FreeLibrary(AvrtDll);
				}

				int _nInChannels = m_Information.inputChannels;
				int _nOutChannels = m_Information.outputChannels;
				int _bufferSize = m_Information.bufferSize;
				auto _sampleRate = m_Information.sampleRate;
				auto _deviceInFormat = m_Information.deviceInFormat;
				auto _deviceOutFormat = m_Information.deviceOutFormat;
				auto _inFormat = m_Information.inFormat;
				auto _outFormat = m_Information.outFormat;
				char** _inputs = m_InputBuffers;
				char** _outputs = m_OutputBuffers;

				auto _captureEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
				auto _renderEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
				HANDLE _events[2]{ _captureEvent, _renderEvent };

				unsigned int _inputFramesAvailable = 0;
				unsigned int _outputFramesAvailable = 0;
				unsigned int _framePadding = 0;
				DWORD _flags = 0;
				BYTE* _streamBuffer = nullptr;
				bool _pulled = false;
				bool _pushed = false;

				// Initialize input device
				if (m_InputClient)
				{
					Pointer<WAVEFORMATEX> _inWaveFormat;
					m_CaptureClient.Release();
					m_InputClient.Release();
					CHECK(m_InputDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_InputClient), "Unable to retrieve device audio client.", return);
					CHECK(m_InputClient->GetMixFormat(&_inWaveFormat), "Unable to retrieve device mix format.", return);
					CHECK(m_InputClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, _inWaveFormat, nullptr), "Unable to initialize the output client", return);
					CHECK(m_InputClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_CaptureClient), "Unable to retrieve the render client.", return);
					CHECK(m_InputClient->SetEventHandle(_captureEvent), "Unable to set output event handle", return);
					CHECK(m_InputClient->Start(), "Couldn't start the device", return);
					CHECK(m_InputClient->GetBufferSize(&_inputFramesAvailable), "Unable to retrieve output buffer size", return);
				}
				
				// Initialize output device
				if (m_OutputClient)
				{
					Pointer<WAVEFORMATEX> _outWaveFormat;
					m_RenderClient.Release();
					m_OutputClient.Release();
					CHECK(m_OutputDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_OutputClient), "Unable to retrieve device audio client.", return);
					CHECK(m_OutputClient->GetMixFormat(&_outWaveFormat), "Unable to retrieve device mix format.", return);
					CHECK(m_OutputClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, _outWaveFormat, nullptr), "Unable to initialize the output client", return);
					CHECK(m_OutputClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_RenderClient), "Unable to retrieve the render client.", return);
					CHECK(m_OutputClient->SetEventHandle(_renderEvent), "Unable to set output event handle", return);
					CHECK(m_OutputClient->Start(), "Couldn't start the device", return);
					CHECK(m_OutputClient->GetBufferSize(&_outputFramesAvailable), "Unable to retrieve output buffer size", return);
				}

				// Create ring buffers
				RingBuffer<char> _inRingBuffer{ (_bufferSize + _inputFramesAvailable) * _nInChannels * (_deviceInFormat & Bytes) };
				RingBuffer<char> _outRingBuffer{ (_bufferSize + _outputFramesAvailable) * _nOutChannels * (_deviceOutFormat & Bytes) };

				// Create temporary buffers
				char** _tempInBuff = new char* [_nInChannels];
				for (int i = 0; i < _nInChannels; i++)
					_tempInBuff[i] = new char[_bufferSize * (_deviceInFormat & Bytes)];

				char** _tempOutBuff = new char* [_nOutChannels];
				for (int i = 0; i < _nOutChannels; i++)
					_tempOutBuff[i] = new char[_bufferSize * (_deviceOutFormat & Bytes)];

				// Start loop
				while (m_State == Running)
				{
					// If not pulled from input buffer
					if (!_pulled)
					{
						// If there is an input device, pull from ring buffer
						if (m_InputClient)
						{
							// Only pull if the ring buffer contains enough samples to fill the user buffer
							if (_inRingBuffer.Size() >= _bufferSize * _nInChannels * (_deviceInFormat & Bytes))
							{
								// Get samples from ring buffer
								for (int i = 0; i < _bufferSize; i++)
									for (int j = 0; j < _nInChannels; j++)
										for (int k = 0; k < (_deviceInFormat & Bytes); k++)
											_tempInBuff[j][i * (_deviceInFormat & Bytes) + k] = _inRingBuffer.Dequeue();

								// Convert to the right format
								for (int j = 0; j < _nInChannels; j++)
									ConvertBuffer(_inputs[j], _tempInBuff[j], _bufferSize, _inFormat, _deviceInFormat);

								_pulled = true;
							}
						}

						// If no input device, we don't need to pull, so set to true
						else
							_pulled = true;
						
						// If data pull from input ring buffer was successful, we can call the callback with the data. 
						if (_pulled)
							m_Callback->Call((void**)_inputs, (void**)_outputs, 
								CallbackInfo{ _nInChannels, _nOutChannels, _bufferSize, _sampleRate }, m_UserData);
					}

					// If we've pull, it means the callback was called, so we need to handle the user output buffer
					if (m_OutputClient && _pulled)
					{
						if (_outRingBuffer.Space() >= _bufferSize * _nOutChannels * (_deviceOutFormat & Bytes))
						{
							// First convert to the right format
							for (int j = 0; j < _nOutChannels; j++)
								ConvertBuffer(_tempOutBuff[j], _outputs[j], _bufferSize, _deviceOutFormat, _outFormat);

							// Then add it to the output ring buffer
							for (int i = 0; i < _bufferSize; i++)
								for (int j = 0; j < _nOutChannels; j++)
									for (int k = 0; k < (_deviceOutFormat & Bytes); k++)
										_outRingBuffer.Enqueue(_tempOutBuff[j][i * (_deviceOutFormat & Bytes) + k]);

							_pushed = true;
						}
						else
							_pushed = false;
					}

					// If no output device, we don't need to push, so set to true
					else
						_pushed = true;

					// Wait for one of the events, if duplex.
					DWORD _handled;
					if (m_InputClient && m_OutputClient)
						_handled = WaitForMultipleObjects(2, _events, false, INFINITE);

					// Wait only for input
					else if (m_InputClient && !_pulled)
						_handled = WaitForSingleObject(_captureEvent, INFINITE);

					// Or wait only for output
					else if (m_OutputClient && _pulled && !_pushed)
						_handled = WaitForSingleObject(_renderEvent, INFINITE);

					// If there is an input device, we'll get data from its buffer into the input ring buffer
					if (m_InputClient && (!m_OutputClient || _handled == WAIT_OBJECT_0))
					{
						// Get the buffer from the device
						CHECK(m_CaptureClient->GetBuffer(&_streamBuffer, &_inputFramesAvailable, &_flags, nullptr, nullptr), "Failed to retrieve input buffer.", goto Cleanup);

						// If there is enough space in the input ring buffer, we'll enqueue it.
						if (_inRingBuffer.Space() >= _inputFramesAvailable * _nInChannels * (_deviceInFormat & Bytes))
						{
							// Add the input data to the input ring buffer
							for (int i = 0; i < _inputFramesAvailable * _nInChannels * (_deviceInFormat & Bytes); i++)
								_inRingBuffer.Enqueue(_streamBuffer[i]);

							CHECK(m_CaptureClient->ReleaseBuffer(_inputFramesAvailable), "Unable to release capture buffer", goto Cleanup);
						}

						// Otherwise let wasapi know we haven't handled the buffer
						else
							CHECK(m_CaptureClient->ReleaseBuffer(0), "Unable to release capture buffer", goto Cleanup);
					}

					// If there is an output device, we can push our ringbuffer to the device.
					if (m_OutputClient && (!m_InputClient || _handled == WAIT_OBJECT_0 + 1))
					{
						// Calculate the amount of frames available to write to.
						CHECK(m_OutputClient->GetBufferSize(&_outputFramesAvailable), "Unable to retrieve output buffer size", goto Cleanup);
						CHECK(m_OutputClient->GetCurrentPadding(&_framePadding), "Unable to retrieve output frame padding", goto Cleanup);
						_outputFramesAvailable -= _framePadding;

						// If we have enough data to write to the device from the output ring buffer
						if (_outputFramesAvailable != 0 && _outRingBuffer.Size() >= _outputFramesAvailable * _nOutChannels * (_deviceOutFormat & Bytes))
						{
							// Get the buffer
							CHECK(m_RenderClient->GetBuffer(_outputFramesAvailable, &_streamBuffer), "Failed to retrieve output buffer.", goto Cleanup);

							// Put data in the output device buffer
							for (int i = 0; i < _outputFramesAvailable * _nOutChannels * (_deviceOutFormat & Bytes); i++)
								_streamBuffer[i] = _outRingBuffer.Dequeue();
							
							CHECK(m_RenderClient->ReleaseBuffer(_outputFramesAvailable, 0), "Unable to release capture buffer", goto Cleanup);
						}

						// Otherwise let wasapi know we haven't handled the buffer
						else
							CHECK(m_RenderClient->ReleaseBuffer(0, 0), "Unable to release capture buffer", goto Cleanup);
					}

					// If data has been pushed, let the device know we can pull again.
					if (_pushed)
						_pulled = false;
				}

			Cleanup:
				// Delete the temporary buffers
				for (int i = 0; i < _nInChannels; i++)
					delete[] _tempInBuff[i];
				delete[] _tempInBuff;

				for (int i = 0; i < _nOutChannels; i++)
					delete[] _tempOutBuff[i];
				delete[] _tempOutBuff;

				CoUninitialize();
			}
		};

		return NoError;
	};

	Error WasapiApi::Stop()
	{
		if (m_State == Loaded)
			return NotOpen;

		if (m_State == Prepared)
			return NotRunning;

		m_State = Prepared;
		try
		{
			m_AudioThread.join();
		}
		catch (const std::system_error& e)
		{
			LOGL(e.what());
		}
		return NoError;
	};

	Error WasapiApi::Close()
	{
		if (m_State == Loaded)
			return NotOpen;

		if (m_State == Running)
			Stop();
		
		m_InputDevice.Release();  
		m_OutputDevice.Release(); 
		m_InputClient.Release();  
		m_OutputClient.Release(); 
		m_CaptureClient.Release();
		m_RenderClient.Release(); 
		m_State = Loaded;
	};

	Error WasapiApi::SampleRate(double srate)
	{
		if (m_State == Loaded)
			return NotOpen;

		return Fail;
	};
}
#endif