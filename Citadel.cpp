#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#ifdef __linux__
#include <unistd.h>
#endif
#include <chrono>        // для chrono
#include <thread>        // для sleep_for
#include "json.hpp"
#include <map>

using namespace std;
using json = nlohmann::json;

// Функция для чтения информации о памяти
pair<unsigned long, unsigned long> getMemoryUsage() {
    ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        cerr << "Failed to open /proc/meminfo!" << endl;
        return { 0, 0 };
    }

    map<string, unsigned long> meminfo;
    string key;
    unsigned long value;
    string unit;

    string line;
    while (getline(file, line)) {
        stringstream ss(line);
        ss >> key >> value >> unit;
        // убираем двоеточие в конце ключа
        if (!key.empty() && key.back() == ':') {
            key.pop_back();
        }
        meminfo[key] = value;
    }

    unsigned long total = meminfo["MemTotal"];
    unsigned long free = meminfo["MemFree"];
    unsigned long used = total - free;

    return { used, free }; // возвращаем использованную и свободную память в килобайтах
}


// Вспомогательная функция для чтения времени процессора
bool readCpuTimes(int core_id, unsigned long& idle_time, unsigned long& total_time) {
    ifstream file("/proc/stat");
    if (!file.is_open()) {
        cerr << "Failed to open /proc/stat!" << endl;
        return false;
    }

    string line;
    string target = "cpu" + to_string(core_id);

    while (getline(file, line)) {
        if (line.compare(0, target.size(), target) == 0) {
            stringstream ss(line);
            string cpu_label;
            unsigned long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
            ss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;

            idle_time = idle + iowait;
            unsigned long non_idle = user + nice + system + irq + softirq + steal;
            total_time = idle_time + non_idle;
            return true;
        }
    }

    return false;
}

// Основная функция расчёта загрузки процессора
double getCpuUsage(int core_id) {
    unsigned long idle1, total1;
    unsigned long idle2, total2;

    if (!readCpuTimes(core_id, idle1, total1)) return -1.0;

    this_thread::sleep_for(chrono::milliseconds(200)); // Пауза 200 миллисекунд

    if (!readCpuTimes(core_id, idle2, total2)) return -1.0;

    unsigned long total_delta = total2 - total1;
    unsigned long idle_delta = idle2 - idle1;

    if (total_delta == 0) return 0.0;

    return (double)(total_delta - idle_delta) / total_delta;
}



int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <config_path>" << endl;
        return 1;
    }

    string config_path = argv[1];

    ifstream config_file(config_path);
    if (!config_file) {
        cerr << "Failed to open config file!" << endl;
        return 1;
    }

    json config;
    config_file >> config;

    int period = stoi((string)config["settings"]["period"]);

    // Подготовим файловый вывод, если нужен
    ofstream log_file;
    bool to_console = false;
    bool to_file = false;
    string log_path;

    for (const auto& output : config["outputs"]) {
        string type = output["type"];
        if (type == "console") {
            to_console = true;
        }
        else if (type == "log") {
            to_file = true;
            log_path = output["path"];
        }
    }

    if (to_file) {
        log_file.open(log_path, ios::app);
        if (!log_file.is_open()) {
            cerr << "Failed to open log file!" << endl;
            return 1;
        }
    }

    // Основной цикл мониторинга
    while (true) {
        stringstream output_stream;

        for (const auto& metric : config["metrics"]) {
            string type = metric["type"];

            if (type == "cpu") {
                for (const auto& core_id : metric["ids"]) {
                    int id = core_id;
                    double usage = getCpuUsage(id);
                    if (usage >= 0) {
                        output_stream << "CPU Core " << id << ": " << usage * 100 << "%\n";
                    }
                    else {
                        output_stream << "CPU Core " << id << ": Error reading usage\n";
                    }
                }
            }
            else if (type == "memory") {
                auto memory = getMemoryUsage();
                for (const auto& spec : metric["spec"]) {
                    string spec_name = spec;
                    if (spec_name == "used") {
                        output_stream << "Memory Used: " << memory.first / 1024 << " MB\n";
                    }
                    else if (spec_name == "free") {
                        output_stream << "Memory Free: " << memory.second / 1024 << " MB\n";
                    }
                }
            }
        }

        string result = output_stream.str();

        // Вывод результатов
        if (to_console) {
            cout << "----- Metrics Snapshot -----" << endl;
            cout << result;
            cout << "-----------------------------" << endl;
        }

        if (to_file) {
            log_file << "----- Metrics Snapshot -----" << endl;
            log_file << result;
            log_file << "-----------------------------" << endl;
            log_file.flush(); // принудительно записать в файл
        }

        // Пауза перед следующим замером
        this_thread::sleep_for(chrono::seconds(period));
    }

    return 0;
}

