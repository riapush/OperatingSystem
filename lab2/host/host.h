
#pragma once

#pragma once

#include <atomic>
#include <mutex>
#include <sys/types.h>
#include <semaphore.h>   
#include <bits/types/siginfo_t.h>

#include "../client/client.h"
#include "../connections/conn.h"
#include "../message_queue.h"

class Host {
private:
    std::atomic<bool> is_running = true;
    std::chrono::time_point<std::chrono::high_resolution_clock> last_message_time;
    pid_t host_pid;
    pid_t client_pid;
    sem_t *host_sem;
    sem_t *client_sem;
    static void signal_handler(int signum, siginfo_t* info, void *ptr);
    void connection_job();
    bool read_message();
    bool write_message();
    void close_connection();
    bool prepare();

    Host();
    Host(const Host&) = delete;
    Host(Host&&) = delete;
public:
    static Host& get_instance();

    ~Host() = default;
};