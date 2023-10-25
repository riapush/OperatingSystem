#include "Daemon.h"

static void signal_handler(int sig) {
    switch (sig){
        case SIGHUP:
            Daemon::getInstance().read_config();
            break;
        case SIGTERM:
            syslog(LOG_NOTICE, "Deamon terminated");
            closelog();
            exit(EXIT_SUCCESS);
            break;
        default:
            break;

    }
}

void Daemon::set_config_file(std::string config_file_path) {
    if (std::filesystem::exists(config_file_path)) {
        config_file_ = config_file_path;
    }
    else {
        throw std::runtime_error("File does not exist or you do not have access to open it.");
    }

}

void Daemon::read_config() {
    std::ifstream input(config_file_);
    if(!input) {
        throw std::runtime_error("File does not exist or you do not have access to open it.");
    }
    std::string line;
    int line_count = 1;
    while (std::getline(input, line)) {
        switch (line_count) {
            case 1:
                folder1 = line;
                break;
            case 2:
                folder2 = line;
                break;
            case 3:
                interval = std::stoi(line);
                break;
            default:
                break;
        }
        line_count++;
    }
}

void Daemon::log_folder_contents() {
    std::ofstream logfile(folder2 / "hist.log", std::ios::app);
    if (logfile.is_open()) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        logfile << "Time: " << std::ctime(&now);
        logfile << "Directory contents: \n";
        for (const auto& entry : std::filesystem::directory_iterator(folder1)) {
            logfile << entry.path() << "\n";
        }
        logfile.close();
    } else {
        throw std::runtime_error("File does not exist or you do not have access to open it.");        }
}

void Daemon::start(std::string config_file_path) {
    // Daemon logic to periodically perform actions
    stop_daemon();
    make_daemon();
    set_config_file(config_file_path);
    read_config();
    syslog(LOG_NOTICE, "Daemon started");
    while (true) {
        try {
            log_folder_contents();
            std::this_thread::sleep_for(std::chrono::seconds(interval));
        }
        catch (const std::runtime_error& e) {
            syslog(LOG_ERR, "exception: %s" ,e.what());
            break;
        }
    }
}

void Daemon::stop_daemon(){
    int pid;
    std::ifstream f(pid_path);
    f >> pid;
    if (std::filesystem::exists("/proc/" + std::to_string(pid)))
        kill(pid, SIGTERM);
    syslog(LOG_NOTICE, "Daemon terminated");
}

void Daemon::make_daemon() {
    pid_t pid = fork();
    if (pid != 0) {
        exit(EXIT_FAILURE);
    }

    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }

    std::signal(SIGHUP, signal_handler);
    std::signal(SIGTERM, signal_handler);

    pid = fork();
    if (pid != 0)
        exit(EXIT_FAILURE);

    umask(0);
    if (chdir("/") != 0)
        exit(EXIT_FAILURE);

    for (long x = sysconf(_SC_OPEN_MAX); x >= 0; --x)
        close(x);
    openlog("mydaemon", LOG_PID, LOG_DAEMON);

    std::ofstream f(pid_path, std::ios_base::trunc);
    f << getpid();
}

