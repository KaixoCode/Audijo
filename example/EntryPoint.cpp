#include "Audijo.hpp"

using namespace Audijo;

int main()
{
	Stream<Asio> _stream;
	
	auto& _devs = _stream.Devices();
	for (auto& _dev : _devs)
		LOGL(_dev.name);
}