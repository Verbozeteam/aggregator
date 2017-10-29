#pragma once

#include <boost/log/trivial.hpp>

#include <string>
using std::string;

class Log {
    static int m_max_file_size;
    static int m_max_num_files;
    static int m_max_num_runs;
    static std::string m_log_folder;
    static std::string m_current_run_foldername;
    static std::string m_run_file_prefix;

    static int _initLogDirs();

public:
    static int Initialize();
    static void Cleanup();
};

#define LOG BOOST_LOG_TRIVIAL
