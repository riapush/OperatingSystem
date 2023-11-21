#include "host.h"
#include <csignal>
#include <sys/syslog.h>
#include <sys/stat.h>

void Host::signal_handler(int sig, siginfo_t* info, void *ptr) {
    switch (sig) {
    case SIGTERM:
        Host::getInstance().is_running = false; // TODO: CHANGE
        return;
    case SIGINT:
        syslog(LOG_INFO, "Host has been terminated.");
        exit(EXIT_SUCCESS);
        return;
    default:
        syslog(LOG_INFO, "Command not found.");
    }
}

Host::Host() {
    struct sigaction sig{};
    memset(&sig, 0, sizeof(sig));
    sig.sa_flags = SA_SIGINFO;
    sig.sa_sigaction = Host::signal_handler;
    sigaction(SIGTERM, &sig, nullptr);
    sigaction(SIGINT, &sig, nullptr);
}

 Host& Host::getInstance() {
        static Host hostInstance;
        return hostInstance;
    }

