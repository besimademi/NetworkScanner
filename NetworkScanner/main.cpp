#include <iostream>
#include <string>
#include <thread>
#include <future>
#include <vector>
#include <mutex>
#include <cstdlib>
using namespace std;
mutex mtx;
void ping(string ip, vector<string>& valid_ip)
{
	/*there was problem because before always it turn result 0 even if the host is not in the network
	so i combined 2 commands to see if it works it does*/
	string cmd = "ping " + ip + " | findstr \"TTL\" ";

	int result = system(cmd.c_str());
	if (result == 0)
	{
		//idk what mutex is chat suggested me this ( it is used to prevent multiple threads from accessing the same resource at the same time ) but i dont understnda in details bow it works
// it works
		lock_guard<mutex> lock(mtx);
		valid_ip.push_back(ip);
		cout << ip << " is reachable" << endl;
	}

}
int main()
{
	// vecotr for threads and valid ips
	vector<string> valid_ip;
	vector<thread> threads;
	for (int i = 1; i < 255; i++)
	{
		// generate all the ips
		string ip = "192.168.0." + to_string(i);
		//async(launch::async, ping, ip);
		// i commented the async cuz it was to slow
		// i am making to much threads but we will se to fix this later
		threads.emplace_back(ping, ip, ref(valid_ip));
	}

	// wait for all threads to finish
	for (auto& i : threads)
	{
		i.join();
	}

	// we show here all the valid ips that we found in the network
	cout << endl << endl << endl << endl;
	for (auto& t : valid_ip)
	{
		cout << "Ip :" << t << " is reachable" << endl;
	}


	// now lets go for the second part to see wich ports are open in the valid ips
	int i = 1;
	for (auto& t : valid_ip)
	{
		cout << "ip " << i <<"   "<<t << endl;
		string cmd = "nmap -p- " + t;//+ " | findstr \"open\" ";
		system(cmd.c_str());
		cout << endl << endl << endl; i++;
	}
}
