# KernelScope
### Linux Process & Kernel Performance Observatory

KernelScope is a Linux-based system monitoring application written in C. It monitors running processes and collects important system performance statistics through the Linux `/proc` virtual filesystem.

The project aims to provide practical insights into Linux process management, CPU utilization, memory usage and kernel-level process statistics through a live terminal dashboard.

**Current Status:** Working command-line monitoring engine. A graphical desktop interface is planned for a future release.

## Features

- Live process monitoring with automatic 2-second refresh.
- System-wide CPU utilization monitoring.
- Individual process CPU utilization.
- Process memory monitoring (Resident Set Size).
- Process identification using PID.
- Process filtering by name or PID.
- Top 10 CPU-consuming processes.
- Voluntary and non-voluntary context switch monitoring.
- Minor and major page fault monitoring.
- Automatic historical monitoring logs.
- Command-line help and argument validation.
- Safe shutdown using Ctrl+C.

## Technologies Used

| Technology | Purpose |
|---|---|
| C | Core application development |
| Linux (Ubuntu) | Development and execution environment |
| GCC | Compilation |
| Linux `/proc` filesystem | System and process statistics |
| POSIX APIs | Process signals and timing |

## Project Structure

```text
KernelScope/
├── src/
│   └── main.c
├── kernelscope
├── kernelscope.log
└── README.md
```

The current implementation uses a single main C source file to keep the application straightforward.

## How It Works

KernelScope retrieves system information from the Linux `/proc` virtual filesystem.

The main interfaces used are:

- `/proc/stat` — Aggregate system CPU statistics.
- `/proc/[PID]/status` — Process name, state, parent PID, resident memory and context switches.
- `/proc/[PID]/stat` — Process CPU time and page fault counters.

The application processes these statistics and displays them through a continuously refreshing terminal dashboard.

CPU utilization is calculated using changes in CPU time between monitoring snapshots.

## Requirements

- Linux operating system (Ubuntu recommended).
- GCC compiler.
- Standard C library and POSIX support.

## Compilation

Clone or download the repository and navigate to its directory.

Compile the application:

```bash
gcc -Wall -Wextra -Wpedantic -fanalyzer -O2 src/main.c -o kernelscope
```

## Usage

**Monitor all running processes:**

```bash
./kernelscope
```

**Filter processes by name:**

```bash
./kernelscope chrome
```

**Monitor a specific process using PID:**

```bash
./kernelscope 3688
```

**Display help:**

```bash
./kernelscope --help
```

Press `Ctrl+C` to stop monitoring safely.

## Performance Metrics

| Metric | Description |
|---|---|
| CPU Utilization | CPU usage calculated from CPU time differences |
| RAM Usage | Resident memory used by a process |
| PID | Process Identification Number |
| PPID | Parent Process Identification Number |
| VCTX | Voluntary context switch rate |
| NVCTX | Non-voluntary context switch rate |
| MINFLT | Minor page fault rate |
| MAJFLT | Major page fault rate |

Process CPU utilization is measured per logical CPU and can exceed 100% for processes using multiple CPU cores.

## Historical Logging

KernelScope automatically records monitoring snapshots in:

```text
kernelscope.log
```

The log includes timestamps, system CPU utilization and process-level monitoring information.

View recent records:

```bash
tail -n 30 kernelscope.log
```

View the complete log:

```bash
less -S kernelscope.log
```

## Operating System Concepts

KernelScope demonstrates practical implementation of:

- Linux process management.
- Process identification and parent-child relationships.
- CPU utilization measurement.
- Memory management and resident memory.
- Context switching.
- Minor and major page faults.
- Linux virtual filesystem.
- Dynamic memory allocation.
- File handling in C.
- System monitoring and performance analysis.

## Future Development

Planned improvements include:

- GTK-based graphical desktop interface.
- Interactive process selection.
- Process termination functionality.
- Historical performance graphs.
- Advanced filtering and sorting.
- Modular multi-file architecture.
- Ubuntu `.deb` and AppImage packaging.

These features are planned and are not part of the current command-line implementation.

## Conclusion

KernelScope is a practical exploration of Linux system monitoring and operating system concepts using C.

By collecting and processing information directly from Linux's `/proc` filesystem, the project demonstrates how process-level and system-level performance statistics can be monitored in real time.

The long-term goal is to evolve KernelScope into a complete Linux desktop monitoring application.
