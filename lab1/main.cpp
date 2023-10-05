#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <filesystem>

const int DEFAULT_INTERVAL = 3;

class Daemon {
private:
    static Daemon instance;
    int interval;

    Daemon() : interval(DEFAULT_INTERVAL) {}

public:
    static Daemon& getInstance() {
        static Daemon instance;
        return instance;
    }

    void setInterval(int newInterval) {
        interval = newInterval;
    }

    void start() {
        // Daemon logic to periodically perform actions
        while (true) {
            std::ofstream logfile("D:/Git/GitHub/OperatingSystem/lab1/folder2/hist.log", std::ios::app);
            if (logfile.is_open()) {
                std::filesystem::path folder1("D:/Git/GitHub/OperatingSystem/lab1/folder1");
                auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                logfile << "Time: " << std::ctime(&now);
                logfile << "Directory contents: \n";
                for (const auto& entry : std::filesystem::directory_iterator(folder1)) {
                    logfile << entry.path() << "\n";
                }
                logfile.close();
            } else {
                std::cout << "Unable to open hist.log for writing." << std::endl;
            }
            // Sleep for the specified interval
            std::this_thread::sleep_for(std::chrono::seconds(interval));
        }
    }

};

int main() {
    Daemon& daemon = Daemon::getInstance();
    daemon.start();
    return 0;
}
