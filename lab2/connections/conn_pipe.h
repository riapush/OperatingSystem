#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <unordered_map>

#include "conn.h"

struct PipePair{
    struct PipeDescriptors{
        int _read;
        int _write;
    };
    PipeDescriptors hostDescr;
    PipeDescriptors clientDescr;
};


class PipeConnection: public Connection {
private:
    const std::string PIPE_CODE = "pipe";
    std::string pipeFilename;
    bool isHost = false;
    PipePair pipePair;
    
    int readDescr = -1;
    int writeDescr = -1;
public:
    static std::unordered_map<std::string, PipePair> pipeDict;
    PipeConnection(pid_t pid, bool isHost): isHost(isHost) {
        TYPE_CODE = PIPE_CODE;
        this->pipeFilename = "/pipe_" + std::to_string(pid);
    }
    void update() {};

    void open(size_t id, bool isHost) override;
    void read(void* buffer, size_t count) override;
    void write(void* buffer, size_t count) override;
    void close() override;
    ~PipeConnection() = default;
};