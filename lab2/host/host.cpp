#include <sys/syslog.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <semaphore.h>
#include <csignal>
#include <thread>

#include "host.h"

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

Host& Host::get_instance() {
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
  {
      timespec t;
      clock_gettime(CLOCK_REALTIME, &t);
      t.tv_sec += 5;
      int s = sem_timedwait(host_sem, &t);
      if (s == -1) {
          syslog(LOG_ERR, "ERROR [Host]: Read semaphore timeout");
          is_running = false;
          return false;
      }
  }

  if (messages_in.push_connection(conn.get()) == false) {
      is_running = false;
      return false;
  }
  else if (messages_in.get_size() > 0) {
      last_message_time = std::chrono::high_resolution_clock::now();
  }
  return true;
}

bool Host::write_message() {
  bool res = messages_out.pop_connection(conn.get());
  sem_post(client_sem);
  return res;
}

void Host::close_connection() {
    conn->close();
    sem_close(host_sem);
    sem_close(client_sem);
    kill(client_pid, SIGTERM);
    syslog(LOG_INFO, "Connection has been closed");
}

bool Host::prepare() {
    syslog(LOG_INFO, "Start initialization for host-client chat connection");
    host_pid = getpid();
    conn = Connection::create(host_pid, true);
    host_sem = sem_open("/Host-sem", O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO, 0);
    if (host_sem == SEM_FAILED) {
        syslog(LOG_ERR, "ERROR: host semaphore was not created");
        return false;
    }
    client_sem = sem_open("/Client-sem", O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO, 0);
    if (client_sem == SEM_FAILED) {
        sem_close(host_sem);
        syslog(LOG_ERR, "ERROR: client semaphore was not created");
        return false;
    }
    try {
        Connection *raw = conn.get();
        raw->open(host_pid, true);
    }
    catch (std::exception &e) {
        syslog(LOG_ERR, "ERROR: %s", e.what());
        sem_close(host_sem);
        sem_close(client_sem);
        return false;
    }

    pid_t child_pid = fork();
    if (child_pid == 0) {
        client_pid = getpid();

        if (Client::get_instance().init(host_pid)) {
            Client::get_instance().run();
        }
        else {
            syslog(LOG_ERR, "ERROR: client initialization error");
            return false;
        }
        exit(EXIT_SUCCESS);
    }

    Host::getInstance().is_running = true;
    syslog(LOG_INFO, "INFO: host initialize successfully");
    return true;
}

void Host::run() {
    syslog(LOG_INFO, "Chat started");
    if (prepare() == false) {
        stop();
        return;
    }
    std::thread conn_thread(&Host::connection_job, this);

    connection_job()
    
    stop();
    conn_thread.join();
}

void Host::stop() {
    if (is_running.load()) {
        syslog(LOG_INFO, "Chat has been stopped");
        is_running = false;
    }
}

int main(int argc, char *argv[]) {
    openlog("Chat log", LOG_NDELAY | LOG_PID, LOG_USER);
    try {
        Host::get_instance().run();
    } catch (std::exception &e) {
        syslog(LOG_ERR, "ERROR: %s. Close chat...", e.what());
    }
    closelog();
    return 0;
}