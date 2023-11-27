#include <sys/syslog.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <semaphore.h>
#include <csignal>
#include <thread>
#include <future>
#include <ostream>
#include <iostream>

#include "host.h"

Host::Host() {
    struct sigaction sig{};
    memset(&sig, 0, sizeof(sig));
    sig.sa_flags = SA_SIGINFO;
    sig.sa_sigaction = Host::signal_handler;
    sigaction(SIGTERM, &sig, nullptr);
    sigaction(SIGINT, &sig, nullptr);
}

void Host::signal_handler(int signum, siginfo_t* info, void *ptr) {
    switch (signum) {
    case SIGTERM:
        Host::get_instance().isRunning = false;
        return;
    case SIGINT:
        syslog(LOG_INFO, "[LAB2] Host terminated.");
        exit(EXIT_SUCCESS);
        return;
    default:
        syslog(LOG_INFO, "[LAB2] Unknown command.");
    }
}

static std::string getAnswer()
{    
    std::string answer;
    std::cin >> std::ws;
    std::getline(std::cin, answer);
    return answer;
}

void Host::run() {
    syslog(LOG_INFO, "[LAB2] Chat started.");
    if (prepare() == false) {
        stop();
        return;
    }
    std::thread connThread(&Host::connection_job, this);

    while (isRunning.load()) {
        // Получение сообщения от пользователя
        std::cout << "Enter your message: ";
        std::future<std::string> future = std::async(getAnswer);
        std::string answer;
        Message msg = {0};

        // Создание сообщения и помещение его в очередь для отправки
        std::string ans = future.get();
        strncpy(msg.text, ans.c_str(), ans.size());
        writeWin(msg);

        // Ожидание перед отправкой сообщения
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    stop();
    connThread.join();
}

void Host::stop() {
    if (isRunning.load()) {
        syslog(LOG_INFO, "[LAB2] Chat stops working.");
        isRunning = false;
    }
    system("pkill gnome-terminal");
}

bool Host::prepare() {
    syslog(LOG_INFO, "[LAB2] Preparing for chat start.");
    host_pid = getpid();
    conn = Connection::create(host_pid, true);
    host_sem = sem_open("/Host-sem", O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO, 0);
    if (host_sem == SEM_FAILED) {
        syslog(LOG_ERR, "[LAB2] ERROR: Host semaphore is not created.");
        return false;
    }
    client_sem = sem_open("/Client-sem", O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO, 0);
    if (client_sem == SEM_FAILED) {
        sem_close(host_sem);
        syslog(LOG_ERR, "[LAB2] ERROR: Client semaphore is not created.");
        return false;
    }
    try {
        Connection *raw = conn.get();
        raw->open(host_pid, true);
    }
    catch (std::exception &e) {
        syslog(LOG_ERR, "[LAB2] ERROR: %s", e.what());
        sem_close(host_sem);
        sem_close(client_sem);
        return false;
    }

    pid_t childPid = fork();
    if (childPid == 0)
    {
        client_pid = getpid();

        if (Client::get_instance().init(host_pid))
            Client::get_instance().run();
        else 
        {
            syslog(LOG_ERR, "[LAB2] ERROR: Client initialization error.");
            return false;
        }
        exit(EXIT_SUCCESS);
    }

    Host::get_instance().isRunning = true;
    syslog(LOG_INFO, "[LAB2] INFO: host initialized successfully.");
    return true;
}

void Host::connection_job() {
    lastMsgTime = std::chrono::high_resolution_clock::now();

    while (isRunning.load()) {
        double minutes_passed = std::chrono::duration_cast<std::chrono::minutes>(
            std::chrono::high_resolution_clock::now() - lastMsgTime).count();

        if (minutes_passed >= 1) {
          syslog(LOG_INFO, "[LAB2] Killing chat for 1 minute silence.");
          isRunning = false;
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
        if (s == -1)
        {
            syslog(LOG_ERR, "[LAB2] ERROR [Host]: Read semaphore timeout.");
            isRunning = false;
            return false;
        }
    }

    if (messagesIn.pushConnection(conn.get()) == false)
    {
        isRunning = false;
        return false;
    }
    else if (messagesIn.getSize() > 0) {
        lastMsgTime = std::chrono::high_resolution_clock::now();
        Message msg = {0};
        if (messagesIn.popMessage(&msg))
            std::cout << "New message from client: " << msg.text << std::endl;
}
    return true;
}

bool Host::write_message() {
    bool res = messagesOut.popConnection(conn.get());
    sem_post(client_sem);
    return res;
}

void Host::close_connection() {
    conn->close();
    sem_close(host_sem);
    sem_close(client_sem);
    kill(client_pid, SIGTERM);
}

bool Host::IsRun() {
    return Host::get_instance().isRunning.load();
}

void Host::writeWin(Message msg) {
    Host::get_instance().messagesOut.pushMessage(msg);
}

int main(int argc, char *argv[]) {
    openlog("Chat log", LOG_NDELAY | LOG_PID, LOG_USER);
    try {
        Host::get_instance().run();
    } catch (std::exception &e) {
        syslog(LOG_ERR, "[LAB2] ERROR: %s. Closing chat...", e.what());
    }
    closelog();
    return 0;
}