#include "tests.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <unistd.h>
#include <chrono>
#include <cstring>
#include <pthread.h>
#include <sys/syslog.h>
#include <iomanip>
#include "coarse_grained_sync_set.h"
#include "optimistic_sync_set.h"


int main(int argc, char **argv)
{
    SetTests tests;
    if (tests.parseArgs(argc, argv) != EXIT_SUCCESS)
    {
        std::cout << "Failed to parse command arguments? see help below\n";
        tests.printHelp();
        return EXIT_FAILURE;
    }
    tests.printParams();
    std::cout << "Starting funcionality tests.." << std::endl;
    if (tests.functionalityTests() != EXIT_SUCCESS)
    {
        std::cout << "Failed funcionality tests!" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Passed funcionality tests" << std::endl;

    std::cout << "Starting timed tests.." << std::endl;
    if (tests.timedTests() != EXIT_SUCCESS)
    {
        std::cout << "Failed timed tests!" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Passed timed tests" << std::endl;
    return EXIT_SUCCESS;
}

std::vector<int> getInitialSample(int n)
{
    std::vector<int> sample;
    sample.reserve(n);
    for (int i = 0; i < n; ++i)
    {
        sample.push_back(i);
    }
    return sample;
}

std::vector<std::vector<int>> getPartition(std::vector<int> &sample, int partsCount, std::default_random_engine &rng,
                                           bool randomPartition, bool randomOrder = false)
{
    int size = sample.size();
    auto partition = std::vector<std::vector<int>>();
    partition.resize(partsCount);
    auto indexes = std::vector<int>();
    indexes.resize(size);
    for (int j = 0; j < size; ++j)
    {
        indexes[j] = j % partsCount;
    }
    if (randomPartition)
    {
        std::shuffle(indexes.begin(), indexes.end(), rng);
    }
    for (auto &part : partition)
    {
        part.reserve(size / partsCount + 1);
    }
    for (int j = 0; j < size; ++j)
    {
        int i = indexes[j];
        partition[i].push_back(sample[j]);
    }
    if (randomOrder)
    {
        for (auto &part : partition)
        {
            std::shuffle(part.begin(), part.end(), rng);
        }
    }
    return partition;
}

int cmp(const int &a, const int &b)
{
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

template <typename SET>
struct Args
{
    pthread_barrier_t *barrier;
    const std::vector<int> &sample;
    SET &set;
    int *errorCounter;
    int *successCounter;
    int *counterArray;
    Args(pthread_barrier_t *barrier, const std::vector<int> &sample, SET &set, int *errorCounter, int *successCounter, int *counterArray = nullptr)
        : barrier(barrier), sample(sample), set(set), errorCounter(errorCounter), successCounter(successCounter), counterArray(counterArray) {}
};

template <typename SET>
void *multy_write(void *arg)
{
    Args<SET> *args = static_cast<Args<SET> *>(arg);
    pthread_barrier_wait(args->barrier);

    try
    {
        for (auto i : args->sample)
        {
            if (!args->set.add(i))
            {
                std::cerr << "Failed add " << i << std::endl;
            }
            pthread_testcancel();
        }
        __sync_fetch_and_add(args->successCounter, 1);
    }
    catch(const std::exception& e)
    {
        syslog(LOG_ERR, "%s", e.what());
        __sync_fetch_and_add(args->errorCounter, 1);
    }
    pthread_exit(NULL);
}

template <typename SET>
void *multy_read(void *arg)
{
    Args<SET> *args = static_cast<Args<SET> *>(arg);
    auto hungerTime = std::chrono::milliseconds(1000);

    pthread_barrier_wait(args->barrier);
    try
    {
        for (auto i : args->sample)
        {
            if (args->counterArray == nullptr)
            {
                if (!args->set.remove(i))
                {
                    std::cerr << "Failed remove " << i << std::endl;
                }
            }
            else
            {
                auto start = std::chrono::steady_clock::now();

                while (!args->set.remove(i))
                {
                    if (std::chrono::steady_clock::now() >= start + hungerTime)
                    {
                        pthread_testcancel();
                        sched_yield();
                        start = std::chrono::steady_clock::now();
                    }
                }
                __sync_fetch_and_add(args->counterArray + i, 1);
            }
            pthread_testcancel();
        }
        __sync_fetch_and_add(args->successCounter, 1);
    }
    catch(const std::exception& e)
    {
        syslog(LOG_ERR, "%s", e.what());
        __sync_fetch_and_add(args->errorCounter, 1);
    }
    pthread_exit(NULL);
}

template <typename SET>
int SetTests::write_test(float &outTime, int repeatCount, bool randomPartition)
{
    auto rng = std::default_random_engine(0);
    auto sample = getInitialSample(sampleSize);
    float totalDuration = 0.0F;
    for (int c = 0; c < repeatCount; ++c)
    {
        int res = EXIT_SUCCESS;
        SET set(res, &cmp);
        if (res != EXIT_SUCCESS)
        {
            syslog(LOG_ERR, "Failed to init set");
            return EXIT_FAILURE;
        }
        if (randomOrder)
        {
            std::shuffle(sample.begin(), sample.end(), rng);
        }
        auto partition = getPartition(sample, writeTestThreadsCount, rng, randomPartition);
        std::vector<pthread_t> threads;
        std::vector<Args<SET>> args;
        args.reserve(writeTestThreadsCount);
        threads.reserve(writeTestThreadsCount);
        pthread_barrier_t barrier;
        pthread_barrier_init(&barrier, NULL, writeTestThreadsCount + 1);
        int errorCounter = 0;
        int successCounter = 0;
        for (int i = 0; i < writeTestThreadsCount; ++i)
        {
            args.push_back(Args<SET>(&barrier, partition[i], set, &errorCounter, &successCounter));
            pthread_t thread;
            pthread_create(&thread, NULL, &multy_write<SET>, &args[i]);
            threads.push_back(thread);
        }
        auto start = std::chrono::steady_clock::now();
        pthread_barrier_wait(&barrier);
        while (errorCounter == 0 && successCounter < writeTestThreadsCount)
        {
            sched_yield();
        }
        if (errorCounter != 0)
        {
            for (auto thread : threads)
            {
                pthread_cancel(thread);
                pthread_detach(thread);
            }
            pthread_barrier_destroy(&barrier);
            return EXIT_FAILURE;
        }
        for (auto thread : threads)
        {
            void *ret;
            pthread_join(thread, &ret);
        }
        pthread_barrier_destroy(&barrier);
        auto end = std::chrono::steady_clock::now();
        totalDuration += static_cast<float>((end - start).count()) * 1e-9F / repeatCount;
        for (auto i : sample)
        {
            if (!set.contains(i))
            {
                std::cerr << "Missig: " << i << std::endl;
            }
        }
    }
    outTime = totalDuration;
    return EXIT_SUCCESS;
}

template <typename SET>
int SetTests::read_test(float &outTime, int repeatCount, bool randomPartition)
{
    auto rng = std::default_random_engine(0);
    auto sample = getInitialSample(sampleSize);
    float totalDuration = 0.0F;
    for (int c = 0; c < repeatCount; ++c)
    {
        int res = EXIT_SUCCESS;
        SET set(res, &cmp);
        if (res != EXIT_SUCCESS)
        {
            syslog(LOG_ERR, "Failed to init set");
            return EXIT_FAILURE;
        }
        if (randomOrder)
        {
            std::shuffle(sample.begin(), sample.end(), rng);
        }
        auto partition = getPartition(sample, readTestThreadsCount, rng, randomPartition);
        std::vector<pthread_t> threads;
        std::vector<Args<SET>> args;
        args.reserve(readTestThreadsCount);
        threads.reserve(readTestThreadsCount);
        pthread_barrier_t barrier;
        pthread_barrier_init(&barrier, NULL, readTestThreadsCount + 1);
        int errorCounter = 0;
        int successCounter = 0;
        for (int i = 0; i < readTestThreadsCount; ++i)
        {
            args.push_back(Args<SET>(&barrier, partition[i], set, &errorCounter, &successCounter));
            pthread_t thread;
            pthread_create(&thread, NULL, &multy_read<SET>, &args[i]);
            threads.push_back(thread);
        }
        for (auto i : sample)
        {
            if (!set.add(i))
            {
                std::cerr << "Failed add " << i << std::endl;
            }
        }
        auto start = std::chrono::steady_clock::now();
        pthread_barrier_wait(&barrier);
        while (errorCounter == 0 && successCounter < readTestThreadsCount)
        {
            sched_yield();
        }
        if (errorCounter != 0)
        {
            for (auto thread : threads)
            {
                pthread_cancel(thread);
                pthread_detach(thread);
            }
            pthread_barrier_destroy(&barrier);
            return EXIT_FAILURE;
        }
        for (auto thread : threads)
        {
            void *ret;
            pthread_join(thread, &ret);
        }
        pthread_barrier_destroy(&barrier);
        auto end = std::chrono::steady_clock::now();

        totalDuration += static_cast<float>((end - start).count()) * 1e-9F / repeatCount;

        for (auto i : sample)
        {
            if (set.contains(i))
            {
                std::cerr << "Not empty: " << i << std::endl;
            }
        }
    }
    outTime = totalDuration;
    return EXIT_SUCCESS;
}

template <typename SET>
int SetTests::read_write_test(float &outTime, int writeThreadsCount, int readThreadsCount, int repeatCount, bool randomPartition)
{
    auto rng = std::default_random_engine(0);
    int *counterArray = new int[sampleSize];
    auto sample = getInitialSample(sampleSize);
    float totalDuration = 0.0F;
    for (int c = 0; c < repeatCount; ++c)
    {
        int res = EXIT_SUCCESS;
        SET set(res, &cmp);
        if (res != EXIT_SUCCESS)
        {
            syslog(LOG_ERR, "Failed to init set");
            delete[] counterArray;
            return EXIT_FAILURE;
        }
        memset(counterArray, 0, sizeof(int) * sampleSize);
        if (randomOrder)
        {
            std::shuffle(sample.begin(), sample.end(), rng);
        }
        auto readPartition = getPartition(sample, readThreadsCount, rng, randomPartition);
        auto writePartition = getPartition(sample, writeThreadsCount, rng, randomPartition);
        std::vector<pthread_t> readThreads;
        std::vector<pthread_t> writeThreads;
        std::vector<Args<SET>> readArgs;
        std::vector<Args<SET>> writeArgs;
        readArgs.reserve(readThreadsCount);
        writeArgs.reserve(writeThreadsCount);
        readThreads.reserve(readThreadsCount);
        writeThreads.reserve(writeThreadsCount);
        pthread_barrier_t barrier;
        pthread_barrier_init(&barrier, NULL, readThreadsCount + writeThreadsCount + 1);
        int errorCounter = 0;
        int successCounter = 0;
        for (int i = 0; i < readThreadsCount; ++i)
        {
            readArgs.push_back(Args<SET>(&barrier, readPartition[i], set, &errorCounter, &successCounter, counterArray));
            pthread_t thread;
            pthread_create(&thread, NULL, &multy_read<SET>, &readArgs[i]);
            readThreads.push_back(thread);
        }
        for (int i = 0; i < writeThreadsCount; ++i)
        {
            writeArgs.push_back(Args<SET>(&barrier, writePartition[i], set, &errorCounter, &successCounter));
            pthread_t thread;
            pthread_create(&thread, NULL, &multy_write<SET>, &writeArgs[i]);
            writeThreads.push_back(thread);
        }
        auto start = std::chrono::steady_clock::now();
        pthread_barrier_wait(&barrier);
        while (errorCounter == 0 && successCounter < readThreadsCount + writeThreadsCount)
        {
            sched_yield();
        }
        if (errorCounter != 0)
        {
            for (auto thread : writeThreads)
            {
                pthread_cancel(thread);
                pthread_detach(thread);
            }
            for (auto thread : readThreads)
            {
                pthread_cancel(thread);
                pthread_detach(thread);
            }
            pthread_barrier_destroy(&barrier);
            delete[] counterArray;
            return EXIT_FAILURE;
        }
        for (auto thread : writeThreads)
        {
            void *ret;
            pthread_tryjoin_np(thread, &ret);
        }
        for (auto thread : readThreads)
        {
            void *ret;
            pthread_join(thread, &ret);
        }
        pthread_barrier_destroy(&barrier);
        auto end = std::chrono::steady_clock::now();
        totalDuration += static_cast<float>((end - start).count()) * 1e-9F / repeatCount;
        for (int i = 0; i < sampleSize; ++i)
        {
            if (counterArray[i] != 1)
            {
                std::cerr << "Error: " << i << " read " << counterArray[i] << " times!" << std::endl;
            }
        }
    }
    delete[] counterArray;
    outTime = totalDuration;
    return EXIT_SUCCESS;
}

void SetTests::printTableHeader()
{
    std::cout << std::setw(5) << std::right << "" << "|";
    std::cout << std::setw(5) << std::right << "" << "|";
    std::cout << std::setw(20) << std::right << "CoarseGrained" << " ";
    std::cout << std::setw(10) << std::right << "" << "|";
    std::cout << std::setw(20) << std::right << "Optimistic" << " ";
    std::cout << std::setw(10) << std::right << "" << "|";
    std::cout << std::endl;
    std::cout << std::setw(5) << std::right << "write" << "|";
    std::cout << std::setw(5) << std::right << "read" << "|";
    std::cout << std::setw(10) << std::right << "random" << std::setw(5) << "" << "|";
    std::cout << std::setw(10) << std::right << "even" << std::setw(5) << "" << "|";
    std::cout << std::setw(10) << std::right << "random" << std::setw(5) << "" << "|";
    std::cout << std::setw(10) << std::right << "even" << std::setw(5) << "" << "|";
    std::cout << std::endl;
    std::cout << std::setfill('_');
    std::cout << std::setw(5) << std::right << "" << "|";
    std::cout << std::setw(5) << std::right << "" << "|";
    std::cout << std::setw(15) << std::right << "" << "|";
    std::cout << std::setw(15) << std::right << "" << "|";
    std::cout << std::setw(15) << std::right << "" << "|";
    std::cout << std::setw(15) << std::right << "" << "|";
    std::cout << std::endl;
    std::cout << std::setfill(' ');
}

void SetTests::printTableRow(const TableRow &row)
{
    std::cout << std::setw(5) << std::right << row.writeThreadsCount << "|";
    std::cout << std::setw(5) << std::right << row.readThreadsCount << "|";
    std::cout << std::setw(15) << std::right << std::scientific << row.resRandomCoarseGrained << "|";
    std::cout << std::setw(15) << std::right << std::scientific << row.resEvenCoarseGrained << "|";
    std::cout << std::setw(15) << std::right << std::scientific << row.resRandomOptimistic << "|";
    std::cout << std::setw(15) << std::right << std::scientific << row.resEvenOptimistic << "|";
    std::cout << std::endl;
}

int SetTests::timedTests()
{
    std::vector<TableRow> table;
    printTableHeader();
    TableRow row;
    row.writeThreadsCount = writeTestThreadsCount;
    row.readThreadsCount = 0;
    if (EXIT_SUCCESS != write_test<CoarseGrainedSyncSet<int>>(row.resRandomCoarseGrained, repeatCount, true))
    {
        std::cout << "CoarseGrained write test failed" << std::endl;
        return EXIT_FAILURE;
    }
    if (EXIT_SUCCESS != write_test<CoarseGrainedSyncSet<int>>(row.resEvenCoarseGrained, repeatCount, false))
    {
        std::cout << "CoarseGrained write test failed" << std::endl;
        return EXIT_FAILURE;
    }
    if (EXIT_SUCCESS != write_test<OptimisticSyncSet<int>>(row.resRandomOptimistic, repeatCount, true))
    {
        std::cout << "Optimistic write test failed" << std::endl;
        return EXIT_FAILURE;
    }
    if (EXIT_SUCCESS != write_test<OptimisticSyncSet<int>>(row.resEvenOptimistic, repeatCount, false))
    {
        std::cout << "Optimistic write test failed" << std::endl;
        return EXIT_FAILURE;
    }
    printTableRow(row);
    row.writeThreadsCount = 0;
    row.readThreadsCount = readTestThreadsCount;
    if (EXIT_SUCCESS != read_test<CoarseGrainedSyncSet<int>>(row.resRandomCoarseGrained, repeatCount, true))
    {
        std::cout << "CoarseGrained read test failed" << std::endl;
        return EXIT_FAILURE;
    }
    if (EXIT_SUCCESS != read_test<CoarseGrainedSyncSet<int>>(row.resEvenCoarseGrained, repeatCount, false))
    {
        std::cout << "CoarseGrained read test failed" << std::endl;
        return EXIT_FAILURE;
    }
    if (EXIT_SUCCESS != read_test<OptimisticSyncSet<int>>(row.resRandomOptimistic, repeatCount, true))
    {
        std::cout << "Optimistic read test failed" << std::endl;
        return EXIT_FAILURE;
    }
    if (EXIT_SUCCESS != read_test<OptimisticSyncSet<int>>(row.resEvenOptimistic, repeatCount, false))
    {
        std::cout << "Optimistic read test failed" << std::endl;
        return EXIT_FAILURE;
    }
    printTableRow(row);
    for (int sum = 2; sum <= threadsCountLimit; ++sum)
    {
        for (int writeThreadsCount = 1; writeThreadsCount <= sum - 1; ++writeThreadsCount)
        {
            int readThreadsCount = sum - writeThreadsCount;
            row.writeThreadsCount = writeThreadsCount;
            row.readThreadsCount = readThreadsCount;
            if (EXIT_SUCCESS != read_write_test<CoarseGrainedSyncSet<int>>(row.resRandomCoarseGrained, writeThreadsCount, readThreadsCount,
                                                                repeatCount, true))
            {
                std::cout << "CoarseGrained time test failed" << std::endl;
                return EXIT_FAILURE;
            }
            if (EXIT_SUCCESS != read_write_test<CoarseGrainedSyncSet<int>>(row.resEvenCoarseGrained, writeThreadsCount, readThreadsCount,
                                                                repeatCount, false))
            {
                std::cout << "CoarseGrained time test failed" << std::endl;
                return EXIT_FAILURE;
            }
            if (EXIT_SUCCESS != read_write_test<OptimisticSyncSet<int>>(row.resRandomOptimistic, writeThreadsCount, readThreadsCount,
                                                            repeatCount, true))
            {
                std::cout << "Optimistic time test failed" << std::endl;
                return EXIT_FAILURE;
            }
            if (EXIT_SUCCESS != read_write_test<OptimisticSyncSet<int>>(row.resEvenOptimistic, writeThreadsCount, readThreadsCount,
                                                            repeatCount, false))
            {
                std::cout << "Optimistic time test failed" << std::endl;
                return EXIT_FAILURE;
            }
            printTableRow(row);
        }
    }
    return EXIT_SUCCESS;
}

int SetTests::functionalityTests()
{
    float tmpTime = 0;
    if (EXIT_SUCCESS != write_test<CoarseGrainedSyncSet<int>>(tmpTime, 1, true))
    {
        std::cout << "CoarseGrained write test failed" << std::endl;
        return EXIT_FAILURE;
    }
    if (EXIT_SUCCESS != write_test<OptimisticSyncSet<int>>(tmpTime, 1, true))
    {
        std::cout << "Optimistic write test failed" << std::endl;
        return EXIT_FAILURE;
    }
    if (EXIT_SUCCESS != read_test<CoarseGrainedSyncSet<int>>(tmpTime, 1, true))
    {
        std::cout << "CoarseGrained read test failed" << std::endl;
        return EXIT_FAILURE;
    }
    if (EXIT_SUCCESS != read_test<OptimisticSyncSet<int>>(tmpTime, 1, true))
    {
        std::cout << "Optimistic read test failed" << std::endl;
        return EXIT_FAILURE;
    }
    for (int sum = 2; sum <= threadsCountLimit; ++sum)
    {
        for (int writeThreadsCount = 1; writeThreadsCount <= sum - 1; ++writeThreadsCount)
        {
            int readThreadsCount = sum - writeThreadsCount;
            if (EXIT_SUCCESS != read_write_test<CoarseGrainedSyncSet<int>>(tmpTime, writeThreadsCount, readThreadsCount,
                                                                1, true))
            {
                std::cout << "CoarseGrained time test failed" << std::endl;
                return EXIT_FAILURE;
            }
            if (EXIT_SUCCESS != read_write_test<OptimisticSyncSet<int>>(tmpTime, writeThreadsCount, readThreadsCount,
                                                                1, true))
            {
                std::cout << "Optimistic time test failed" << std::endl;
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}


void SetTests::printParams()
{
    std::cout << "order = " << (randomOrder ? "random" : "continuous") << std::endl;
    std::cout << "threads-count-limit = " << threadsCountLimit << std::endl;
    std::cout << "sample-size = " << sampleSize << std::endl;
    std::cout << "write-threads-count = " << writeTestThreadsCount << std::endl;
    std::cout << "read-threads-count = " << readTestThreadsCount << std::endl;
    std::cout << "repeat-count = " << repeatCount << std::endl;
}

void SetTests::printHelp()
{
    std::cout << "Command usage:\n";
    std::cout << "set_tests [--help] [--order (random|continuous)] [--threads-count-limit <number >= 2>]\n"
    << "\t\t[--sample-size <number >= 0>] [--write-threads-count <number >= 1>]\n"
    << "\t\t[--read-threads-count <number >= 1>] [--repeat-count <number >= 1>] " << std::endl;
}
int SetTests::parseArgs(int argc, char **argv)
{
    for (int currArg = 1; currArg < argc; ++currArg)
    {
        if (strcmp(argv[currArg], "--help") == 0)
        {
            printHelp();
            exit(EXIT_SUCCESS);
        }
        else if (strcmp(argv[currArg], "--order") == 0)
        {
            ++currArg;
            if (currArg >= argc)
            {
                return EXIT_FAILURE;
            }
            if (strcmp(argv[currArg], "random") == 0)
            {
                randomOrder = true;
            }
            else if (strcmp(argv[currArg], "continuous") == 0)
            {
                randomOrder = false;
            }
            else
            {
                return EXIT_FAILURE;
            }
        }
        else if (strcmp(argv[currArg], "--threads-count-limit") == 0)
        {
            ++currArg;
            if (currArg >= argc)
            {
                return EXIT_FAILURE;
            }
            int tmp = threadsCountLimit;
            try
            {
                tmp = std::stoi(argv[currArg]);
            }
            catch (std::exception const &e)
            {
                return EXIT_FAILURE;
            }
            if (tmp < 2)
            {
                return EXIT_FAILURE;
            }
            if(tmp <= static_cast<int>(std::thread::hardware_concurrency()))
            {
                threadsCountLimit = tmp;
            }
            else
            {
                threadsCountLimit = std::thread::hardware_concurrency();
            }
        }
        else if (strcmp(argv[currArg], "--sample-size") == 0)
        {
            ++currArg;
            if (currArg >= argc)
            {
                return EXIT_FAILURE;
            }
            int tmp = sampleSize;
            try
            {
                tmp = std::stoi(argv[currArg]);
            }
            catch (std::exception const &e)
            {
                return EXIT_FAILURE;
            }
            if (tmp < 0)
            {
                return EXIT_FAILURE;
            }
            sampleSize = tmp;
        }
        else if (strcmp(argv[currArg], "--write-threads-count") == 0)
        {
            ++currArg;
            if (currArg >= argc)
            {
                return EXIT_FAILURE;
            }
            int tmp = writeTestThreadsCount;
            try
            {
                tmp = std::stoi(argv[currArg]);
            }
            catch (std::exception const &e)
            {
                return EXIT_FAILURE;
            }
            if (tmp < 1)
            {
                return EXIT_FAILURE;
            }
            if(tmp <= static_cast<int>(std::thread::hardware_concurrency()))
            {
                writeTestThreadsCount = tmp;
            }
            else
            {
                writeTestThreadsCount = std::thread::hardware_concurrency();
            }
        }
        else if (strcmp(argv[currArg], "--read-threads-count") == 0)
        {
            ++currArg;
            if (currArg >= argc)
            {
                return EXIT_FAILURE;
            }
            int tmp = readTestThreadsCount;
            try
            {
                tmp = std::stoi(argv[currArg]);
            }
            catch (std::exception const &e)
            {
                return EXIT_FAILURE;
            }
            if (tmp < 1)
            {
                return EXIT_FAILURE;
            }
            if(tmp <= static_cast<int>(std::thread::hardware_concurrency()))
            {
                readTestThreadsCount = tmp;
            }
            else
            {
                readTestThreadsCount = std::thread::hardware_concurrency();
            }
        }
        else if (strcmp(argv[currArg], "--repeat-count") == 0)
        {
            ++currArg;
            if (currArg >= argc)
            {
                return EXIT_FAILURE;
            }
            int tmp = repeatCount;
            try
            {
                tmp = std::stoi(argv[currArg]);
            }
            catch (std::exception const &e)
            {
                return EXIT_FAILURE;
            }
            if (tmp < 1)
            {
                return EXIT_FAILURE;
            }
            repeatCount = tmp;
        }
        else
        {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}