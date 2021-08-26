#include "Audijo.hpp"

using namespace Audijo;

int main()
{
	//Stream _stream{ Asio };

	//_stream.SetCallback([&](double** input, double** output, CallbackInfo info)
	//	{   // generate a simple sinewave
	//		static int _counter = 0;
	//		for (int i = 0; i < info.bufferSize; i++, _counter++)
	//			for (int j = 0; j < info.outputChannels; j++)
	//				output[j][i] = std::sin(_counter * 0.01) * 0.5;
	//	});
	//
	//StreamSettings _settings;
	//_settings.bufferSize = 256;
	//_settings.sampleRate = 44100;
	//_settings.output.deviceId = DefaultDevice;
	//_stream.OpenStream(_settings);
	//_stream.StartStream();

	//_stream.Get<Asio>().OpenControlPanel();
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
		{   
			for (int i = 0; i < info.bufferSize; i++)
				for (int j = 0; j < info.outputChannels; j++)
					output[j][i] = input[j][i];

			//// generate a simple sinewave
			//static int _counter = 0;
			//for (int i = 0; i < info.bufferSize; i++, _counter++)
			//	for (int j = 0; j < info.outputChannels; j++)
			//		output[j][i] += std::sin(_counter * 0.01) * 0.5;
		});

	_stream.OpenStream();
	_stream.StartStream();

	while (true);
}
