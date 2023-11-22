#include "host.h"
#include <csignal>
#include <sys/syslog.h>
#include <sys/stat.h>
#include <chrono>

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
      static Host host;
      return host;
}

void Host::connection_job() {
    last_message_time = std::chrono::high_resolution_clock::now();

    while (is_running.load()) {
        double minutes_passed = std::chrono::duration_cast<std::chrono::minutes>(
            std::chrono::high_resolution_clock::now() - last_message_time).count();

        if (minutes_passed >= 1) {
          syslog(LOG_INFO, "Client was silent for one minute. Disconnecting...");
          is_running = false;
          break;
        }
        if (!read_message())
          break;
        if (!write_message())
          break;

        std::this_thread::sleep_for(std::chrono::milliseconds(60));
    }
    close_connection();
}

bool Host::read_message() {
  ; // TODO
}

bool Host::write_message() {
  ; // TODO
}

void Host::close_connection() {
    syslog(LOG_INFO, "Client has been disconnected");
    ; // TODO
}

