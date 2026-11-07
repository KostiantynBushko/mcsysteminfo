#include "mcsysteminfo/sysinfo.hpp"
#include "mcsysteminfo/version.hpp"

#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Options {
    bool                     json = false;
    bool                     color = true;
    bool                     help = false;
    bool                     version = false;
    std::vector<std::string> sections;
};

void printUsage(const char* program) {
    std::cout
        << "Usage: " << program << " [OPTIONS]\n"
        << "\n"
        << "Report hardware and operating system information of the running system.\n"
        << "\n"
        << "Options:\n"
        << "  -j, --json              Emit machine readable JSON instead of text\n"
        << "  -s, --section <NAME>    Limit output to a section (repeatable)\n"
        << "                          Sections: host, kernel, cpu, memory, uptime,\n"
        << "                                    storage, network\n"
        << "      --no-color          Disable ANSI colors in the text output\n"
        << "  -h, --help              Show this help and exit\n"
        << "  -V, --version           Show version information and exit\n"
        << "\n"
        << "Exit status:\n"
        << "  0  success\n"
        << "  1  invalid usage\n"
        << "  2  collection failed\n";
}

bool parseArguments(int argc, char** argv, Options& options, std::string& error) {
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];

        if (argument == "-h" || argument == "--help") {
            options.help = true;
        } else if (argument == "-V" || argument == "--version") {
            options.version = true;
        } else if (argument == "-j" || argument == "--json") {
            options.json = true;
        } else if (argument == "--no-color") {
            options.color = false;
        } else if (argument == "-s" || argument == "--section") {
            if (i + 1 >= argc) {
                error = "missing argument for " + argument;
                return false;
            }
            const std::string name = argv[++i];
            const std::vector<std::string>& known = mcsysteminfo::sectionNames();
            bool valid = false;
            for (std::vector<std::string>::const_iterator it = known.begin(); it != known.end();
                 ++it) {
                if (*it == name) {
                    valid = true;
                    break;
                }
            }
            if (!valid) {
                error = "unknown section: " + name;
                return false;
            }
            options.sections.push_back(name);
        } else if (argument.compare(0, 10, "--section=") == 0) {
            const std::string name = argument.substr(10);
            const std::vector<std::string>& known = mcsysteminfo::sectionNames();
            bool valid = false;
            for (std::vector<std::string>::const_iterator it = known.begin(); it != known.end();
                 ++it) {
                if (*it == name) {
                    valid = true;
                    break;
                }
            }
            if (!valid) {
                error = "unknown section: " + name;
                return false;
            }
            options.sections.push_back(name);
        } else {
            error = "unknown option: " + argument;
            return false;
        }
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    std::string error;

    if (!parseArguments(argc, argv, options, error)) {
        std::cerr << "mcsysteminfo: " << error << "\n"
                  << "Try '" << argv[0] << " --help' for more information.\n";
        return 1;
    }

    if (options.help) {
        printUsage(argv[0]);
        return 0;
    }

    if (options.version) {
        std::cout << mcsysteminfo_version_string() << "\n";
        return 0;
    }

    // Colors are only useful on a terminal; --no-color keeps them off.
    options.color = options.color && (::isatty(STDOUT_FILENO) == 1);

    const mcsysteminfo::SystemInfo info = mcsysteminfo::collect();

    if (options.json) {
        std::cout << mcsysteminfo::toJson(info, options.sections);
    } else {
        std::cout << mcsysteminfo::toText(info, options.sections, options.color) << "\n";
    }

    return 0;
}
