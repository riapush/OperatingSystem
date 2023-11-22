
#pragma once

#include <atomic>
#include <sys/types.h>
#include <sys/syslog.h>
#include <sys/stat.h>

class Host {
private:
    std::atomic<bool> is_running = true;
    static void signal_handler(int signum, siginfo_t* info, void *ptr);
    void connection_job();
    std::chrono::time_point<std::chrono::high_resolution_clock> last_message_time;
    bool read_message();
    bool write_message();
    void Host::connectionClose();


    Host();
    Host(const Host&) = delete;
    Host(Host&&) = delete;
public:
    static Host& getInstance();

    ~Host() = default;
};