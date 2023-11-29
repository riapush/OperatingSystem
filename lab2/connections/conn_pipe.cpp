#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <unordered_map>

#include <stdexcept>

#include "conn_pipe.h"
std::unordered_map<std::string, PipePair> PipeConnection::pipeDict = std::unordered_map<std::string, PipePair>();

std::unique_ptr<Connection> Connection::create(pid_t pid, bool isHost) {
    return std::make_unique<PipeConnection>(pid, isHost);
}

void PipeConnection::open(size_t id, bool isHost) {
    auto pipePairIterator = pipeDict.find(pipeFilename);
        if(pipePairIterator == pipeDict.end()) {
            int tmp[2];
            if (pipe(tmp) < 0) {
                throw("ERROR: failed to create pipe");
            }
            pipePair.hostDescr._read = tmp[0];
            pipePair.hostDescr._write = tmp[1];
            if (pipe(tmp) < 0){
                throw("ERROR: failed to create pipe");
            }
            pipePair.clientDescr._read = tmp[0];
            pipePair.clientDescr._write = tmp[1];
            pipeDict[pipeFilename] = pipePair;
        }
        else 
            pipePair = pipePairIterator->second;

    if(isHost){
        readDescr = pipePair.clientDescr._read;
        writeDescr = pipePair.hostDescr._write;
    }
    else
    {
        readDescr = pipePair.hostDescr._read;
        writeDescr = pipePair.clientDescr._write;
        if (::close(pipePair.hostDescr._write) < 0 || ::close(pipePair.clientDescr._read) < 0)
        {
            throw("ERROR: failed to create pipe");
        }
    }
}

void PipeConnection::read(void* buffer, size_t count) {
    if(::read(readDescr, buffer, count) < 0)
        throw std::invalid_argument("ERROR: read error");
}

void PipeConnection::write(void* buffer, size_t count) {
    if(::write(writeDescr, buffer, count) < 0)
        throw std::invalid_argument("ERROR: write error");

}
void PipeConnection::close() {
    if (::close(readDescr) < 0 || ::close(writeDescr) < 0)
    {
        throw std::invalid_argument("ERROR: close error");
    }
}
