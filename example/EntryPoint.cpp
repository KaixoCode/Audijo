#include "Audijo/Audijo.hpp"

using namespace Audijo;

int main()
{
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
	
	
	//Stream _stream{ Wasapi };

	//auto& _devs = _stream.Devices();
	//for (auto& _dev : _devs)
	//{
	//	LOGL("name:    " << _dev.name);
	//	LOGL("id:      " << _dev.id);
	//	LOGL("in:      " << _dev.inputChannels);
	//	LOGL("out:     " << _dev.outputChannels);
	//	LOG("srates:  ");
	//	for (auto& i : _dev.sampleRates)
	//		LOG(i << ", ");
	//	LOGL("");
	//}
	//_stream.SetCallback([&](float** input, float** output, CallbackInfo info)
	//	{   // Forward the input audio to the output
	//		//for (int i = 0; i < info.bufferSize; i++)
	//		//	for (int j = 0; j < std::min(info.outputChannels, info.inputChannels); j++)
	//		//		output[j][i] = input[j][i];

	//		static int _counter = 0;
	//		for (int i = 0; i < info.bufferSize; i++, _counter++)
	//			for (int j = 0; j < info.outputChannels; j++)
	//				output[j][i] = 0.5 * ((std::rand() % 10000) / 10000. - 0.5);
	//	});

	//StreamParameters _settings;
	//_settings.bufferSize = 256;
	//_settings.input = DefaultDevice;
	//_settings.output = DefaultDevice;
	//_stream.OpenStream(_settings);
	//_stream.StartStream();

	//while (true);
	//std::this_thread::sleep_for(std::chrono::milliseconds(5000));

	//_stream.CloseStream();
}
