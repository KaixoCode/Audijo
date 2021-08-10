#include "Audijo.hpp"

using namespace Audijo;

int main()
{
	Stream<Asio> _stream;
	
	_stream.SetCallback([&](float* input, float* output) -> int {
		return 0;
	});

	StreamSettings settings;
	settings.bufferSize = 512;
	settings.input.deviceId = 0;
	settings.input.channels = 2;
	settings.output.deviceId = 0;
	settings.output.channels = 2;
	settings.sampleRate = 48000;
	_stream.OpenStream(settings);

	float a = 1;
	double va = 2;
	_stream.m_Api->m_Callback->Call(&a, &a, &va);

	auto& _devs = _stream.Devices();
	for (auto& _dev : _devs)
	{
		LOGL("name:    " << _dev.name);
		LOGL("id:      " << _dev.id);
		LOGL("in:      " << _dev.inputChannels);
		LOGL("out:     " << _dev.outputChannels);
		LOGL("srates:  ");
		for (auto& i : _dev.sampleRates)
			LOG(i << ", ");
		LOGL("");
	}
}