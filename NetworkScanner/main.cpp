#include <iostream>
#include <string>
#include <thread>
#include <future>
#include <vector>
#include <mutex>
#include <cstdlib>
#include <cstdio>
#include <array>
#include <sstream>
#include <algorithm>
#include <iomanip>

using namespace std;

mutex mtx;

struct NetworkConfig {
    string ipAddress;
    string subnetMask;
    string networkAddress;
    string broadcastAddress;
    int prefixLength;
    int totalHosts;
};

string executeCommand(const string& cmd) {
    array<char, 128> buffer;
    string result;

    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) {
        return "";
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    _pclose(pipe);
    return result;
}

vector<int> parseIP(const string& ip) {
    vector<int> octets;
    stringstream ss(ip);
    string token;

    while (getline(ss, token, '.')) {
        octets.push_back(stoi(token));
    }

    return octets;
}

string ipToString(const vector<int>& octets) {
    if (octets.size() != 4) return "";
    return to_string(octets[0]) + "." + to_string(octets[1]) + "." +
        to_string(octets[2]) + "." + to_string(octets[3]);
}

vector<int> calculateNetworkAddress(const vector<int>& ip, const vector<int>& mask) {
    vector<int> network(4);
    for (int i = 0; i < 4; i++) {
        network[i] = ip[i] & mask[i];
    }
    return network;
}

vector<int> calculateBroadcastAddress(const vector<int>& network, const vector<int>& mask) {
    vector<int> broadcast(4);
    vector<int> invertedMask(4);

    for (int i = 0; i < 4; i++) {
        invertedMask[i] = 255 - mask[i];
        broadcast[i] = network[i] | invertedMask[i];
    }
    return broadcast;
}

int calculatePrefixLength(const vector<int>& mask) {
    int prefix = 0;
    for (int i = 0; i < 4; i++) {
        int val = mask[i];
        while (val) {
            prefix += val & 1;
            val >>= 1;
        }
    }
    return prefix;
}

bool parseNetworkConfig(NetworkConfig& config) {
    string output = executeCommand("ipconfig");

    cout << "=== Raw ipconfig output ===" << endl;
    cout << output << endl;
    cout << "===========================" << endl << endl;

    size_t ipPos = output.find("IPv4");
    if (ipPos == string::npos) {
        ipPos = output.find("IP Address");
    }

    if (ipPos != string::npos) {
        size_t colonPos = output.find(":", ipPos);
        if (colonPos != string::npos) {
            size_t lineEnd = output.find("\n", colonPos);
            string ipLine = output.substr(colonPos + 1, lineEnd - colonPos - 1);

            size_t parenPos = ipLine.find("(");
            if (parenPos != string::npos) {
                ipLine = ipLine.substr(0, parenPos);
            }

            ipLine.erase(0, ipLine.find_first_not_of(" \t"));
            ipLine.erase(ipLine.find_last_not_of(" \t\r\n") + 1);

            if (ipLine.find(".") != string::npos) {
                config.ipAddress = ipLine;
            }
        }
    }

    size_t maskPos = output.find("Subnet Mask");
    if (maskPos == string::npos) {
        maskPos = output.find("Mask");
    }

    if (maskPos != string::npos) {
        size_t colonPos = output.find(":", maskPos);
        if (colonPos != string::npos) {
            size_t lineEnd = output.find("\n", colonPos);
            string maskLine = output.substr(colonPos + 1, lineEnd - colonPos - 1);

            maskLine.erase(0, maskLine.find_first_not_of(" \t"));
            maskLine.erase(maskLine.find_last_not_of(" \t\r\n") + 1);

            if (maskLine.find(".") != string::npos) {
                config.subnetMask = maskLine;
            }
        }
    }

    if (config.ipAddress.empty() || config.subnetMask.empty()) {
        cout << "Failed to parse network configuration!" << endl;
        cout << "Detected IP: " << (config.ipAddress.empty() ? "N/A" : config.ipAddress) << endl;
        cout << "Detected Mask: " << (config.subnetMask.empty() ? "N/A" : config.subnetMask) << endl;
        return false;
    }

    vector<int> ipOctets = parseIP(config.ipAddress);
    vector<int> maskOctets = parseIP(config.subnetMask);

    vector<int> networkAddr = calculateNetworkAddress(ipOctets, maskOctets);
    vector<int> broadcastAddr = calculateBroadcastAddress(networkAddr, maskOctets);

    config.networkAddress = ipToString(networkAddr);
    config.broadcastAddress = ipToString(broadcastAddr);
    config.prefixLength = calculatePrefixLength(maskOctets);

    int hostBits = 32 - config.prefixLength;
    config.totalHosts = (1 << hostBits) - 2;  

    return true;
}

void ping(const string& ip, vector<string>& valid_ip) {
    string cmd = "ping  " + ip + " | findstr \"TTL\" >nul 2>&1";

    int result = system(cmd.c_str());

    if (result == 0) {
        lock_guard<mutex> lock(mtx);
        valid_ip.push_back(ip);
        cout << "[+] " << ip << " is reachable" << endl;
    }
}

vector<string> generateIPRange(const NetworkConfig& config) {
    vector<string> ipRange;
    vector<int> network = parseIP(config.networkAddress);
    vector<int> broadcast = parseIP(config.broadcastAddress);

    unsigned int networkInt = (network[0] << 24) | (network[1] << 16) | (network[2] << 8) | network[3];
    unsigned int broadcastInt = (broadcast[0] << 24) | (broadcast[1] << 16) | (broadcast[2] << 8) | broadcast[3];

    for (unsigned int ip = networkInt + 1; ip < broadcastInt; ip++) {
        int o1 = (ip >> 24) & 0xFF;
        int o2 = (ip >> 16) & 0xFF;
        int o3 = (ip >> 8) & 0xFF;
        int o4 = ip & 0xFF;

        ipRange.push_back(to_string(o1) + "." + to_string(o2) + "." +
            to_string(o3) + "." + to_string(o4));
    }

    return ipRange;
}

void scanNetwork(const vector<string>& ipRange, vector<string>& validIPs, int maxThreads = 50) {
    vector<thread> threads;
    vector<string> threadLocalResults;

    cout << "\n[*] Starting network scan with " << maxThreads << " threads..." << endl;
    cout << "[*] Scanning " << ipRange.size() << " IP addresses..." << endl << endl;

    for (size_t i = 0; i < ipRange.size(); i += maxThreads) {
        vector<thread> batchThreads;
        size_t batchSize = min((size_t)maxThreads, ipRange.size() - i);

        for (size_t j = 0; j < batchSize; j++) {
            batchThreads.emplace_back(ping, cref(ipRange[i + j]), ref(validIPs));
        }

        for (auto& t : batchThreads) {
            t.join();
        }
    }
}

void scanPorts(const vector<string>& validIPs) {
    cout << "\n[*] Starting port scan on " << validIPs.size() << " hosts..." << endl;

    int hostNum = 1;
    for (const auto& ip : validIPs) {
        cout << "\n[Scanning] Host " << hostNum << "/" << validIPs.size()
            << ": " << ip << endl;
        cout << string(50, '-') << endl;

        string cmd = "nmap -sS -T4 --top-ports 100 " + ip;
        system(cmd.c_str());

        hostNum++;
    }
}

void printBanner() {
    cout << R"(
==========================================
    Network Scanner - Auto IP Detection
==========================================
)" << endl;
}

int main() {
    printBanner();

    NetworkConfig config;

    cout << "[*] Detecting network configuration..." << endl << endl;

    if (!parseNetworkConfig(config)) {
        cerr << "[!] Error: Could not detect network configuration!" << endl;
        cout << "\n[*] Please enter manually:" << endl;
        cout << "IP Address: ";
        cin >> config.ipAddress;
        cout << "Subnet Mask: ";
        cin >> config.subnetMask;

        vector<int> ipOctets = parseIP(config.ipAddress);
        vector<int> maskOctets = parseIP(config.subnetMask);

        vector<int> networkAddr = calculateNetworkAddress(ipOctets, maskOctets);
        vector<int> broadcastAddr = calculateBroadcastAddress(networkAddr, maskOctets);

        config.networkAddress = ipToString(networkAddr);
        config.broadcastAddress = ipToString(broadcastAddr);
        config.prefixLength = calculatePrefixLength(maskOctets);
        config.totalHosts = (1 << (32 - config.prefixLength)) - 2;
    }

    cout << "\n========== Network Configuration ==========" << endl;
    cout << "IP Address:      " << config.ipAddress << endl;
    cout << "Subnet Mask:     " << config.subnetMask << endl;
    cout << "Network:         " << config.networkAddress << "/" << config.prefixLength << endl;
    cout << "Broadcast:       " << config.broadcastAddress << endl;
    cout << "Total Hosts:     " << config.totalHosts << endl;
    cout << "============================================" << endl << endl;

    cout << "[*] Generating IP range..." << endl;
    vector<string> ipRange = generateIPRange(config);

    cout << "[*] IP range: " << ipRange.front() << " - " << ipRange.back() << endl;
    cout << "[*] Total IPs to scan: " << ipRange.size() << endl;

    cout << "\n[*] Press Enter to start scanning or 'q' to quit...";
    char choice = cin.get();
    if (choice == 'q' || choice == 'Q') {
        return 0;
    }

    vector<string> validIPs;
    scanNetwork(ipRange, validIPs);

    cout << "\n\n========== Scan Results ==========" << endl;
    cout << "Reachable hosts found: " << validIPs.size() << endl;
    cout << "==================================" << endl;

    for (const auto& ip : validIPs) {
        cout << "  [+] " << ip << endl;
    }

    if (validIPs.empty()) {
        cout << "No hosts found!" << endl;
        return 0;
    }

    cout << "\n[*] Do you want to scan ports on these hosts? (y/n): ";
    cin >> choice;

    if (choice == 'y' || choice == 'Y') {
        scanPorts(validIPs);
    }

    cout << "\n[*] Scan complete!" << endl;

    return 0;
}
