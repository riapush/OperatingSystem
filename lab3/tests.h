#pragma once

#include <thread>


class SetTests
{
    bool randomOrder = true;
    int threadsCountLimit = std::thread::hardware_concurrency();
    int sampleSize = 2000;
    int writeTestThreadsCount = 8;
    int readTestThreadsCount = 8;
    int repeatCount = 10;


    template <typename SET>
    int write_test(float &outTime, int repeatCount, bool randomPartition);

    template <typename SET>
    int read_test(float &outTime, int repeatCount, bool randomPartition);

    template <typename SET>
    int read_write_test(float &outTime, int writeThreadsCount, int readThreadsCount, int repeatCount, bool randomPartition);
    
    struct TableRow
    {
        int writeThreadsCount;
        int readThreadsCount;
        float resRandomCoarseGrained;
        float resEvenCoarseGrained;
        float resRandomOptimistic;
        float resEvenOptimistic;
    };

    void printTableHeader();

    void printTableRow(const TableRow &row);

public:
    void printHelp();
    void printParams();
    int parseArgs(int argc, char **argv);
    int timedTests();
    int functionalityTests();
};