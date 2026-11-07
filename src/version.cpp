#include "mcsysteminfo/version.hpp"

extern "C" const char* mcsysteminfo_version(void) {
    return MCSYSTEMINFO_VERSION;
}

extern "C" const char* mcsysteminfo_version_string(void) {
    return "mcsysteminfo " MCSYSTEMINFO_VERSION;
}
