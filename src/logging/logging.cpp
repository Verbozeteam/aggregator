#include "config/config.hpp"
#include "logging/logging.hpp"

#include <iostream>
using std::cout;

#include <algorithm>
using std::min;
using std::max;

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
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

int Log::Initialize() {
    int log_level = std::min<int>(std::max<int>(5 - ConfigManager::get<int>("verbozity"), 0), 5);
    boost::log::trivial::severity_level severity = (boost::log::trivial::severity_level)log_level;

    logging::core::get()->set_filter (
        logging::trivial::severity >= severity
    );

    boost::log::add_file_log(
        keywords::file_name = "log_files/sample_%N.log",
        keywords::rotation_size = 10 ,
        keywords::max_files = 10,
        keywords::format =
        (
            expr::stream
                << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S")
                << "] [" << logging::trivial::severity << "]: " << expr::smessage
        )
    );
    logging::add_console_log(std::cout, boost::log::keywords::format = (
        expr::stream << "[" << logging::trivial::severity << "]: " << expr::smessage
    ));
    logging::add_common_attributes();

    BOOST_LOG_TRIVIAL(trace) << "A trace severity message";
    BOOST_LOG_TRIVIAL(debug) << "A trace severity message";
    BOOST_LOG_TRIVIAL(info) << "A trace severity message";
    BOOST_LOG_TRIVIAL(warning) << "A trace severity message";
    BOOST_LOG_TRIVIAL(error) << "A trace severity message";
    BOOST_LOG_TRIVIAL(fatal) << "A trace severity message";
    BOOST_LOG_TRIVIAL(error) << "A trace severity message";
    BOOST_LOG_TRIVIAL(fatal) << "A trace severity message";
    BOOST_LOG_TRIVIAL(error) << "A trace severity message";
    BOOST_LOG_TRIVIAL(fatal) << "A trace severity message";
    BOOST_LOG_TRIVIAL(error) << "A trace severity message";
    BOOST_LOG_TRIVIAL(fatal) << "A trace severity message";
    BOOST_LOG_TRIVIAL(error) << "A trace severity message";
    BOOST_LOG_TRIVIAL(fatal) << "A trace severity message";
    BOOST_LOG_TRIVIAL(error) << "A trace severity message";
    BOOST_LOG_TRIVIAL(fatal) << "A trace severity message";
    BOOST_LOG_TRIVIAL(error) << "A trace severity message";
    BOOST_LOG_TRIVIAL(fatal) << "A trace severity message";
    BOOST_LOG_TRIVIAL(error) << "A trace severity message";
    BOOST_LOG_TRIVIAL(fatal) << "A trace severity message";

    return 0;
}

void Log::Cleanup() {
    logging::core::get()->remove_all_sinks();
}
