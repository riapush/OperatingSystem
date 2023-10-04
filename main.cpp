#include <iostream>

const int DEFAULT_INTERVAL = 60;

class Daemon {
private:
    static Daemon instance;
    int interval;

    Daemon() : interval(DEFAULT_INTERVAL) {}

public:
    static Daemon& getInstance() {
        return instance;
    }

    void setInterval(int newInterval) {
        interval = newInterval;
    }

};

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
