#include "config/config.hpp"

#include <iostream>
using std::cout;

po::options_description ConfigManager::m_options_desc;
po::variables_map ConfigManager::m_variable_map;

int ConfigManager::LoadFromCommandline(int argc, char* argv[]) {
    m_options_desc.add_options()
        ("help", "produce help message")
        ("verbozity", po::value<int>()->default_value(3), "Set logging verbozity (from 0 to 6 (6 being highest verbozity))")
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