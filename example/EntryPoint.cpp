#include "Audijo.hpp"


#include <thread>
#include <chrono>
using namespace Audijo;

struct MyStream : public Stream<ASIO>
{
	MyStream()
		: Stream()
	{}
};


struct Channel
{
	double samples[512];
};

int main()
{

	std::vector<Channel> channels;
	channels.reserve(8);
	for (int i = 0; i < 8; i++)
		channels.emplace_back();

	Channel* channels2 = new Channel[8];
	for (int i = 0; i < 8; i++)
		channels2[i] = Channel{};

	auto start = std::chrono::system_clock::now();
	for (int i = 0; i < 100000; i++)
	{
		for (int b = 0; b < 8; b++)
		{
			channels[b].samples[2] = i * 0.003;
			channels[b].samples[3] = i * 0.002;
		}
	}
	auto end = std::chrono::system_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
	LOGL(duration.count());

	start = std::chrono::system_clock::now();
	for (int i = 0; i < 100000; i++)
	{
		for (int b = 0; b < 8; b++)
		{
			channels2[b].samples[2] = i * 0.003;
			channels2[b].samples[3] = i * 0.002;
		}
	}
	end = std::chrono::system_clock::now();
	duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
	LOGL(duration.count());
}