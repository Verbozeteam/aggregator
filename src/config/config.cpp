#include "config/config.hpp"

#include <iostream>
using std::cout;

po::options_description ConfigManager::m_options_desc;
po::variables_map ConfigManager::m_variable_map;

int ConfigManager::LoadFromCommandline(int argc, char* argv[]) {
    m_options_desc.add_options()
        ("help", "produce help message")
        ("verbozity", po::value<int>()->default_value(3), "Set logging verbozity (from 0 to 6 (6 being highest verbozity)) (default is 3)")
        ("max-log-file-size", po::value<int>()->default_value(1 * 1024 * 1024), "Set the maximum size of a single log file (default is 1MB)")
        ("max-num-log-files", po::value<int>()->default_value(5), "Set the maximum number of log files per run (default is 5)")
        ("max-num-log-runs", po::value<int>()->default_value(5), "Set the maximum number of runs to log (default is 5)")
        ("max-socket-clients", po::value<int>()->default_value(1023), "Set the maximum number of clients that can connect (default is 1023)")
        ("hosting-port", po::value<int>()->default_value(4568), "Set the hosting port for the socket server (default is 4568)")
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