#pragma once

#include <string>
#include <sys/shm.h>

#include "conn.h"

class SHMConnection : public Connection {
private:
    const std::string SHM_CODE = "shm";
    const size_t MAX_SIZE = 1024;

    bool isHost;
    std::string shmFilename;

    int shm_shift;

    int fileDescr = -1; //default fd in mmap
    void *bufptr = nullptr; 

public:
    SHMConnection(pid_t pid, bool isHost) : isHost(isHost) {
        TYPE_CODE = SHM_CODE;
        this->shmFilename = "/shm_" + std::to_string(pid);
    };

    void update() {shm_shift = 0;};

    void open(size_t id, bool isHost) override;
    void read(void* buffer, size_t count) override;
    void write(void* buffer, size_t count) override;
    void close() override;

    ~SHMConnection() = default;
};