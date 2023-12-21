#pragma once

#include <pthread.h>
#include <mutex>
#include <string>
#include <sys/syslog.h>
#include <stdexcept>


template <typename T>
struct CGSSNode
{
    T item;
    CGSSNode<T>* next;
    CGSSNode() : item(), next(nullptr) {}
    template <typename U>
    CGSSNode(U && item, CGSSNode<T>* next = nullptr) : item(item), next(next) {}
};


template <typename T>
class CoarseGrainedSyncSet
{
private:
    int (*cmp)(const T &a, const T &b);
    CGSSNode<T> head;
    struct MutexWraper
    {
        pthread_mutex_t access;
        inline void lock()
        {
            int res = pthread_mutex_lock(&access);
            if (res != 0)
            {
                throw std::runtime_error("Failed to lock mutex, code " + std::to_string(res));
            }
        }
        inline void unlock()
        {
            int res = pthread_mutex_unlock(&access);
            if (res != 0)
            {
                syslog(LOG_ERR, "Failed to unlock mutex, code %d", res);
            }
        }
    } accessWraped;
    

public:
    CoarseGrainedSyncSet(int &res, int (*cmp)(const T &a, const T &b)) : cmp(cmp), head()
    {
        int localRes = pthread_mutex_init(&accessWraped.access, 0);
        if (localRes != 0)
        {
            syslog(LOG_ERR, "Failed to init mutex, code %d", localRes);
            res = localRes;
        }
    }

    ~CoarseGrainedSyncSet()
    {
        CGSSNode<T>* tmp = nullptr; 
        CGSSNode<T>* curr = head.next; 
        while (curr != nullptr)
        {
            tmp = curr->next;
            delete curr;
            curr = tmp;
        }
        int res = pthread_mutex_destroy(&accessWraped.access);
        if (res != 0)
        {
            syslog(LOG_ERR, "Failed to destroy mutex, code %d", res);
        }
    }

    bool remove(const T &item)
    {
        std::unique_lock lk(accessWraped);
        CGSSNode<T>* pred = &head;
        CGSSNode<T>* curr = head.next; 
        while (curr != nullptr && cmp(curr->item, item) < 0)
        {
            pred = curr;
            curr = curr->next;
        }
        if (curr != nullptr && cmp(curr->item, item) == 0)
        {
            pred->next = curr->next;
            delete curr;
            return true;
        }
        return false;
    }

    bool contains(const T &item)
    {
        std::unique_lock lk(accessWraped);
        CGSSNode<T>* curr = head.next; 
        while (curr != nullptr && cmp(curr->item, item) < 0)
        {
            curr = curr->next;
        }
        if (curr != nullptr && cmp(curr->item, item) == 0)
        {
            return true;
        }
        return false;
    }

    template <typename U>
    bool add(U && item)
    {
        std::unique_lock lk(accessWraped);
        CGSSNode<T>* pred = &head;
        CGSSNode<T>* curr = head.next; 
        while (curr != nullptr && cmp(curr->item, item) < 0)
        {
            pred = curr;
            curr = curr->next;
        }
        if (curr != nullptr && cmp(curr->item, item) == 0)
        {
            return false;
        }
        CGSSNode<T>* node = new CGSSNode<T>(std::forward<U>(item), curr);
        pred->next = node;
        return true;
    }

};