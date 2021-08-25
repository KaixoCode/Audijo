#include "Audijo.hpp"

using namespace Audijo;

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto Exit; }
#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

HRESULT PlayAudioStream()
{
    CoInitialize(nullptr);
    HRESULT hr;
    REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
    REFERENCE_TIME hnsActualDuration;
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioClient* pAudioClient = NULL;
    IAudioRenderClient* pRenderClient = NULL;
    WAVEFORMATEX* pwfx = NULL;
    UINT32 bufferFrameCount;
    UINT32 numFramesAvailable;
    UINT32 numFramesPadding;
    BYTE* pData;
    DWORD flags = 0;
    E_POINTER;

    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator, NULL,
        CLSCTX_ALL, IID_IMMDeviceEnumerator,
        (void**)&pEnumerator);
    EXIT_ON_ERROR(hr)

        hr = pEnumerator->GetDefaultAudioEndpoint(
            eRender, eConsole, &pDevice);
    EXIT_ON_ERROR(hr)

        hr = pDevice->Activate(
            IID_IAudioClient, CLSCTX_ALL,
            NULL, (void**)&pAudioClient);
    EXIT_ON_ERROR(hr)
        hr = pAudioClient->GetMixFormat(&pwfx);
    EXIT_ON_ERROR(hr)

        hr = pAudioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            0,
            hnsRequestedDuration,
            0,
            pwfx,
            NULL);
    EXIT_ON_ERROR(hr)

        // Get the actual size of the allocated buffer.
        hr = pAudioClient->GetBufferSize(&bufferFrameCount);
    EXIT_ON_ERROR(hr)

        hr = pAudioClient->GetService(
            IID_IAudioRenderClient,
            (void**)&pRenderClient);
    EXIT_ON_ERROR(hr)

        // Grab the entire buffer for the initial fill operation.
        hr = pRenderClient->GetBuffer(bufferFrameCount, &pData);
    EXIT_ON_ERROR(hr)

        hr = pRenderClient->ReleaseBuffer(bufferFrameCount, flags);
    EXIT_ON_ERROR(hr)

        // Calculate the actual duration of the allocated buffer.
        hnsActualDuration = (double)REFTIMES_PER_SEC *
        bufferFrameCount / pwfx->nSamplesPerSec;

    hr = pAudioClient->Start();  // Start playing.
    EXIT_ON_ERROR(hr)

        // Each loop fills about half of the shared buffer.
        while (flags != AUDCLNT_BUFFERFLAGS_SILENT)
        {
            // Sleep for half the buffer duration.
            Sleep((DWORD)(hnsActualDuration / REFTIMES_PER_MILLISEC / 2));

            // See how much buffer space is available.
            hr = pAudioClient->GetCurrentPadding(&numFramesPadding);
            EXIT_ON_ERROR(hr)

                numFramesAvailable = bufferFrameCount - numFramesPadding;

            // Grab all the available space in the shared buffer.
            hr = pRenderClient->GetBuffer(numFramesAvailable, &pData);
            EXIT_ON_ERROR(hr)

                // Get next 1/2-second of data from the audio source.
                for (int i = 0; i < numFramesAvailable * 2 * 4; i++)
                    pData[i] = (std::rand());

            hr = pRenderClient->ReleaseBuffer(numFramesAvailable, flags);
            EXIT_ON_ERROR(hr)
        }

    // Wait for last data in buffer to play before stopping.
    Sleep((DWORD)(hnsActualDuration / REFTIMES_PER_MILLISEC / 2));

    hr = pAudioClient->Stop();  // Stop playing.
    EXIT_ON_ERROR(hr)

        Exit:
    CoTaskMemFree(pwfx);
    SAFE_RELEASE(pEnumerator)
        SAFE_RELEASE(pDevice)
        SAFE_RELEASE(pAudioClient)
        SAFE_RELEASE(pRenderClient)

        return hr;
}


int main()
{
    return PlayAudioStream();
    //Stream<Asio> _stream;

	//_stream.SetCallback([&](double** input, double** output, CallbackInfo info)
	//	{   // generate a simple sinewave
	//		static int _counter = 0;
	//		for (int i = 0; i < info.bufferSize; i++, _counter++)
	//			for (int j = 0; j < info.outputChannels; j++)
	//				output[j][i] = std::sin(_counter * 0.01) * 0.5;
	//	});
	
	//StreamSettings _settings;
	//_settings.bufferSize = 256;
	//_settings.sampleRate = 44100;
	//_settings.output.deviceId = DefaultDevice;
	//_stream.OpenStream(_settings);
	//_stream.StartStream();

	//_stream.OpenControlPanel();
	//while (true);

	Stream<Wasapi> _stream;

	auto& _devs = _stream.Devices();
	for (auto& _dev : _devs)
	{
		LOGL("name:    " << _dev.name);
		LOGL("id:      " << _dev.id);
		LOGL("in:      " << _dev.inputChannels);
		LOGL("out:     " << _dev.outputChannels);
		LOGL("bits:    " << _dev.sampleFormat);
		LOG("srates:  ");
		for (auto& i : _dev.sampleRates)
			LOG(i << ", ");
		LOGL("");
	}
	_stream.SetCallback([&](float** input, float** output, CallbackInfo info)
		{   // generate a simple sinewave
			static int _counter = 0;
			for (int i = 0; i < info.bufferSize; i++, _counter++)
				for (int j = 0; j < info.outputChannels; j++)
					output[j][i] = std::sin(_counter * 0.01) * 0.5;
		});

	StreamSettings _settings;
	_settings.bufferSize = 256;
	_settings.output.deviceId = DefaultDevice;
	_stream.OpenStream(_settings);
	_stream.StartStream();

	while (true);
}
