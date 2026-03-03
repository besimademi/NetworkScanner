#include <iostream>
#include <string>
#include <thread>
#include <future>
#include <cstdlib>
using namespace std;

void ping(string ip)
{
	system(ip.c_str());
}
int main()
{
	for (int i = 1; i < 255; i++)
	{
		string ip = "ping 192.168.0." + to_string(i);
		async(launch::async, ping, ip);

	}
}