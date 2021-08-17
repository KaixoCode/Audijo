#include "Audijo.hpp"

using namespace Audijo;

int main()
{

	Stream<Asio> _stream;
	auto& devs1 = _stream.Devices();
	auto& devs2 = _stream.Devices();


	int _counter = 0;
	auto _callback = [&](float** input, float** output, Audijo::CallbackInfo info) -> int {
		for (int i = 0; i < info.bufferSize; i++)
		{
			_counter++;
			float data = std::sin(_counter * 0.01) * 0.5;
			for (int j = 0; j < info.outputChannels; j++)
				output[j][i] = data;
		}
		return 0;
	};
	_stream.SetCallback(_callback);
	_stream.OpenStream();
	_stream.StartStream();

	_stream.OpenControlPanel();

	while (true);
}