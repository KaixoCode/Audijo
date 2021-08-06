#include "Audijo.hpp"

using namespace Audijo;

int main()
{
	Stream<Asio> _stream;

	int(*callback)(int*, int*, int&) = [](int* input, int* output, int& data) -> int { return data; };
	
	_stream.SetCallback(callback);

	auto& _devs = _stream.Devices();
	for (auto& _dev : _devs)
		LOGL(_dev.name);
}