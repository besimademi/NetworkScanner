#include <iostream>
#include <string>
#include <thread>
#include <future>
#include <vector>
#include <cstdlib>
using namespace std;

void ping(string ip)
{
	system(ip.c_str());
}
int main()
{
	vector<thread> threads;
	for (int i = 1; i < 255; i++)
	{
		string ip = "ping 192.168.0." + to_string(i);
		//async(launch::async, ping, ip);
		threads.emplace_back(ping, ip);
	}
	for (auto& i : threads)
	{
		i.join();
	}

}