#include "config/config.hpp"

#include <iostream>
#include <vector>
#include <string>

namespace std {
    std::ostream& operator<<(std::ostream &os, const std::vector<std::string> &vec) {
        for (auto item : vec)
            os << item << " ";
        return os;
    }
}

po::options_description ConfigManager::m_options_desc;
po::variables_map ConfigManager::m_variable_map;

int ConfigManager::LoadFromCommandline(int argc, char* argv[]) {
    m_options_desc.add_options()
        ("help,h", "produce help message")
        ("verbozity,v", po::value<int>()->default_value(3), "Set logging verbozity (from 0 to 6 (6 being highest verbozity))")
        ("max-log-file-size,lfs", po::value<int>()->default_value(1 * 1024 * 1024), "Set the maximum size of a single log file")
        ("max-num-log-files,nlf", po::value<int>()->default_value(5), "Set the maximum number of log files per run")
        ("max-num-log-runs,nlr", po::value<int>()->default_value(5), "Set the maximum number of runs to log")
        ("discovery-interfaces,i", po::value<std::vector<std::string>>()->multitoken()->default_value(std::vector<std::string>{"en0", "eth0", "eth1", "wlan0", "wlan1"}), "Set the interfaces on which discovery happens")
        ("websocket-url,ws", po::value<std::string>()->default_value("ws://localhost:8000"), "Set the url of the permenant websocket connection to Verboze")
    ;

    try {
        po::store(po::parse_command_line(argc, argv, m_options_desc), m_variable_map);
        po::notify(m_variable_map);

        if (m_variable_map.count("help")) {
            std::cout << argv[0] << " [options]:\n";
            std::cout << m_options_desc << "\n";
            return 1;
        }
    } catch (...) {
        std::cout << argv[0] << " [options]:\n";
        std::cout << m_options_desc << "\n";
        return -1;
    }

    return 0;
}