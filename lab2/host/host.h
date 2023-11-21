
#pragma once

#include <atomic>
#include <mutex>
#include <sys/types.h>
#include <semaphore.h>   
#include <bits/types/siginfo_t.h>

#include "../client/client.h"
#include "../gui/gui.h"
#include "../connections/conn.h"
#include "../queueMsg.h"

class Host {
private:
    std::atomic<bool> is_running = true;
    static void signal_handler(int signum, siginfo_t* info, void *ptr);

    Host();
    Host(const Host&) = delete;
    Host(Host&&) = delete;
public:
    static Host& getInstance();

    void run();
    void stop();

    ~Host() = default;
};