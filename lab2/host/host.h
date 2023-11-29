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
    std::atomic<bool> isRunning = true;

    std::unique_ptr<Connection> conn;
    pid_t host_pid;
    pid_t client_pid;
    sem_t *host_sem;
    sem_t *client_sem;

    void connection_job();

    bool prepare();
    bool read_message();
    bool write_message();
    void close_connection();

    MessageQueue messagesIn;
    MessageQueue messagesOut;
    static void signal_handler(int signum, siginfo_t* info, void *ptr);

    std::chrono::time_point<std::chrono::high_resolution_clock> lastMsgTime;
    static bool IsRun();
    static void writeWin(Message msg);

    Host();
    Host(const Host&) = delete;
    Host(Host&&) = delete;
public:
    static Host& get_instance(){
        static Host host_instance;
        return host_instance;
    }

    void run();
    void stop();

    ~Host() = default;
};
