#pragma once

#include <string>
#include <sys/shm.h>

#include "conn.h"

class MmapConnection : public Connection {
private:
    const std::string MMAP_CODE = "mmap";
    const size_t MAX_SIZE = 1024;
    bool isHost;

    int mmap_shift;
public:
    static void* clientBuf;
    static void* hostBuf;

    MmapConnection(pid_t pid, bool isHost) : isHost(isHost) {
        TYPE_CODE = MMAP_CODE;
    };

    void update() {mmap_shift = 0;};

    void open(size_t id, bool isHost) override;
    void read(void* buffer, size_t count) override;
    void write(void* buffer, size_t count) override;
    void close() override;

    ~MmapConnection() = default;
};