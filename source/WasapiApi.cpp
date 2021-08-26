#include "WasapiApi.hpp"

namespace Audijo
{

#define CHECK(x, msg, type) if (HRESULT hr = FAILED(x)) { LOGL(msg << " code:" << hr); type; }

	template <typename T> 
	class CircularBuffer {
	private:
		std::unique_ptr<T[]> m_Buffer; 

		size_t m_Head = 0;             
		size_t m_Tail = 0;
		size_t m_Count = 0;
		size_t m_MaxSize;
		T m_EmptyItem = 0;
	public:

		CircularBuffer(size_t max_size)
			: m_Buffer(std::unique_ptr<T[]>(new T[max_size])), m_MaxSize(max_size) {};

		void Enqueue(T item) 
		{
			if (IsFull())
				Dequeue();

			m_Buffer[m_Tail] = item;
			m_Tail = (m_Tail + 1) % m_MaxSize;
			m_Count++;
		}

		T Dequeue() 
		{
			if (IsEmpty())
				return m_EmptyItem;

			T item = m_Buffer[m_Head];
			m_Buffer[m_Head] = m_EmptyItem;
			m_Head = (m_Head + 1) % m_MaxSize;
			m_Count--;
			return item;
		}

		auto Front() { return m_Buffer[m_Head]; }
		bool IsEmpty() { return m_Count == 0; }
		bool IsFull() { return m_Count == m_MaxSize; }
		int Space() { return m_MaxSize - m_Count - 1; }

		size_t Size() 
		{
			return m_Count;
		}
	};

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
					_device.sampleFormat = _format->wBitsPerSample ;

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

			if (m_Settings.input.deviceId == NoDevice)
				m_Settings.input.channels = 0;

			if (m_Settings.output.deviceId == NoDevice)
				m_Settings.output.channels = 0;
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
			CHECK(m_InputClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_CaptureClient), "Unable to retrieve the capture client.", return Fail);
			
			// If no samplerate, set to supported samplerate
			if (_sampleRate == -1)
				m_Settings.sampleRate = _sampleRate = _inFormat->nSamplesPerSec;

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
			CHECK(m_WasapiDevices->Item(_outDeviceId, &m_OutputDevice), "Unable to retrieve device handle.", return NotPresent);
			CHECK(m_OutputDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_OutputClient), "Unable to retrieve device audio client.", return Fail);
			CHECK(m_OutputClient->GetMixFormat(&_outFormat), "Unable to retrieve device mix format.", return Fail);
	
			// If no samplerate, set to supported samplerate
			if (_sampleRate == -1)
				m_Settings.sampleRate = _sampleRate = _outFormat->nSamplesPerSec;

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

		// Allocate the user callback buffers
		AllocateBuffers();

		m_State = Prepared;
		return NoError;
	}

	Error WasapiApi::StartStream()
	{
		if (m_State == Loaded)
			return NotOpen;

		if (m_State == Running)
			return AlreadyRunning;

		m_State = Running;
		m_AudioThread = std::thread{ [this]()
			{
				CHECK(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "Failed to CoInitialize thread.", return NoError);

				// Retrieve information from object
				int _nInChannels = m_Settings.input.channels;
				int _nOutChannels = m_Settings.output.channels;
				int _bufferSize = m_Settings.bufferSize;
				auto _sampleRate = m_Settings.sampleRate;
				auto _inFormat = m_Settings.m_DeviceInFormat;
				auto _outFormat = m_Settings.m_DeviceOutFormat;
				auto _format = m_Settings.m_Format;
				char** _inputs = m_InputBuffers;
				char** _outputs = m_OutputBuffers;

				auto _captureEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
				auto _renderEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

				CircularBuffer<char> _inRingBuffer{ (size_t)(4 * _bufferSize * _nInChannels * FormatBytes(_inFormat)) };
				CircularBuffer<char> _outRingBuffer{ (size_t)(4 * _bufferSize * _nOutChannels * FormatBytes(_outFormat)) };

				unsigned int _inputFramesAvailable = 0;
				unsigned int _outputFramesAvailable = 0;
				unsigned int _framePadding = 0;
				DWORD _inFlags = 0;
				BYTE* _deviceInputBuffer = nullptr;
				BYTE* _deviceOutputBuffer = nullptr;
				bool _pulled = false;
				bool _pushed = false;

				Pointer<WAVEFORMATEX> _inWaveFormat;
				if (m_InputClient)
				{
					m_CaptureClient.Release();
					m_InputClient.Release();
					CHECK(m_InputDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_InputClient), "Unable to retrieve device audio client.", return NoError);
					CHECK(m_InputClient->GetMixFormat(&_inWaveFormat), "Unable to retrieve device mix format.", return NoError);
					CHECK(m_InputClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, _inWaveFormat, nullptr), "Unable to initialize the output client", return NoError);
					CHECK(m_InputClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_CaptureClient), "Unable to retrieve the render client.", return NoError);
					CHECK(m_InputClient->SetEventHandle(_captureEvent), "Unable to set output event handle", return NoError);
					CHECK(m_InputClient->Start(), "Couldn't start the device", return NoError);
				}
				
				Pointer<WAVEFORMATEX> _outWaveFormat;
				if (m_OutputClient)
				{
					m_RenderClient.Release();
					m_OutputClient.Release();
					CHECK(m_OutputDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_OutputClient), "Unable to retrieve device audio client.", return NoError);
					CHECK(m_OutputClient->GetMixFormat(&_outWaveFormat), "Unable to retrieve device mix format.", return NoError);
					CHECK(m_OutputClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, _outWaveFormat, nullptr), "Unable to initialize the output client", return NoError);
					CHECK(m_OutputClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_RenderClient), "Unable to retrieve the render client.", return NoError);
					CHECK(m_OutputClient->SetEventHandle(_renderEvent), "Unable to set output event handle", return NoError);
					CHECK(m_OutputClient->Start(), "Couldn't start the device", return NoError);
				}

				while (m_State == Running)
				{
					// If not pulled from input buffer
					if (!_pulled)
					{
						if (m_InputClient)
						{
							// Only pull if the ring buffer contains enough samples to fill the user buffer
							if (_inRingBuffer.Size() >= _bufferSize * _nInChannels * FormatBytes(_inFormat))
							{
								// Get samples from ring buffer
								for (int i = 0; i < _bufferSize * FormatBytes(_inFormat); i++)
									for (int j = 0; j < _nInChannels; j++)
										_inputs[j][i] = _inRingBuffer.Dequeue();
								
								// Convert to the right format
								for (int j = 0; j < _nInChannels; j++)
									ConvertBuffer(_inputs[j], _inputs[j], _bufferSize, _format, _inFormat);

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
						// First convert to the right format
						for (int j = 0; j < _nOutChannels; j++)
							ConvertBuffer(_outputs[j], _outputs[j], _bufferSize, _outFormat, _format);

						// Then add it to the output ring buffer
						for (int i = 0; i < _bufferSize * FormatBytes(_outFormat); i++)
							for (int j = 0; j < _nOutChannels; j++)
								_outRingBuffer.Enqueue(_outputs[j][i]);

						_pushed = true;
					}

					// If no output device, we don't need to push, so set to true
					else
						_pushed = true;

					// If there is an input device, we'll get data from its buffer into the input ring buffer
					if (m_InputClient)
					{
						if (!_pulled)
							WaitForSingleObject(_captureEvent, INFINITE);

						// Get the buffer from the device
						CHECK(m_CaptureClient->GetBuffer(&_deviceInputBuffer, &_inputFramesAvailable, &_inFlags, nullptr, nullptr), "Failed to retrieve input buffer.", return NoError);

						// If there is enough space in the input ring buffer, we'll enqueue it.
						if (_inRingBuffer.Space() >= _inputFramesAvailable * _nInChannels * FormatBytes(_inFormat))
						{
							// Add the input data to the input ring buffer
							for (int i = 0; i < _inputFramesAvailable * _nInChannels * FormatBytes(_inFormat); i++)
								_inRingBuffer.Enqueue(_deviceInputBuffer[i]);

							CHECK(m_CaptureClient->ReleaseBuffer(_inputFramesAvailable), "Unable to release capture buffer", return NoError);
						}

						// Otherwise let wasapi know we haven't handled the buffer
						else
							CHECK(m_CaptureClient->ReleaseBuffer(0), "Unable to release capture buffer", return NoError);
					}

					// If there is an output device, we can push our ringbuffer to the device.
					if (m_OutputClient)
					{
						if (_pulled && !_pushed)
							WaitForSingleObject(_renderEvent, INFINITE);

						// Calculate the amount of frames available to write to.
						CHECK(m_OutputClient->GetBufferSize(&_outputFramesAvailable), "Unable to retrieve output buffer size", return NoError);
						CHECK(m_OutputClient->GetCurrentPadding(&_framePadding), "Unable to retrieve output frame padding", return NoError);
						_outputFramesAvailable -= _framePadding;

						// If we have enough data to  write to the device in the output ring buffer
						if (_outRingBuffer.Size() >= _outputFramesAvailable * _nOutChannels * FormatBytes(_outFormat))
						{
							// Get the buffer
							CHECK(m_RenderClient->GetBuffer(_outputFramesAvailable, &_deviceOutputBuffer), "Failed to retrieve output buffer.", return NoError);

							// Put data in the output device buffer
							for (int i = 0; i < _outputFramesAvailable * _nOutChannels * FormatBytes(_outFormat); i++)
								_deviceOutputBuffer[i] = _outRingBuffer.Dequeue();

							CHECK(m_RenderClient->ReleaseBuffer(_outputFramesAvailable, 0), "Unable to release capture buffer", return NoError);
						}
					}

					// If data has been pushed, let the device know we can pull again.
					if (_pushed)
						_pulled = false;
				}
			}
		};

		return NoError;
	};

	Error WasapiApi::StopStream()
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

	Error WasapiApi::CloseStream()
	{
		if (m_State == Loaded)
			return NotOpen;

		if (m_State == Running)
		{
			StopStream();
		}

		m_State = Loaded;
	};
}