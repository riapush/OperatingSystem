#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <stdexcept>
#include "conn_mmap.h"

std::unique_ptr<Connection> Connection::create(pid_t pid, bool isHost) {
    return std::make_unique<MmapConnection>(pid, isHost);
}
void* MmapConnection::clientBuf = nullptr;
void* MmapConnection::hostBuf = nullptr;

void MmapConnection::open(size_t id, bool isHost) {
    if(isHost){
        clientBuf = mmap(nullptr, MAX_SIZE, PROT_READ | PROT_WRITE, 
            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (clientBuf == MAP_FAILED) {
            throw std::runtime_error("ERROR: unable to get shared memory address");
        }
        hostBuf = mmap(nullptr, MAX_SIZE, PROT_READ | PROT_WRITE, 
            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (hostBuf == MAP_FAILED) {
            throw std::runtime_error("ERROR: unable to get shared memory address");
        }
    }
}

void MmapConnection::read(void* buffer, size_t count) {
    if(isHost){
        if (count + mmap_shift > MAX_SIZE)
            throw std::invalid_argument("ERROR: read error");
        memcpy(buffer, ((char *)clientBuf + mmap_shift), count);
    }
    else{
        if (count + mmap_shift > MAX_SIZE)
            throw std::invalid_argument("ERROR: read error");
        memcpy(buffer, ((char *)hostBuf + mmap_shift), count);
    }
    mmap_shift += count;
}

void MmapConnection::write(void* buffer, size_t count) {
    if(isHost){
        if (count + mmap_shift > MAX_SIZE)
            throw std::invalid_argument("ERROR: write error");
        memcpy(((char *)hostBuf + mmap_shift), buffer, count);
    }
    else {
        if (count + mmap_shift > MAX_SIZE)
            throw std::invalid_argument("ERROR: write error");
        memcpy(((char *)clientBuf + mmap_shift), buffer, count);
    }
    mmap_shift += count;
}

void MmapConnection::close() {
    munmap(clientBuf, MAX_SIZE);
    munmap(hostBuf, MAX_SIZE);
}