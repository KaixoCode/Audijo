#include "Audijo.hpp"

using namespace Audijo;

int main()
{
	Stream<Asio> _stream;
	
	_stream.SetCallback([&](float* input, float* output) -> int {
		return 0;
	});

	_stream.OpenStream();
	_stream.StartStream();

	float a = 1;
	double va = 2;
	_stream.m_Api->m_Callback->Call(&a, &a, &va);

	auto& _devs = _stream.Devices();
	for (auto& _dev : _devs)
		LOGL(_dev.name);
}