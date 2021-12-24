# Audijo
Very simple modern C++ audio library with Wasapi and ASIO support. Requires C++20.

Documentation: https://code.kaixo.me/Audijo/

Here's a quick example, it opens the default Wasapi device and just generates noise.
```cpp
Stream<Wasapi> _stream;

// Generate noise
_stream.Callback([&](Buffer<float>& input, Buffer<float>& output, CallbackInfo info) {   
    for (auto& _frame : output)
        for (auto& _channel : _frame)
            _channel = 0.5 * ((std::rand() % 10000) / 10000. - 0.5);
});

_stream.Open({ // Open stream
    .input = NoDevice,
    .output = Default,
    .bufferSize = Default, 
    .sampleRate = Default 
});

// Get information
_stream.Information().sampleRate;
_stream.Information().bufferSize; // etc.

// Start stream
_stream.Start();
std::cin.get(); // Wait for console input to close stream
_stream.Close();
```

Simply an up to date audio library with actual documentation and full ASIO/WASAPI support.
