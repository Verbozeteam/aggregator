#include "config/config.hpp"
#include "logging/logging.hpp"

#include <iostream>
using std::cout;

#include <algorithm>
using std::min;
using std::max;

#include <dirent.h>

#include <boost/filesystem.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/support/date_time.hpp>

#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;

int Log::m_max_file_size = 0;
int Log::m_max_num_files = 0;
int Log::m_max_num_runs = 0;
std::string Log::m_log_folder = "log_files";
std::string Log::m_current_run_foldername = "";
std::string Log::m_run_file_prefix = "run_";

int Log::_initLogDirs() {
    DIR* dir = opendir(m_log_folder.c_str());
    struct dirent *entry = readdir(dir);
    std::vector<std::pair<int, std::string>> run_folders; // pairs (run_id, dir name)
    int highest_run = -1;

    // for every sub-directory, see if its a run folder and get its number
    while (entry != NULL) {
        if (entry->d_type == DT_DIR && strlen(entry->d_name) > m_run_file_prefix.length()) {
            std::string dname = std::string(entry->d_name);
            if (dname.find(m_run_file_prefix) == 0) {
                try {
                    std::string number_part = dname.substr(m_run_file_prefix.length());
                    int num = std::stoi(number_part);
                    run_folders.push_back(std::pair<int, std::string>(num, dname));
                    highest_run = std::max(num, highest_run);
                } catch(...) {}
            }
        }

        entry = readdir(dir);
    }

    closedir(dir);

    // if there are too many run folders, delete the early runs
    std::sort(run_folders.begin(), run_folders.end(), [ ]( const std::pair<int, std::string>& lhs, const std::pair<int, std::string>& rhs ) {
       return lhs.first < rhs.first;
    });
    while (run_folders.size() >= (uint32_t)m_max_num_runs) {
        std::string run_folder = run_folders[0].second;
        boost::filesystem::path run_folder_dir(m_log_folder+"/"+run_folder);
        boost::filesystem::remove_all(run_folder_dir);
        run_folders.erase(run_folders.begin());
    }

    // create the new run folder
    m_current_run_foldername = m_run_file_prefix + std::to_string(highest_run+1);
    boost::filesystem::path dir_path(m_log_folder+"/"+m_current_run_foldername);
    if (!boost::filesystem::create_directory(dir_path))
        return -1;

    return 0;
}

int Log::Initialize() {
    m_max_file_size = ConfigManager::get<int>("max-log-file-size");
    m_max_num_files = ConfigManager::get<int>("max-num-log-files");
    m_max_num_runs = ConfigManager::get<int>("max-num-log-runs");
    int log_level = std::min<int>(std::max<int>(5 - ConfigManager::get<int>("verbozity"), 0), 5);

    /**
     * Manage run folders
     */
    if (_initLogDirs() < 0)
        return -1;

    /**
     * Initialize logging in the currently selected run folder
     */
    boost::log::trivial::severity_level severity = (boost::log::trivial::severity_level)log_level;

    logging::core::get()->set_filter (
        logging::trivial::severity >= severity
    );
    logging::add_common_attributes();

    logging::add_console_log(std::cout, keywords::format = (
        expr::stream << "[" << logging::trivial::severity << "]: " << expr::smessage
    ));

    boost::shared_ptr< logging::core > core = logging::core::get();

    boost::shared_ptr< sinks::text_file_backend > backend =
        boost::make_shared< sinks::text_file_backend >(
            keywords::file_name = m_log_folder + "/" + m_current_run_foldername + string("/log_%N.log"),
            keywords::rotation_size = m_max_file_size, // max single log file size
            keywords::format = (expr::stream
                << "[" << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S") << "]"
                << "[" << logging::trivial::severity << "]: " << expr::smessage
            )
        );

    // Wrap it into the frontend and register in the core.
    // The backend requires synchronization in the frontend.
    typedef sinks::synchronous_sink< sinks::text_file_backend > sink_t;
    boost::shared_ptr< sink_t > sink(new sink_t(backend));

    sink->locked_backend()->set_file_collector(sinks::file::make_collector(
        keywords::target = m_log_folder + "/" + m_current_run_foldername,
        keywords::max_size = m_max_file_size,
        keywords::max_files = m_max_num_files // max number of log files
    ));

    core->add_sink(sink);

    return 0;
}

void Log::Cleanup() {
    logging::core::get()->remove_all_sinks();
}
