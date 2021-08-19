#include "Audijo.hpp"

using namespace Audijo;

int main()
{
	Stream<Asio> _stream;

	_stream.SetCallback([&](double** input, double** output, CallbackInfo info)
		{   // generate a simple sinewave
			static int _counter = 0;
			for (int i = 0; i < info.bufferSize; i++, _counter++)
				for (int j = 0; j < info.outputChannels; j++)
					output[j][i] = std::sin(_counter * 0.01) * 0.5;
		});

	StreamSettings _settings;
	_settings.bufferSize = 256;
	_settings.sampleRate = 48000;
	_settings.output.deviceId = 0;
	_stream.OpenStream(_settings);
	_stream.StartStream();

	while (true);
}
