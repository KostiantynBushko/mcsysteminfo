#include "mcsysteminfo/sysinfo.hpp"
#include "mcsysteminfo/version.hpp"

#include <iostream>
#include <string>

namespace {

int g_failures = 0;

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::cerr << "FAILED (line " << line << "): " << expression << "\n";
        ++g_failures;
    }
}

#define CHECK(cond) check((cond), #cond, __LINE__)

}  // namespace

int main() {
    using namespace mcsysteminfo;

    // ---- collection ------------------------------------------------------
    const SystemInfo info = collect();

    CHECK(!info.hostname.empty());
    CHECK(!info.kernel.sysname.empty());
    CHECK(!info.kernel.release.empty());
    CHECK(!info.kernel.machine.empty());
    CHECK(info.cpu.threads > 0);
    CHECK(info.cpu.cores > 0);
    CHECK(info.cpu.cores <= info.cpu.threads);
    CHECK(info.memory.total_bytes > 0);
    CHECK(info.memory.available_bytes <= info.memory.total_bytes);
    CHECK(info.uptime.uptime_seconds >= 0.0);

    // ---- json ------------------------------------------------------------
    const std::string json = toJson(info);
    CHECK(!json.empty());
    CHECK(json.front() == '{');
    CHECK(json.back() == '\n' || json.back() == '}');
    CHECK(json.find("\"hostname\"") != std::string::npos);
    CHECK(json.find("\"kernel\"") != std::string::npos);
    CHECK(json.find("\"cpu\"") != std::string::npos);
    CHECK(json.find("\"memory\"") != std::string::npos);
    CHECK(json.find("\"filesystems\"") != std::string::npos);
    CHECK(json.find("\"interfaces\"") != std::string::npos);

    // No dangling comma before the closing brace.
    CHECK(json.find(",\n}") == std::string::npos);

    // ---- text ------------------------------------------------------------
    const std::string text = toText(info);
    CHECK(!text.empty());
    CHECK(text.find("Host") != std::string::npos);
    CHECK(text.find("Kernel") != std::string::npos);
    CHECK(text.find("CPU") != std::string::npos);
    CHECK(text.find("Memory") != std::string::npos);
    CHECK(text.find("Uptime") != std::string::npos);
    CHECK(text.find("Storage") != std::string::npos);
    CHECK(text.find("Network") != std::string::npos);

    // ---- section filtering ----------------------------------------------
    const std::string cpuOnly = toJson(info, std::vector<std::string>(1, "cpu"));
    CHECK(cpuOnly.find("\"cpu\"") != std::string::npos);
    CHECK(cpuOnly.find("\"memory\"") == std::string::npos);
    CHECK(cpuOnly.find("\"hostname\"") == std::string::npos);
    CHECK(cpuOnly.find(",\n}") == std::string::npos);

    const std::string cpuText = toText(info, std::vector<std::string>(1, "cpu"));
    CHECK(cpuText.find("CPU") != std::string::npos);
    CHECK(cpuText.find("Memory") == std::string::npos);

    // ---- formatting ------------------------------------------------------
    CHECK(formatBytes(0) == "0 B");
    CHECK(formatBytes(1024) == "1.00 KiB");
    CHECK(formatBytes(1024ULL * 1024ULL) == "1.00 MiB");
    CHECK(formatUptime(0.0) == "00:00:00");
    CHECK(formatUptime(3661.0) == "01:01:01");
    CHECK(formatUptime(90061.0) == "1 day, 01:01:01");

    // ---- version ---------------------------------------------------------
    CHECK(std::string(mcsysteminfo_version()) == "0.1.0");
    CHECK(std::string(MCSYSTEMINFO_VERSION) == "0.1.0");

    if (g_failures == 0) {
        std::cout << "All checks passed.\n";
        return 0;
    }
    std::cerr << g_failures << " check(s) failed.\n";
    return 1;
}
