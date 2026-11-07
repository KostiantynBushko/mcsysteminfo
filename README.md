# mcsysteminfo

A small, dependency-free C++17 utility that reports hardware and operating
system information of the machine it runs on.

It is designed as the payload for a Yocto recipe demo: the binary is useful on
its own, it has no third-party dependencies, it builds cleanly with CMake, and
it exercises the parts of the Yocto build system that matter in practice
(pkg-config, `-dev` package splitting, systemd service integration and
cross-compilation).

On an embedded target this is the first thing you run after flashing an image
to confirm *which* image you actually booted.

## Features

* Hostname and machine ID (`/etc/machine-id`)
* Kernel identity from `uname(2)`
* CPU vendor, model, physical/logical core count and clock frequency
* Memory totals from `/proc/meminfo`
* Uptime and load average
* Mounted filesystems with usage, excluding bind mounts and pseudo filesystems
* Network interfaces with their addresses and link state
* Human readable output or machine readable JSON

## Building

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

## Testing

```sh
cd build && ctest --output-on-failure
```

The test suite is self-contained (no external test framework) and is run
automatically by `bitbake mcsysteminfo` when the recipe enables `ptest`.

## Installation

```sh
cmake --install build --prefix /usr
```

This installs:

| Path | Content |
|---|---|
| `${bindir}/mcsysteminfo` | the command line tool |
| `${libdir}/libmcsysteminfo.a` | the static library |
| `${includedir}/mcsysteminfo/` | public headers |
| `${libdir}/pkgconfig/mcsysteminfo.pc` | pkg-config metadata |
| `${libdir}/cmake/mcsysteminfo/` | CMake package config |

The library can be consumed either through pkg-config:

```sh
g++ my_tool.cpp $(pkg-config --cflags --libs mcsysteminfo)
```

or through CMake:

```cmake
find_package(mcsysteminfo REQUIRED)
target_link_libraries(my_tool PRIVATE mcsysteminfo::mcsysteminfo)
```

## Usage

```sh
mcsysteminfo                 # human readable report
mcsysteminfo --json          # machine readable report
mcsysteminfo --section cpu   # only the CPU section
mcsysteminfo --help          # full option list
```

Sections are `host`, `kernel`, `cpu`, `memory`, `uptime`, `storage` and
`network`. The `--section` option may be repeated.

### Example

```console
$ mcsysteminfo
Host
  Hostname      : qemux86-64
  Machine ID    : 3f2a9c1e5b7d4a8f9c0e1d2b3a4f5e6d

Kernel
  System        : Linux
  Release       : 6.6.0-yocto
  Version       : #1 SMP PREEMPT_DYNAMIC Thu Jan  1 00:00:00 UTC 2024
  Architecture  : x86_64

CPU
  Vendor        : GenuineIntel
  Model         : Intel(R) Core(TM) i7-6700K CPU @ 4.00GHz
  Cores         : 4 physical / 8 logical
  Frequency     : 4000.00 MHz

Memory
  Total         : 3.85 GiB
  Free          : 1.20 GiB
  Available     : 2.90 GiB
  Cached        : 1.05 GiB

Uptime
  Uptime        : 00:12:45
  Load average  : 0.15, 0.20, 0.18

Storage
  /                    ext4    3.71 GiB total, 1.02 GiB used, 2.51 GiB available

Network
  lo                  127.0.0.1 (up)
  eth0                192.168.1.10 (up)
```

### JSON

```console
$ mcsysteminfo --json
{
  "hostname": "qemux86-64",
  "machine_id": "3f2a9c1e5b7d4a8f9c0e1d2b3a4f5e6d",
  "kernel": {
    "sysname": "Linux",
    "release": "6.6.0-yocto",
    "version": "#1 SMP PREEMPT_DYNAMIC Thu Jan  1 00:00:00 UTC 2024",
    "machine": "x86_64"
  },
  "cpu": {
    "vendor": "GenuineIntel",
    "model": "Intel(R) Core(TM) i7-6700K CPU @ 4.00GHz",
    "cores": 4,
    "threads": 8,
    "mhz": 4000
  },
  "memory": {
    "total_bytes": 4132896768,
    "free_bytes": 1288490188,
    "available_bytes": 3113158122,
    "cached_bytes": 1127219200
  },
  "uptime": {
    "uptime_seconds": 765.0,
    "load1": 0.15,
    "load5": 0.2,
    "load15": 0.18
  },
  "filesystems": [
    { "mount_point": "/", "type": "ext4", "total_bytes": 3983052800, "used_bytes": 1095000000, "available_bytes": 2697000000 }
  ],
  "interfaces": [
    { "name": "lo", "address": "127.0.0.1", "up": true },
    { "name": "eth0", "address": "192.168.1.10", "up": true }
  ]
}
```

## Yocto integration

The `meta-sysinfo` layer contains the recipe that builds and packages this
project as `mcsysteminfo`. See that repository for the build instructions.

## License

MIT — see [LICENSE](LICENSE).
