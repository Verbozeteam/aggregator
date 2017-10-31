#include "utilities/time_utilities.hpp"

milliseconds __get_time_ms() {
    return duration_cast< milliseconds >(
        system_clock::now().time_since_epoch()
    );
}