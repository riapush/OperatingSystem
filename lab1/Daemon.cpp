#include "Daemon.h"

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
    while (true) {
        try {
            set_config_file(config_file_path);
            read_config();
            log_folder_contents();
            std::this_thread::sleep_for(std::chrono::seconds(interval));
        }
        catch (const std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
            break;
        }
    }
}
