# Audijo

**It is in beta!**
Help me test this badboi, I can't really test all edge cases since I myself only have 2 ASIO devices. So if you find any bugs or crashes lemme know. If you want to use ASIO you're going to have to download the asio sdk and put the folder in <code>./sdk</code> in the root folder.

Documentation: https://code.kaixo.me/Audijo/

Here's a quick example, it opens the default Wasapi device and just generates a sine wave.
```cpp
// Make a Wasapi Stream object
Stream<Wasapi> _stream;

// Set the callback, this can be a (capturing) lambda, but also a function pointer
_stream.SetCallback([&](double** input, double** output, CallbackInfo info)
    {   // generate a simple sinewave
        static int _counter = 0;
        for (int i = 0; i < info.bufferSize; i++, _counter++)
            for (int j = 0; j < info.outputChannels; j++)
                output[j][i] = std::sin(_counter * 0.01) * 0.5;
    });

// Open the stream with default settings, and then start the stream
_stream.OpenStream({ 
    .input = NoDevice, 
    .output = Default, 
    .bufferSize = Default, 
    .sampleRate = Default });
_stream.StartStream();

// Infinite loop to halt the program.
while (true);
```

Simply an up to date audio library with actual documentation and full ASIO/WASAPI support.
