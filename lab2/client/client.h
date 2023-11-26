#pragma once

#include <atomic>
#include <mutex>
#include <sys/types.h>
#include <semaphore.h>   
#include <bits/types/siginfo_t.h>

#include "../host/host.h"
#include "../connections/conn.h"
#include "../message_queue.h"

class Client{
private:
    std::atomic<bool> isRunning = true;

    std::unique_ptr<Connection> conn;
    pid_t host_pid;
    pid_t client_pid;
    sem_t *host_sem;
    sem_t *client_sem;

    void connection_job();

    bool prepare(const pid_t& host_pid);
    bool read_message();
    bool write_message();
    void close_connection();

    MessageQueue messagesIn;
    MessageQueue messagesOut;
    static void signal_handler(int signum, siginfo_t* info, void *ptr);

    static bool IsRun();
    static void writeWin(Message msg);

    Client();
    Client(const Client&) = delete;
    Client(Client&&) = delete;
public:
    static Client& get_instance(){
        static Client client_instance;
        return client_instance;
    }
    bool init(const pid_t& host_pid);
    void run();
    void stop();

    ~Client() = default;
};