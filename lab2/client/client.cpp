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
#include <fstream>

#include "client.h"

void Client::signal_handler(int signum, siginfo_t* info, void *ptr) {
    switch (signum) {
    case SIGTERM:
        Client::get_instance().isRunning = false;
        return;
    case SIGINT:
        syslog(LOG_INFO, "[LAB2] INFO [Client]: Client is terminated.");
        exit(EXIT_SUCCESS);
        return;
    case SIGUSR1:
        syslog(LOG_INFO, "[LAB2] INFO [Client] chat terminated.");
        kill(Client::get_instance().host_pid, SIGTERM);
        exit(EXIT_SUCCESS);
        return;
    default:
        syslog(LOG_INFO, "[LAB2]: unknown command.");
    }
}

Client::Client() {
    struct sigaction sig{};

    memset(&sig, 0, sizeof(sig));
    sig.sa_flags = SA_SIGINFO;
    sig.sa_sigaction = Client::signal_handler;
    sigaction(SIGTERM, &sig, nullptr);
    sigaction(SIGINT, &sig, nullptr);
    sigaction(SIGUSR1, &sig, nullptr);
}

bool Client::init(const pid_t& host_pid) {
    syslog(LOG_INFO, "[LAB2] INFO[Client]: initializing...");
    isRunning = prepare(host_pid);

    if (isRunning)
        syslog(LOG_INFO, "[LAB2] INFO[Client]: client initialized successfully.");
    else
        syslog(LOG_INFO, "[LAB2] INFO[Client]: Cannot initialize chat.");

    return isRunning;
}

void Client::run() {
    syslog(LOG_INFO, "[LAB2] INFO[Client]: Client started running.");
    std::thread connThread(&Client::connection_job, this);
    
    std::ofstream log;
    log.open("client_terminal_out.file");
    system("gnome-terminal -e \"bash -c 'cp /dev/stdin  client_terminal_in.txt | tail -f client_terminal_out.file ;'\""); // открыли терминал, out которого соединен с дескриптором файла client_terminal_out.file, а stdin с дескриптором файла client_terminal_in.txt
    log.close();
    std::string input_file_name = "client_terminal_in.txt";
    std::ifstream input_file(input_file_name);
    input_file.seekg(0, std::ios::end);
    std::streampos file_size = input_file.tellg();
    input_file.close();
    
    while (isRunning.load()) {
        std::ifstream input_file(input_file_name);
        input_file.seekg(0, std::ios::end);

        std::streampos tail = input_file.tellg();
        if (file_size != tail) {
            input_file.seekg(file_size);
            int buffer_size = 1024;
            char buffer[buffer_size];
            input_file.read(buffer, sizeof(buffer));
            //printf("\nclient: %s\n", buffer);
            file_size = tail;
            Message msg = {0};
            strncpy(msg.text, buffer, buffer_size);
            writeWin(msg);
        }

        input_file.close();

        // Ожидание перед отправкой сообщения
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    connThread.join();
}

void Client::stop() {
    if (isRunning.load()) {
        syslog(LOG_INFO, "[LAB2] INFO[Client]: Stop working...");
        isRunning = false;
    }
}

bool Client::prepare(const pid_t& host_pid) {
    syslog(LOG_INFO, "[LAB2] INFO [Client]: Preparing for start");
    this->host_pid = host_pid;
    
    conn = Connection::create(host_pid, false);

    host_sem = sem_open("/Host-sem", O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO, 0);
    if (host_sem == SEM_FAILED) {
        syslog(LOG_ERR, "[LAB2] ERROR [Client]: Cannot connect to host semaphore.");
        return false;
    }
    client_sem = sem_open("/Client-sem", O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO, 0);
    if (client_sem == SEM_FAILED) {
        sem_close(host_sem);
        syslog(LOG_ERR, "[LAB2] ERROR [Client]: Cannot connect to client semaphore.");
        return false;
    }

    try {
        conn->open(host_pid, false);
        Client::get_instance().isRunning = true;
        syslog(LOG_INFO, "[LAB2] INFO [Client]: connection initializing complete!");
        return true;
    }
    catch (std::exception &e) {
        syslog(LOG_ERR, "[LAB2] ERROR [Client]: %s", e.what());
        sem_close(host_sem);
        sem_close(client_sem);
        return false;
    }
}

void Client::connection_job() {
    syslog(LOG_INFO, "[LAB2] INFO [Client]: Client starts working.");

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    while (isRunning.load()) {
        if (!write_message())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        if (!read_message())
            break;
    }
    close_connection();
}

bool Client::read_message() {
    {
        timespec t;
        clock_gettime(CLOCK_REALTIME, &t);
        t.tv_sec += 5;
        int s = sem_timedwait(client_sem, &t);
        if (s == -1)
        {
            syslog(LOG_ERR, "[LAB2] ERROR[Client]: Read semaphore timeout.");
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
        Message msg = {0};
        if (messagesIn.popMessage(&msg)){
            std::ofstream log("client_terminal_out.file", std::ios::app);
            if (log.is_open()){
                log << "New message from host: " << msg.text << '\n';
                log.close();
            }
            else{
                syslog(LOG_ERR, "[LAB2] ERORR[Client]: Unable to show new message to client.");
            }
            }
}
    return true;
}

bool Client::write_message() {
    bool res = messagesOut.popConnection(conn.get());
    sem_post(host_sem);
    return res;
}

void Client::close_connection() {
    conn->close();
    sem_close(host_sem);
    sem_close(client_sem);
    kill(host_pid, SIGTERM);
}

bool Client::IsRun() {
    return Client::get_instance().isRunning.load();
}

void Client::writeWin(Message msg) {
    Client::get_instance().messagesOut.pushMessage(msg);
}
