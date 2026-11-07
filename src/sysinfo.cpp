#include "mcsysteminfo/sysinfo.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <set>
#include <sstream>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <utility>

namespace mcsysteminfo {
namespace {

// ---------------------------------------------------------------------------
// Small string helpers
// ---------------------------------------------------------------------------

std::string readFile(const std::string& path) {
    std::ifstream in(path.c_str());
    if (!in) {
        return std::string();
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string trim(const std::string& s) {
    const std::string::size_type first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return std::string();
    }
    const std::string::size_type last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string current;
    std::istringstream stream(s);
    while (std::getline(stream, current, sep)) {
        out.push_back(current);
    }
    return out;
}

// ---------------------------------------------------------------------------
// /proc/cpuinfo
// ---------------------------------------------------------------------------

CpuInfo parseCpuInfo(const std::string& content) {
    CpuInfo cpu;
    std::set<std::pair<std::string, std::string> > coreIds;
    std::string physicalId;
    std::string coreId;
    bool havePhysical = false;
    bool haveCore = false;

    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        const std::string::size_type colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, colon));
        const std::string value = trim(line.substr(colon + 1));

        if (key == "processor") {
            ++cpu.threads;
            physicalId.clear();
            coreId.clear();
            havePhysical = false;
            haveCore = false;
        } else if (key == "vendor_id") {
            cpu.vendor = value;
        } else if (key == "model name" || key == "Model" || key == "Hardware") {
            if (cpu.model.empty()) {
                cpu.model = value;
            }
        } else if (key == "cpu MHz" || key == "clock") {
            if (cpu.mhz <= 0.0) {
                try {
                    cpu.mhz = std::stod(value);
                } catch (const std::exception&) {
                    cpu.mhz = 0.0;
                }
            }
        } else if (key == "physical id") {
            physicalId = value;
            havePhysical = true;
        } else if (key == "core id") {
            coreId = value;
            haveCore = true;
        }

        if (havePhysical && haveCore) {
            coreIds.insert(std::make_pair(physicalId, coreId));
        }
    }

    cpu.cores = coreIds.empty() ? cpu.threads : static_cast<std::uint32_t>(coreIds.size());
    return cpu;
}

// ---------------------------------------------------------------------------
// /proc/meminfo
// ---------------------------------------------------------------------------

std::uint64_t parseKilobyteValue(const std::string& line) {
    const std::string::size_type colon = line.find(':');
    if (colon == std::string::npos) {
        return 0;
    }
    const std::vector<std::string> parts = split(trim(line.substr(colon + 1)), ' ');
    if (parts.empty()) {
        return 0;
    }
    try {
        return static_cast<std::uint64_t>(std::stoull(parts[0])) * 1024ULL;
    } catch (const std::exception&) {
        return 0;
    }
}

MemoryInfo parseMemInfo(const std::string& content) {
    MemoryInfo memory;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        const std::string::size_type colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, colon));
        if (key == "MemTotal") {
            memory.total_bytes = parseKilobyteValue(line);
        } else if (key == "MemFree") {
            memory.free_bytes = parseKilobyteValue(line);
        } else if (key == "MemAvailable") {
            memory.available_bytes = parseKilobyteValue(line);
        } else if (key == "Cached") {
            memory.cached_bytes = parseKilobyteValue(line);
        }
    }
    if (memory.available_bytes == 0) {
        memory.available_bytes = memory.free_bytes + memory.cached_bytes;
    }
    return memory;
}

// ---------------------------------------------------------------------------
// /proc/uptime and /proc/loadavg
// ---------------------------------------------------------------------------

UptimeInfo parseUptime(const std::string& uptimeContent, const std::string& loadContent) {
    UptimeInfo uptime;

    const std::vector<std::string> uptimeParts = split(trim(uptimeContent), ' ');
    if (!uptimeParts.empty()) {
        try {
            uptime.uptime_seconds = std::stod(uptimeParts[0]);
        } catch (const std::exception&) {
            uptime.uptime_seconds = 0.0;
        }
    }

    const std::vector<std::string> loadParts = split(trim(loadContent), ' ');
    if (loadParts.size() >= 3) {
        try {
            uptime.load1 = std::stod(loadParts[0]);
            uptime.load5 = std::stod(loadParts[1]);
            uptime.load15 = std::stod(loadParts[2]);
        } catch (const std::exception&) {
            uptime.load1 = uptime.load5 = uptime.load15 = 0.0;
        }
    }
    return uptime;
}

// ---------------------------------------------------------------------------
// /proc/mounts
// ---------------------------------------------------------------------------

const std::set<std::string>& pseudoFilesystemTypes() {
    static const std::set<std::string> types = {
        "proc", "sysfs", "devpts", "devtmpfs", "cgroup", "cgroup2", "securityfs",
        "debugfs", "tracefs", "configfs", "fusectl", "mqueue", "pstore", "bpf",
        "binfmt_misc", "autofs", "rpc_pipefs", "nsfs", "ramfs", "efivarfs",
        "squashfs", "rootfs"
    };
    return types;
}

std::vector<FilesystemInfo> parseFilesystems() {
    std::vector<FilesystemInfo> result;

    struct AcceptedMount {
        std::string device;
        std::string mount;
        std::string type;
    };
    std::vector<AcceptedMount> accepted;

    std::istringstream stream(readFile("/proc/mounts"));
    std::string line;

    while (std::getline(stream, line)) {
        const std::vector<std::string> parts = split(line, ' ');
        if (parts.size() < 3) {
            continue;
        }
        const std::string& device = parts[0];
        const std::string& mountPoint = parts[1];
        const std::string& type = parts[2];

        if (pseudoFilesystemTypes().count(type) != 0) {
            continue;
        }
        if (type.compare(0, 4, "fuse") == 0) {
            continue;
        }
        if (mountPoint.compare(0, 5, "/proc") == 0 || mountPoint.compare(0, 4, "/sys") == 0 ||
            mountPoint.compare(0, 4, "/dev") == 0) {
            continue;
        }

        // A mount nested inside an already accepted mount, backed by the same
        // device, is a bind mount and adds no information.
        bool nested = false;
        bool bindMount = false;
        for (std::size_t i = 0; i < accepted.size() && !nested; ++i) {
            const std::string& parent = accepted[i].mount;
            const bool isChild = (parent == "/") ||
                                 (mountPoint.size() > parent.size() &&
                                  mountPoint.compare(0, parent.size(), parent) == 0 &&
                                  mountPoint[parent.size()] == '/');
            if (!isChild) {
                continue;
            }
            nested = true;
            // Same device means a bind mount; a tmpfs inside a tmpfs is just a
            // private instance of the parent and not a volume of its own.
            if (accepted[i].device == device ||
                (type == "tmpfs" && accepted[i].type == "tmpfs")) {
                bindMount = true;
            }
        }
        if (bindMount) {
            continue;
        }

        struct statvfs stats;
        if (::statvfs(mountPoint.c_str(), &stats) != 0) {
            continue;
        }

        FilesystemInfo fs;
        fs.mount_point = mountPoint;
        fs.type = type;
        fs.total_bytes = static_cast<std::uint64_t>(stats.f_blocks) * stats.f_frsize;
        fs.available_bytes = static_cast<std::uint64_t>(stats.f_bavail) * stats.f_frsize;
        const std::uint64_t freeBytes = static_cast<std::uint64_t>(stats.f_bfree) * stats.f_frsize;
        fs.used_bytes = (fs.total_bytes > freeBytes) ? (fs.total_bytes - freeBytes) : 0;
        result.push_back(fs);

        AcceptedMount entry;
        entry.device = device;
        entry.mount = mountPoint;
        entry.type = type;
        accepted.push_back(entry);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Network interfaces
// ---------------------------------------------------------------------------

std::vector<NetworkInfo> parseInterfaces() {
    std::vector<NetworkInfo> result;

    struct ifaddrs* addresses = nullptr;
    if (::getifaddrs(&addresses) != 0) {
        return result;
    }

    for (struct ifaddrs* entry = addresses; entry != nullptr; entry = entry->ifa_next) {
        if (entry->ifa_name == nullptr) {
            continue;
        }
        const std::string name = entry->ifa_name;
        const bool up = (entry->ifa_flags & IFF_UP) != 0;

        std::string address;
        if (entry->ifa_addr != nullptr &&
            (entry->ifa_addr->sa_family == AF_INET || entry->ifa_addr->sa_family == AF_INET6)) {
            char host[NI_MAXHOST];
            host[0] = '\0';
            const socklen_t length = (entry->ifa_addr->sa_family == AF_INET6)
                                         ? sizeof(struct sockaddr_in6)
                                         : sizeof(struct sockaddr_in);
            if (::getnameinfo(entry->ifa_addr, length, host, sizeof(host), nullptr, 0,
                              NI_NUMERICHOST) == 0) {
                address = host;
            }
        }

        bool known = false;
        for (std::vector<NetworkInfo>::iterator it = result.begin(); it != result.end(); ++it) {
            if (it->name == name) {
                known = true;
                if (it->address.empty()) {
                    it->address = address;
                }
                break;
            }
        }
        if (!known) {
            NetworkInfo info;
            info.name = name;
            info.address = address;
            info.up = up;
            result.push_back(info);
        }
    }

    ::freeifaddrs(addresses);
    return result;
}

// ---------------------------------------------------------------------------
// Rendering helpers
// ---------------------------------------------------------------------------

std::string jsonEscape(const std::string& s) {
    std::ostringstream out;
    for (std::string::const_iterator it = s.begin(); it != s.end(); ++it) {
        const char c = *it;
        switch (c) {
            case '"':  out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b";  break;
            case '\f': out << "\\f";  break;
            case '\n': out << "\\n";  break;
            case '\r': out << "\\r";  break;
            case '\t': out << "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x",
                                  static_cast<unsigned int>(static_cast<unsigned char>(c)));
                    out << buffer;
                } else {
                    out << c;
                }
                break;
        }
    }
    return out.str();
}

bool sectionWanted(const std::vector<std::string>& sections, const std::string& name) {
    if (sections.empty()) {
        return true;
    }
    return std::find(sections.begin(), sections.end(), name) != sections.end();
}

}  // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

SystemInfo collect() {
    SystemInfo info;

    char hostname[256];
    hostname[0] = '\0';
    if (::gethostname(hostname, sizeof(hostname) - 1) == 0) {
        info.hostname = hostname;
    }

    info.machine_id = trim(readFile("/etc/machine-id"));
    if (info.machine_id.empty()) {
        info.machine_id = trim(readFile("/var/lib/dbus/machine-id"));
    }

    struct utsname uts;
    if (::uname(&uts) == 0) {
        info.kernel.sysname = uts.sysname;
        info.kernel.release = uts.release;
        info.kernel.version = uts.version;
        info.kernel.machine = uts.machine;
    }

    info.cpu = parseCpuInfo(readFile("/proc/cpuinfo"));
    info.memory = parseMemInfo(readFile("/proc/meminfo"));
    info.uptime = parseUptime(readFile("/proc/uptime"), readFile("/proc/loadavg"));
    info.filesystems = parseFilesystems();
    info.interfaces = parseInterfaces();

    return info;
}

std::string formatBytes(std::uint64_t bytes) {
    static const char* const units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 5) {
        value /= 1024.0;
        ++unit;
    }
    std::ostringstream out;
    out << std::fixed << std::setprecision(unit == 0 ? 0 : 2) << value << " " << units[unit];
    return out.str();
}

std::string formatUptime(double seconds) {
    if (seconds < 0.0) {
        seconds = 0.0;
    }
    const long long total = static_cast<long long>(seconds);
    const long long days = total / 86400LL;
    const long long hours = (total % 86400LL) / 3600LL;
    const long long minutes = (total % 3600LL) / 60LL;
    const long long secs = total % 60LL;

    std::ostringstream out;
    if (days > 0) {
        out << days << (days == 1 ? " day, " : " days, ");
    }
    out << std::setfill('0') << std::setw(2) << hours << ":"
        << std::setfill('0') << std::setw(2) << minutes << ":"
        << std::setfill('0') << std::setw(2) << secs;
    return out.str();
}

const std::vector<std::string>& sectionNames() {
    static const std::vector<std::string> names = {
        "host", "kernel", "cpu", "memory", "uptime", "storage", "network"
    };
    return names;
}

std::string toJson(const SystemInfo& info, const std::vector<std::string>& sections) {
    std::vector<std::string> members;

    if (sectionWanted(sections, "host")) {
        members.push_back("  \"hostname\": \"" + jsonEscape(info.hostname) + "\"");
        members.push_back("  \"machine_id\": \"" + jsonEscape(info.machine_id) + "\"");
    }

    if (sectionWanted(sections, "kernel")) {
        std::ostringstream block;
        block << "  \"kernel\": {\n"
              << "    \"sysname\": \"" << jsonEscape(info.kernel.sysname) << "\",\n"
              << "    \"release\": \"" << jsonEscape(info.kernel.release) << "\",\n"
              << "    \"version\": \"" << jsonEscape(info.kernel.version) << "\",\n"
              << "    \"machine\": \"" << jsonEscape(info.kernel.machine) << "\"\n"
              << "  }";
        members.push_back(block.str());
    }

    if (sectionWanted(sections, "cpu")) {
        std::ostringstream block;
        block << "  \"cpu\": {\n"
              << "    \"vendor\": \"" << jsonEscape(info.cpu.vendor) << "\",\n"
              << "    \"model\": \"" << jsonEscape(info.cpu.model) << "\",\n"
              << "    \"cores\": " << info.cpu.cores << ",\n"
              << "    \"threads\": " << info.cpu.threads << ",\n"
              << "    \"mhz\": " << info.cpu.mhz << "\n"
              << "  }";
        members.push_back(block.str());
    }

    if (sectionWanted(sections, "memory")) {
        std::ostringstream block;
        block << "  \"memory\": {\n"
              << "    \"total_bytes\": " << info.memory.total_bytes << ",\n"
              << "    \"free_bytes\": " << info.memory.free_bytes << ",\n"
              << "    \"available_bytes\": " << info.memory.available_bytes << ",\n"
              << "    \"cached_bytes\": " << info.memory.cached_bytes << "\n"
              << "  }";
        members.push_back(block.str());
    }

    if (sectionWanted(sections, "uptime")) {
        std::ostringstream block;
        block << "  \"uptime\": {\n"
              << "    \"uptime_seconds\": " << info.uptime.uptime_seconds << ",\n"
              << "    \"load1\": " << info.uptime.load1 << ",\n"
              << "    \"load5\": " << info.uptime.load5 << ",\n"
              << "    \"load15\": " << info.uptime.load15 << "\n"
              << "  }";
        members.push_back(block.str());
    }

    if (sectionWanted(sections, "storage")) {
        std::ostringstream block;
        block << "  \"filesystems\": [";
        for (std::size_t i = 0; i < info.filesystems.size(); ++i) {
            const FilesystemInfo& fs = info.filesystems[i];
            block << "\n    { \"mount_point\": \"" << jsonEscape(fs.mount_point) << "\""
                  << ", \"type\": \"" << jsonEscape(fs.type) << "\""
                  << ", \"total_bytes\": " << fs.total_bytes
                  << ", \"used_bytes\": " << fs.used_bytes
                  << ", \"available_bytes\": " << fs.available_bytes << " }"
                  << ((i + 1 < info.filesystems.size()) ? "," : "");
        }
        if (!info.filesystems.empty()) {
            block << "\n  ";
        }
        block << "]";
        members.push_back(block.str());
    }

    if (sectionWanted(sections, "network")) {
        std::ostringstream block;
        block << "  \"interfaces\": [";
        for (std::size_t i = 0; i < info.interfaces.size(); ++i) {
            const NetworkInfo& nic = info.interfaces[i];
            block << "\n    { \"name\": \"" << jsonEscape(nic.name) << "\""
                  << ", \"address\": \"" << jsonEscape(nic.address) << "\""
                  << ", \"up\": " << (nic.up ? "true" : "false") << " }"
                  << ((i + 1 < info.interfaces.size()) ? "," : "");
        }
        if (!info.interfaces.empty()) {
            block << "\n  ";
        }
        block << "]";
        members.push_back(block.str());
    }

    std::ostringstream out;
    out << "{\n";
    for (std::size_t i = 0; i < members.size(); ++i) {
        out << members[i];
        if (i + 1 < members.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "}\n";
    return out.str();
}

std::string toText(const SystemInfo& info, const std::vector<std::string>& sections, bool color) {
    const std::string bold = color ? "\033[1;36m" : "";
    const std::string dim = color ? "\033[0;90m" : "";
    const std::string off = color ? "\033[0m" : "";

    std::ostringstream out;

    struct Row {
        const char* label;
        std::string value;
    };

    const auto emitRows = [&out, &bold, &dim, &off](const char* title,
                                                    const std::vector<Row>& rows) {
        out << bold << title << off << "\n";
        for (std::vector<Row>::const_iterator it = rows.begin(); it != rows.end(); ++it) {
            out << "  " << dim << std::left << std::setw(14) << it->label << off
                << ": " << it->value << "\n";
        }
        out << "\n";
    };

    if (sectionWanted(sections, "host")) {
        std::vector<Row> rows;
        Row hostnameRow = {"Hostname", info.hostname};
        Row machineRow = {"Machine ID", info.machine_id.empty() ? std::string("n/a") : info.machine_id};
        rows.push_back(hostnameRow);
        rows.push_back(machineRow);
        emitRows("Host", rows);
    }

    if (sectionWanted(sections, "kernel")) {
        std::vector<Row> rows;
        Row sysnameRow = {"System", info.kernel.sysname};
        Row releaseRow = {"Release", info.kernel.release};
        Row versionRow = {"Version", info.kernel.version};
        Row machineRow = {"Architecture", info.kernel.machine};
        rows.push_back(sysnameRow);
        rows.push_back(releaseRow);
        rows.push_back(versionRow);
        rows.push_back(machineRow);
        emitRows("Kernel", rows);
    }

    if (sectionWanted(sections, "cpu")) {
        std::ostringstream cores;
        cores << info.cpu.cores << " physical / " << info.cpu.threads << " logical";

        std::ostringstream frequency;
        if (info.cpu.mhz > 0.0) {
            frequency << std::fixed << std::setprecision(2) << info.cpu.mhz << " MHz";
        } else {
            frequency << "n/a";
        }

        std::vector<Row> rows;
        Row vendorRow = {"Vendor", info.cpu.vendor.empty() ? std::string("n/a") : info.cpu.vendor};
        Row modelRow = {"Model", info.cpu.model.empty() ? std::string("n/a") : info.cpu.model};
        Row coresRow = {"Cores", cores.str()};
        Row frequencyRow = {"Frequency", frequency.str()};
        rows.push_back(vendorRow);
        rows.push_back(modelRow);
        rows.push_back(coresRow);
        rows.push_back(frequencyRow);
        emitRows("CPU", rows);
    }

    if (sectionWanted(sections, "memory")) {
        std::vector<Row> rows;
        Row totalRow = {"Total", formatBytes(info.memory.total_bytes)};
        Row freeRow = {"Free", formatBytes(info.memory.free_bytes)};
        Row availableRow = {"Available", formatBytes(info.memory.available_bytes)};
        Row cachedRow = {"Cached", formatBytes(info.memory.cached_bytes)};
        rows.push_back(totalRow);
        rows.push_back(freeRow);
        rows.push_back(availableRow);
        rows.push_back(cachedRow);
        emitRows("Memory", rows);
    }

    if (sectionWanted(sections, "uptime")) {
        std::ostringstream load;
        load << std::fixed << std::setprecision(2) << info.uptime.load1 << ", "
             << info.uptime.load5 << ", " << info.uptime.load15;

        std::vector<Row> rows;
        Row uptimeRow = {"Uptime", formatUptime(info.uptime.uptime_seconds)};
        Row loadRow = {"Load average", load.str()};
        rows.push_back(uptimeRow);
        rows.push_back(loadRow);
        emitRows("Uptime", rows);
    }

    if (sectionWanted(sections, "storage")) {
        out << bold << "Storage" << off << "\n";
        if (info.filesystems.empty()) {
            out << "  " << dim << "no filesystems reported" << off << "\n";
        } else {
            for (std::size_t i = 0; i < info.filesystems.size(); ++i) {
                const FilesystemInfo& fs = info.filesystems[i];
                std::ostringstream usage;
                usage << formatBytes(fs.total_bytes) << " total, "
                      << formatBytes(fs.used_bytes) << " used, "
                      << formatBytes(fs.available_bytes) << " available";
                out << "  " << dim << std::left << std::setw(24) << fs.mount_point << off
                    << " " << std::left << std::setw(8) << fs.type << usage.str() << "\n";
            }
        }
        out << "\n";
    }

    if (sectionWanted(sections, "network")) {
        out << bold << "Network" << off << "\n";
        if (info.interfaces.empty()) {
            out << "  " << dim << "no interfaces reported" << off << "\n";
        } else {
            for (std::size_t i = 0; i < info.interfaces.size(); ++i) {
                const NetworkInfo& nic = info.interfaces[i];
                out << "  " << dim << std::left << std::setw(16) << nic.name << off
                    << " " << nic.address;
                if (!nic.address.empty()) {
                    out << " ";
                }
                out << "(" << (nic.up ? "up" : "down") << ")\n";
            }
        }
        out << "\n";
    }

    std::string text = out.str();
    // Drop the trailing blank line for a tidier terminal output.
    while (!text.empty() && text[text.size() - 1] == '\n') {
        text.erase(text.size() - 1);
    }
    return text;
}

}  // namespace mcsysteminfo
