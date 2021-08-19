# Audijo

**It is in beta!**
Help me test this badboi, I can't really test all edge cases since I myself only have 2 ASIO devices. So if you find any bugs or crashes lemme know. ASIO should function right now, next up is WASAPI.

Here's a quick example, just generates a sinewave, and opens the first ASIO device it finds.
```cpp
Stream<Asio> _stream;

_stream.SetCallback([&](double** input, double** output, CallbackInfo info) -> int
    {   // generate a simple sinewave
        static int _counter = 0;
        for (int i = 0; i < info.bufferSize; i++, _counter++)
            for (int j = 0; j < info.outputChannels; j++)
                output[j][i] = std::sin(_counter * 0.01) * 0.5;
        return 0;
    });

StreamSettings _settings;
_settings.bufferSize = 256;
_settings.sampleRate = 48000;
_settings.output.deviceId = 0;
_stream.OpenStream(_settings);
_stream.StartStream();

while (true);
```

Simply an up to date audio library with actual documentation and full ASIO/WASAPI support.
