#include "Audijo.hpp"

#include <variant>

using namespace Audijo;



int main()
{
	Stream<Asio> _stream;

	int _counter = 0;
	auto _callback = [&](double** input, double** output, Audijo::CallbackInfo info) -> int {
		for (int i = 0; i < info.bufferSize; i++, _counter++)
			for (int j = 0; j < info.outputChannels; j++)
				output[j][i] = std::sin(_counter * 0.01) * 0.5;
		return 0;
	};
	_stream.SetCallback(_callback);

	StreamSettings _settings;
	_settings.bufferSize = 256;
	_settings.input.deviceId = 0;
	_stream.OpenStream(_settings);
	_stream.StartStream();

	while (true);
}
