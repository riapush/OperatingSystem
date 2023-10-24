#ifndef OPERATINGSYSTEM_DAEMON_H
#define OPERATINGSYSTEM_DAEMON_H

#include "Daemon.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <filesystem>

#include <csignal>
#include <ctime>

#include <sys/stat.h>
#include <syslog.h>
#include <tuple>
#include <unistd.h>

const int DEFAULT_INTERVAL = 3;

class Daemon {
private:
    static Daemon instance;
    int interval;
    const std::string pid_path = std::filesystem::absolute("mydaemon.pid");
    std::filesystem::path config_file_;
    std::filesystem::path folder1;
    std::filesystem::path folder2;

    Daemon() : interval(DEFAULT_INTERVAL) {};
    void set_config_file(std::string config_file_path);
    void read_config();
    void log_folder_contents();
    void stop_daemon();
    void make_daemon();

public:
    Daemon(const Daemon&) = delete;
    Daemon(Daemon&&) = delete;
    Daemon& operator=(const Daemon&) = delete;
    Daemon& operator=(Daemon&&) = delete;

    static Daemon& getInstance() {
        static Daemon instance;
        return instance;
    }

    void start(std::string config_file_path);
};


#endif //OPERATINGSYSTEM_DAEMON_H