#include <sys/shm.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

#include "conn_shm.h"

std::unique_ptr<Connection> Connection::create(pid_t pid, bool isHost) {
    return std::make_unique<SHMConnection>(pid, isHost);
}

void SHMConnection::open(size_t id, bool isHost) {
    int oflag = isHost ? O_CREAT | O_RDWR | O_EXCL : O_RDWR;
    fileDescr = shm_open(shmFilename.c_str(), oflag, 0666); //0666 mode - three level read-open mode
    if (fileDescr == -1)
        throw std::invalid_argument("ERROR: object creation error");
    ftruncate(fileDescr, MAX_SIZE);
    bufptr = mmap(0, MAX_SIZE, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, fileDescr, 0);
    if (bufptr == MAP_FAILED) {
        ::close(fileDescr);
        if (isHost)
            shm_unlink(shmFilename.c_str());
        throw std::runtime_error("ERROR: object mapping error");
    }
}

void SHMConnection::read(void* buffer, size_t count) {
    if (count + shm_shift > MAX_SIZE)
        throw std::invalid_argument("ERROR: read error");
    memcpy(buffer, ((char *)bufptr + shm_shift), count);
    shm_shift += count;
}

void SHMConnection::write(void* buffer, size_t count) {
    if (count + shm_shift > MAX_SIZE)
        throw std::invalid_argument("ERROR: write error");
    memcpy(((char *)bufptr + shm_shift), buffer, count);
    shm_shift += count;
}

void SHMConnection::close() {
    munmap(bufptr, MAX_SIZE);
    ::close(fileDescr);
    if (isHost)
        shm_unlink(shmFilename.c_str());
}