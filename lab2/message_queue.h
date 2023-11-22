#pragma once

#include <queue>
#include <mutex>
#include <syslog.h>
#include "connections/AbstractConnection.h"

#define MAX_CHAR_LENGTH 300
struct Message {
    char text[MAX_CHAR_LENGTH];
};

class MessageQueue {
protected:
    std::queue<Message> q;
    mutable std::mutex mutex;
public:
    MessageQueue() = default;
    MessageQueue(const MessageQueue&) = delete;
    MessageQueue(MessageQueue&&) = delete;

    void push_message(const Message& msg){
        mutex.lock();
        q.push(msg);
        mutex.unlock();
    }

    bool pop_message(Message* msg){
        mutex.lock();
        if (q.empty()) {
            mutex.unlock();
            return false;
        }
        *msg = q.front();
        q.pop();
        mutex.unlock();
        return true;
    }

    size_t get_size(){
        size_t elements = -1;
        mutex.lock();
        elements = q.size();
        mutex.unlock();
        return elements;        
    }

    bool push_connection(Connection *conn) {
        mutex.lock();
        conn->update();

        uint size = 0;
        try {
            conn->read((void *)&size, sizeof(uint));
            if (size == 0) {
                mutex.unlock();
                return true;
            }
        }
        catch (std::exception &e) {
            syslog(LOG_ERR, "%s", e.what());
            mutex.unlock();
            return false;
        }
        syslog(LOG_ERR, "Total %u msgs!", size);

        std::string log_str; 

        Message msg;
        for (uint i = 0; i < size; i++) {
            syslog(LOG_ERR, "Reading msg #%u", i);
            msg = {0};
            try {
                conn->read((void *)&msg, sizeof(Message));

                log_str = std::string("Readed msg: ") + std::string(msg.text);
                syslog(LOG_INFO, "%s", log_str.c_str());
                q.push(msg);
            }
            catch (std::exception &e) {
                syslog(LOG_ERR, "%s", e.what());
                mutex.unlock();
                return false;
            }
        }

        mutex.unlock();
        return true;
    }

    bool pop_connection(Connection *conn) {
        mutex.lock();
    	conn->update();

        std::string log_str;
        uint size = q.size();
        conn->write((void *)&size, sizeof(uint));

        Message msg;
        while (!q.empty()) {
            msg = q.front();

            try {

                conn->write((void *)&msg, sizeof(Message));
                log_str = std::string("Written msg: ") + std::string(msg.text);
                syslog(LOG_INFO, "%s", log_str.c_str());
                q.pop();
            }
            catch (std::exception &e) {
                syslog(LOG_ERR, "%s", e.what());
                mutex.unlock();
                return false;
            }
        }

        mutex.unlock();
        return true;
    }

    ~MessageQueue() = default;
};