#pragma once

#include <pthread.h>
#include <mutex>
#include <string>
#include <sys/syslog.h>
#include <stdexcept>


template <typename T>
class OSSNode
{
public:
    T item;
    OSSNode<T> *next;
    OSSNode<T> *nextRemoved;

private:
    pthread_mutex_t access;
    OSSNode() = delete;

public:
    OSSNode(int &res) : item(), next(nullptr), nextRemoved(nullptr)
    {
        res = pthread_mutex_init(&access, 0);
        if (res != 0)
        {
            syslog(LOG_ERR, "Failed to init mutex, code %d", res);
        }
    }
    template <typename U>
    OSSNode(int &res, U &&item, OSSNode<T> *next = nullptr) : item(item), next(next), nextRemoved(nullptr)
    {
        res = pthread_mutex_init(&access, 0);
        if (res != 0)
        {
            syslog(LOG_ERR, "Failed to init mutex, code %d", res);
        }
    }
    ~OSSNode()
    {
        int res = pthread_mutex_destroy(&access);
        if (res != 0)
        {
            syslog(LOG_ERR, "Failed to destroy mutex, code %d", res);
        }
    }
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
};

template <typename T>
class OptimisticSyncSet
{
private:
    OSSNode<T>* head;
    OSSNode<T>* removedHead;
    int (*cmp)(const T &a, const T &b);

    bool validate(const OSSNode<T> *pred, const OSSNode<T> *curr)
    {
        OSSNode<T> *node = head;
        while (node != nullptr && cmp(node->item, pred->item) <= 0)
        {
            if (node == pred)
            {
                return pred->next == curr;
            }
            node = node->next;
        }
        return false;
    }

public:
    OptimisticSyncSet(int &res, int (*cmp)(const T &a, const T &b)) : cmp(cmp)
    {
        int localRes = 0;
        head = new OSSNode<T>(localRes);
        if (localRes != 0)
        {
            syslog(LOG_ERR, "Failed to init node, code %d", localRes);
            delete head;
            res = localRes;
            return;
        }
        removedHead = new OSSNode<T>(localRes);
        if (localRes != 0)
        {
            syslog(LOG_ERR, "Failed to init node, code %d", localRes);
            res = localRes;
            delete head;
            delete removedHead;
            return;
        }
    }

    ~OptimisticSyncSet()
    {
        deleteRemoved();
        OSSNode<T> *tmp = nullptr;
        OSSNode<T> *curr = head->next;
        while (curr != nullptr)
        {
            tmp = curr->next;
            delete curr;
            curr = tmp;
        }
        delete head;
        delete removedHead;
    }

    // Not thread safe!
    void deleteRemoved()
    {
        OSSNode<T> *tmp = nullptr;
        OSSNode<T> *curr = removedHead->nextRemoved;
        removedHead->nextRemoved = nullptr;
        while (curr != nullptr)
        {
            tmp = curr->nextRemoved;
            delete curr;
            curr = tmp;
        }
    }

    bool remove(const T &item)
    {
        while (true)
        {
            OSSNode<T> *pred = head;
            OSSNode<T> *curr = head->next;
            while (curr != nullptr && cmp(curr->item, item) < 0)
            {
                pred = curr;
                curr = curr->next;
            }
            if (curr == nullptr)
            {
                return false;
            }
            {
                std::unique_lock<OSSNode<T>> predLock(*pred);
                {                
                    std::unique_lock<OSSNode<T>> currLock(*curr);
                    if (!validate(pred, curr))
                    {
                        continue;
                    }
                    if (cmp(curr->item, item) == 0)
                    {
                        pred->next = curr->next;
                        currLock.unlock();
                        predLock.unlock();
                        {
                            std::unique_lock<OSSNode<T>> predLock(*removedHead);
                            curr->nextRemoved = removedHead->nextRemoved;
                            removedHead->nextRemoved = curr;
                        }
                        return true;
                    }
                    return false;
                }
            }
            
        }
    }

    bool contains(const T &item)
    {
        while (true)
        {
            OSSNode<T> *pred = head;
            OSSNode<T> *curr = head->next;
            while (curr != nullptr && cmp(curr->item, item) < 0)
            {
                pred = curr;
                curr = curr->next;
            }
            if (curr == nullptr)
            {
                return false;
            }
            {
                std::unique_lock<OSSNode<T>> predLock(*pred);
                {                
                    std::unique_lock<OSSNode<T>> currLock(*curr);
                    if (!validate(pred, curr))
                    {
                        continue;
                    }
                    return (cmp(curr->item, item) == 0);
                }
            }
            
        }
    }

    template <typename U>
    bool add(U &&item)
    {
        while (true)
        {
            OSSNode<T> *pred = head;
            OSSNode<T> *curr = head->next;
            while (curr != nullptr && cmp(curr->item, item) < 0)
            {
                pred = curr;
                curr = curr->next;
            }
            {
                std::unique_lock<OSSNode<T>> predLock(*pred);
                std::unique_lock<OSSNode<T>> currLock;
                if (curr != nullptr)
                {
                    currLock = std::unique_lock<OSSNode<T>>(*curr);
                }
                if (!validate(pred, curr))
                {
                    continue;
                }
                if (curr != nullptr && cmp(curr->item, item) == 0)
                {
                    return false;
                }
                int res = 0;
                OSSNode<T> *node = new OSSNode<T>(res, std::forward<U>(item), curr);
                if (res != 0)
                {
                    delete head;
                    throw std::runtime_error("Failed to init node, code " + std::to_string(res));
                }
                pred->next = node;
                return true;
            }
        }
    }
};