#include <memory>
#include <string>
#include <sys/types.h>

class Connection {
protected:
    std::string TYPE_CODE;
public:
    static std::unique_ptr<Connection> create(pid_t pid, bool isHost);
    
    //for shm and mmap
    virtual void update() = 0;

    virtual void open(size_t id, bool host) = 0;
    virtual void read(void* buffer, size_t count) = 0;
    virtual void write(void* buffer, size_t count) = 0;
    virtual void close() = 0;
    
    std::string getConnectionCode() { return TYPE_CODE; };
};