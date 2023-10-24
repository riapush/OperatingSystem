#include "Daemon.h"


int main() {
    Daemon& daemon = Daemon::getInstance();
    daemon.start("/home/riapush/os_labs/lab1/config.txt");
    return 0;
}

/*int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printf("Incorrect usage. Please provide only 1 argument with configuration file\n");
        return EXIT_FAILURE;
    }

    Daemon::getInstance().start(argv[1]);

    return EXIT_SUCCESS;
}*/