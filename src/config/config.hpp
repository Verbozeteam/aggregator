#pragma once

#include <string>
using std::string;

#include <boost/program_options.hpp>
namespace po = boost::program_options;

class ConfigManager {
    static po::options_description m_options_desc;
    static po::variables_map m_variable_map;

public:
    static int LoadFromCommandline(int argc, char* argv[]);

    template<typename T>
    static T get(std::string key) {
        if (m_variable_map.count(key))
            return m_variable_map[key].as<int>();
        return T();
    };
};
