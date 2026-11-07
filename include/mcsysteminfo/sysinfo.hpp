#ifndef MCSYSTEMINFO_SYSINFO_HPP
#define MCSYSTEMINFO_SYSINFO_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace mcsysteminfo {

/// Processor description as reported by the kernel.
struct CpuInfo {
    std::string   vendor;
    std::string   model;
    std::uint32_t cores   = 0;  ///< physical cores, 0 if the kernel does not report them
    std::uint32_t threads = 0;  ///< logical processors visible to the system
    double        mhz     = 0.0;
};

/// Physical memory summary derived from /proc/meminfo.
struct MemoryInfo {
    std::uint64_t total_bytes     = 0;
    std::uint64_t free_bytes      = 0;
    std::uint64_t available_bytes = 0;
    std::uint64_t cached_bytes    = 0;
};

/// Kernel identity obtained from uname(2).
struct KernelInfo {
    std::string sysname;
    std::string release;
    std::string version;
    std::string machine;
};

/// Time and load information.
struct UptimeInfo {
    double uptime_seconds = 0.0;
    double load1 = 0.0;
    double load5 = 0.0;
    double load15 = 0.0;
};

/// Usage of a mounted filesystem.
struct FilesystemInfo {
    std::string   mount_point;
    std::string   type;
    std::uint64_t total_bytes     = 0;
    std::uint64_t used_bytes      = 0;
    std::uint64_t available_bytes = 0;
};

/// A network interface and its primary address.
struct NetworkInfo {
    std::string name;
    std::string address;
    bool        up = false;
};

/// Complete snapshot of the running system.
struct SystemInfo {
    std::string              hostname;
    std::string              machine_id;
    KernelInfo               kernel;
    CpuInfo                  cpu;
    MemoryInfo               memory;
    UptimeInfo               uptime;
    std::vector<FilesystemInfo> filesystems;
    std::vector<NetworkInfo>    interfaces;
};

/// Collect a snapshot of the running system.
/// Never throws; individual fields are left at their default when the
/// underlying information is not available on this platform.
SystemInfo collect();

/// Render as JSON. An empty \p sections list means "every section".
std::string toJson(const SystemInfo& info,
                   const std::vector<std::string>& sections = std::vector<std::string>());

/// Render as human readable text. An empty \p sections list means "every section".
std::string toText(const SystemInfo& info,
                   const std::vector<std::string>& sections = std::vector<std::string>(),
                   bool color = false);

/// Format a byte count using binary units, e.g. "1.50 GiB".
std::string formatBytes(std::uint64_t bytes);

/// Format a duration as "3 days, 04:15:32".
std::string formatUptime(double seconds);

/// Names of the sections understood by toJson()/toText().
const std::vector<std::string>& sectionNames();

}  // namespace mcsysteminfo

#endif /* MCSYSTEMINFO_SYSINFO_HPP */
