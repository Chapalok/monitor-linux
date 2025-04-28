#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <chrono>

#ifdef __linux__
#include <unistd.h>
#endif

#include "json.hpp"

using namespace std;
using json = nlohmann::json;

double getCpuUsage(int core_id) {
    ifstream file1("/proc/stat");
    string line;
    string target = "cpu" + to_string(core_id);
    unsigned long idle1 = 0, total1 = 0;

    while (getline(file1, line)) {
        if (line.find(target) == 0) {
            stringstream ss(line);
            string cpu;
            unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
            ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
            idle1 = idle + iowait;
            total1 = user + nice + system + idle + iowait + irq + softirq + steal;
            break;
        }
    }

    this_thread::sleep_for(chrono::milliseconds(800));


    ifstream file2("/proc/stat");
    unsigned long idle2 = 0, total2 = 0;
    while (getline(file2, line)) {
        if (line.find(target) == 0) {
            stringstream ss(line);
            string cpu;
            unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
            ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
            idle2 = idle + iowait;
            total2 = user + nice + system + idle + iowait + irq + softirq + steal;
            break;
        }
    }

    if (total2 - total1 == 0) return 0.0;
    return (double)(total2 - total1 - (idle2 - idle1)) / (total2 - total1);
}
pair<unsigned long, unsigned long> getMemoryUsage() {
    ifstream file("/proc/meminfo");
    string line;
    unsigned long total = 0, free = 0;

    while (getline(file, line)) {
        if (line.find("MemTotal:") == 0) {
            stringstream ss(line);
            string label;
            ss >> label >> total;
        }
        if (line.find("MemFree:") == 0) {
            stringstream ss(line);
            string label;
            ss >> label >> free;
        }
    }

    return { total - free, free };
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <config_path>" << endl;
        return 1;
    }

    ifstream config_file(argv[1]);
    if (!config_file.is_open()) {
        cout << "Failed to open config file!" << endl;
        return 1;
    }

    json config;
    config_file >> config;

    int period = stoi((string)config["settings"]["period"]);

    ofstream log_file;
    bool to_console = false, to_file = false;
    string log_path;

    for (auto& output : config["outputs"]) {
        if (output["type"] == "console") {
            to_console = true;
        }
        if (output["type"] == "log") {
            to_file = true;
            log_path = output["path"];
        }
    }

    if (to_file) {
        log_file.open(log_path, ios::app);
    }

    while (true) {
        for (auto& metric : config["metrics"]) {
            if (metric["type"] == "cpu") {
                for (auto& id : metric["ids"]) {
                    double usage = getCpuUsage(id);
                    if (to_console) {
                        cout << "CPU" << id << ": " << usage * 100 << "%" << endl;
                    }
                    if (to_file && log_file.is_open()) {
                        log_file << "CPU" << id << ": " << usage * 100 << "%" << endl;
                    }
                }
            }
            if (metric["type"] == "memory") {
                auto memory = getMemoryUsage();
                for (auto& spec : metric["spec"]) {
                    if (spec == "used") {
                        if (to_console) cout << "Memory Used: " << memory.first / 1024 << " MB" << endl;
                        if (to_file && log_file.is_open()) log_file << "Memory Used: " << memory.first / 1024 << " MB" << endl;
                    }
                    if (spec == "free") {
                        if (to_console) cout << "Memory Free: " << memory.second / 1024 << " MB" << endl;
                        if (to_file && log_file.is_open()) log_file << "Memory Free: " << memory.second / 1024 << " MB" << endl;
                    }
                }
            }
        }

        this_thread::sleep_for(chrono::seconds(period));
    }

    return 0;
}
