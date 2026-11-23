
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>

typedef struct {
    char name[256];
    char state;
    long ppid;
    long vmrss_kb;
} ProcessInfo;

typedef struct {
    long pid;
    ProcessInfo info;
    unsigned long long cpu_ticks;
    unsigned long long start_time;
    unsigned long long minor_faults;
    unsigned long long major_faults;
    unsigned long long voluntary_switches;
    unsigned long long nonvoluntary_switches;
} ProcessData;

typedef struct {
    unsigned long long user, nice, system;
    unsigned long long idle, iowait;
    unsigned long long irq, softirq, steal;
} CPUStats;

typedef struct {
    ProcessData data;
    double cpu_usage;
    double voluntary_rate;
    double nonvoluntary_rate;
    double minor_fault_rate;
    double major_fault_rate;
} ProcessRecord;

volatile sig_atomic_t running = 1;

void handle_sigint(int signal_number)
{
    (void)signal_number;
    running = 0;
}

int is_numeric(const char *text)
{
    if (*text == '\0')
        return 0;

    for (int i = 0; text[i] != '\0'; i++) {
        if (!isdigit((unsigned char)text[i]))
            return 0;
    }

    return 1;
}

void print_usage(const char *program)
{
    printf("\n============================================\n");
    printf("          KERNELSCOPE - HELP MENU\n");
    printf("============================================\n\n");

    printf("USAGE:\n");
    printf("  %s              Monitor all processes\n", program);
    printf("  %s chrome       Filter processes by name\n", program);
    printf("  %s 3688         Monitor a specific PID\n", program);
    printf("  %s --help       Display help information\n", program);

    printf("\nSAVED DATA:\n");
    printf("  History File: kernelscope.log\n");
    printf("  Monitoring snapshots are automatically saved.\n");
    printf("  Saved information includes:\n");
    printf("  - Snapshot date and time\n");
    printf("  - System CPU utilization\n");
    printf("  - Process ID and process name\n");
    printf("  - CPU and RAM usage\n");
    printf("  - Context switches\n");
    printf("  - Page faults\n");

    printf("\nVIEW SAVED HISTORY:\n");
    printf("  tail -n 30 kernelscope.log\n");
    printf("  less -S kernelscope.log\n");

    printf("\n============================================\n");
}

int read_cpu_stats(CPUStats *cpu)
{
    FILE *file = fopen("/proc/stat", "r");

    if (!file)
        return 0;

    int result = fscanf(file,
        "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
        &cpu->user, &cpu->nice, &cpu->system,
        &cpu->idle, &cpu->iowait, &cpu->irq,
        &cpu->softirq, &cpu->steal);

    fclose(file);

    return result == 8;
}

double calculate_system_cpu(CPUStats *old, CPUStats *current)
{
    unsigned long long old_total =
        old->user + old->nice + old->system + old->idle +
        old->iowait + old->irq + old->softirq + old->steal;

    unsigned long long new_total =
        current->user + current->nice + current->system +
        current->idle + current->iowait + current->irq +
        current->softirq + current->steal;

    unsigned long long old_idle = old->idle + old->iowait;
    unsigned long long new_idle = current->idle + current->iowait;

    if (new_total <= old_total || new_idle < old_idle)
        return 0.0;

    unsigned long long total_diff = new_total - old_total;
    unsigned long long idle_diff = new_idle - old_idle;

    if (idle_diff > total_diff)
        return 0.0;

    return 100.0 * (total_diff - idle_diff) / total_diff;
}

int read_process_data(const char *pid_text, ProcessData *data)
{
    char path[512];
    char line[1024];

    memset(data, 0, sizeof(*data));

    errno = 0;
    char *endptr;

    long pid = strtol(pid_text, &endptr, 10);

    if (errno == ERANGE || *endptr != '\0' || pid <= 0)
        return 0;

    data->pid = pid;
    data->info.state = '?';
    strcpy(data->info.name, "Unknown");

    snprintf(path, sizeof(path), "/proc/%s/status", pid_text);

    FILE *file = fopen(path, "r");

    if (!file)
        return 0;

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "Name:", 5) == 0)
            sscanf(line + 5, " %255[^\n]", data->info.name);

        else if (strncmp(line, "State:", 6) == 0)
            sscanf(line + 6, " %c", &data->info.state);

        else if (strncmp(line, "PPid:", 5) == 0)
            sscanf(line + 5, "%ld", &data->info.ppid);

        else if (strncmp(line, "VmRSS:", 6) == 0)
            sscanf(line + 6, "%ld", &data->info.vmrss_kb);

        else if (strncmp(line, "voluntary_ctxt_switches:", 24) == 0)
            sscanf(line + 24, "%llu", &data->voluntary_switches);

        else if (strncmp(line, "nonvoluntary_ctxt_switches:", 27) == 0)
            sscanf(line + 27, "%llu", &data->nonvoluntary_switches);
    }

    fclose(file);

    snprintf(path, sizeof(path), "/proc/%s/stat", pid_text);

    file = fopen(path, "r");

    if (!file)
        return 0;

    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return 0;
    }

    fclose(file);

    char *cursor = strrchr(line, ')');

    if (!cursor)
        return 0;

    cursor++;

    char *saveptr;
    char *token = strtok_r(cursor, " \t\n", &saveptr);

    unsigned long long utime = 0;
    unsigned long long stime = 0;

    for (int field = 3; field <= 22; field++) {
        if (!token)
            return 0;

        if (field == 10)
            data->minor_faults = strtoull(token, NULL, 10);

        if (field == 12)
            data->major_faults = strtoull(token, NULL, 10);

        if (field == 14)
            utime = strtoull(token, NULL, 10);

        if (field == 15)
            stime = strtoull(token, NULL, 10);

        if (field == 22)
            data->start_time = strtoull(token, NULL, 10);

        token = strtok_r(NULL, " \t\n", &saveptr);
    }

    data->cpu_ticks = utime + stime;

    return 1;
}

int collect_processes(ProcessData **list,
                      size_t *count,
                      size_t *capacity)
{
    DIR *directory = opendir("/proc");

    if (!directory)
        return 0;

    struct dirent *entry;

    while ((entry = readdir(directory)) != NULL) {
        if (!is_numeric(entry->d_name))
            continue;

        ProcessData current;

        if (!read_process_data(entry->d_name, &current))
            continue;

        if (*count == *capacity) {
            if (*capacity > 0 && *capacity > SIZE_MAX / 2) {
                closedir(directory);
                return 0;
            }

            size_t new_capacity =
                (*capacity == 0) ? 128 : (*capacity * 2);

            if (new_capacity > SIZE_MAX / sizeof(ProcessData)) {
                closedir(directory);
                return 0;
            }

            ProcessData *temp = realloc(
                *list, new_capacity * sizeof(ProcessData));

            if (!temp) {
                closedir(directory);
                return 0;
            }

            *list = temp;
            *capacity = new_capacity;
        }

        (*list)[(*count)++] = current;
    }

    closedir(directory);
    return 1;
}

ProcessData *find_process(ProcessData *list,
                          size_t count,
                          long pid)
{
    for (size_t i = 0; i < count; i++) {
        if (list[i].pid == pid)
            return &list[i];
    }

    return NULL;
}

double calculate_rate(unsigned long long current,
                      unsigned long long previous,
                      double elapsed)
{
    if (elapsed <= 0 || current < previous)
        return 0.0;

    return (double)(current - previous) / elapsed;
}

int compare_cpu(const void *a, const void *b)
{
    const ProcessRecord *p1 = a;
    const ProcessRecord *p2 = b;

    if (p1->cpu_usage > p2->cpu_usage)
        return -1;

    if (p1->cpu_usage < p2->cpu_usage)
        return 1;

    return 0;
}

int matches_filter(ProcessRecord *record, const char *filter)
{
    if (filter == NULL)
        return 1;

    if (is_numeric(filter))
        return record->data.pid == strtol(filter, NULL, 10);

    return strstr(record->data.info.name, filter) != NULL;
}

void write_log_timestamp(FILE *file)
{
    time_t now = time(NULL);
    struct tm local_time;
    char timestamp[64];

    if (localtime_r(&now, &local_time) != NULL) {
        strftime(timestamp, sizeof(timestamp),
                 "%Y-%m-%d %H:%M:%S", &local_time);

        fprintf(file, "\n========== SNAPSHOT: %s ==========\n",
                timestamp);
    }
}

void display_process(ProcessRecord *record)
{
    printf("%-6ld %-14.14s %-3c %-5ld %-7.2f %-6.2f %-6.1f %-6.1f %-7.1f %-7.1f\n",
           record->data.pid,
           record->data.info.name,
           record->data.info.state,
           record->data.info.ppid,
           record->data.info.vmrss_kb / 1024.0,
           record->cpu_usage,
           record->voluntary_rate,
           record->nonvoluntary_rate,
           record->minor_fault_rate,
           record->major_fault_rate);
}

void log_process(FILE *file, ProcessRecord *record)
{
    fprintf(file,
        "%-6ld %-14.14s %-3c %-5ld %-7.2f %-6.2f %-6.1f %-6.1f %-7.1f %-7.1f\n",
        record->data.pid,
        record->data.info.name,
        record->data.info.state,
        record->data.info.ppid,
        record->data.info.vmrss_kb / 1024.0,
        record->cpu_usage,
        record->voluntary_rate,
        record->nonvoluntary_rate,
        record->minor_fault_rate,
        record->major_fault_rate);
}

int main(int argc, char *argv[])
{
    int exit_status = 0;

    if (argc > 2) {
        printf("Error: Too many arguments!\n");
        print_usage(argv[0]);
        return 1;
    }

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (argc == 2 && is_numeric(argv[1])) {
        errno = 0;
        char *endptr;

        long pid = strtol(argv[1], &endptr, 10);

        if (errno == ERANGE || *endptr != '\0' || pid <= 0) {
            printf("Error: Invalid PID provided.\n");
            return 1;
        }
    }

    const char *filter = (argc > 1) ? argv[1] : NULL;

    CPUStats first_cpu, second_cpu;

    ProcessData *first = NULL;
    ProcessData *second = NULL;

    size_t first_count = 0, first_capacity = 0;
    size_t second_count = 0, second_capacity = 0;

    ProcessRecord *records = NULL;
    size_t record_capacity = 0;

    FILE *log_file = NULL;

    long clock_ticks = sysconf(_SC_CLK_TCK);

    struct timespec start, end;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Unable to register SIGINT handler");
        exit_status = 1;
        goto cleanup;
    }

    if (clock_ticks <= 0) {
        fprintf(stderr, "Unable to determine system clock ticks.\n");
        exit_status = 1;
        goto cleanup;
    }

    if (!read_cpu_stats(&first_cpu)) {
        fprintf(stderr, "Unable to read CPU statistics.\n");
        exit_status = 1;
        goto cleanup;
    }

    if (!collect_processes(&first, &first_count, &first_capacity)) {
        fprintf(stderr, "Unable to collect initial process information.\n");
        exit_status = 1;
        goto cleanup;
    }

    log_file = fopen("kernelscope.log", "a");

    if (!log_file) {
        perror("Unable to open kernelscope.log");
        exit_status = 1;
        goto cleanup;
    }

    fprintf(log_file, "\nKernelScope monitoring session started.\n");

    if (fflush(log_file) == EOF) {
        perror("Unable to write to kernelscope.log");
        exit_status = 1;
        goto cleanup;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        perror("Unable to read system clock");
        exit_status = 1;
        goto cleanup;
    }

    while (running) {
        sleep(2);

        if (!running)
            break;

        second_count = 0;

        if (!read_cpu_stats(&second_cpu))
            continue;

        if (!collect_processes(&second, &second_count,
                               &second_capacity)) {
            fprintf(stderr, "Unable to collect process information.\n");
            exit_status = 1;
            goto cleanup;
        }

        if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
            perror("Unable to read system clock");
            exit_status = 1;
            goto cleanup;
        }

        double elapsed =
            (end.tv_sec - start.tv_sec) +
            (end.tv_nsec - start.tv_nsec) / 1000000000.0;

        if (elapsed <= 0)
            elapsed = 2.0;

        double system_usage =
            calculate_system_cpu(&first_cpu, &second_cpu);

        if (second_count > SIZE_MAX / sizeof(ProcessRecord)) {
            fprintf(stderr, "Too many processes to allocate records.\n");
            exit_status = 1;
            goto cleanup;
        }

        if (second_count > record_capacity) {
            ProcessRecord *temp = realloc(
                records, second_count * sizeof(ProcessRecord));

            if (!temp) {
                perror("Unable to allocate process records");
                exit_status = 1;
                goto cleanup;
            }

            records = temp;
            record_capacity = second_count;
        }

        size_t record_count = 0;

        for (size_t i = 0; i < second_count; i++) {
            ProcessData *old = find_process(
                first, first_count, second[i].pid);

            ProcessRecord current = {0};

            current.data = second[i];

            if (old && old->start_time == second[i].start_time) {
                if (second[i].cpu_ticks >= old->cpu_ticks) {
                    current.cpu_usage =
                        100.0 * (double)(second[i].cpu_ticks - old->cpu_ticks) /
                        ((double)clock_ticks * elapsed);
                }

                current.voluntary_rate = calculate_rate(
                    second[i].voluntary_switches,
                    old->voluntary_switches, elapsed);

                current.nonvoluntary_rate = calculate_rate(
                    second[i].nonvoluntary_switches,
                    old->nonvoluntary_switches, elapsed);

                current.minor_fault_rate = calculate_rate(
                    second[i].minor_faults,
                    old->minor_faults, elapsed);

                current.major_fault_rate = calculate_rate(
                    second[i].major_faults,
                    old->major_faults, elapsed);
            }

            records[record_count++] = current;
        }

        size_t total_processes = record_count;

        if (filter != NULL) {
            size_t filtered_count = 0;

            for (size_t i = 0; i < record_count; i++) {
                if (matches_filter(&records[i], filter)) {
                    records[filtered_count++] = records[i];
                }
            }

            record_count = filtered_count;
        }

        if (records != NULL && record_count > 1) {
            qsort(records, record_count,
                  sizeof(ProcessRecord), compare_cpu);
        }

        if (record_count > 10)
            record_count = 10;

        printf("\033[2J\033[H");

        printf("===============================================================\n");
        printf("             KERNELSCOPE - LIVE SYSTEM OBSERVATORY\n");
        printf("===============================================================\n");

        printf("\nSystem CPU Utilization: %.2f%%\n", system_usage);
        printf("Refresh Interval: 2 seconds\n");
        printf("Total Processes Sampled: %zu\n", total_processes);
        printf("Displaying Top %zu Processes\n", record_count);

        if (filter)
            printf("Active Filter: %s\n", filter);
        else
            printf("Active Filter: None\n");

        printf("\n%-6s %-14s %-3s %-5s %-7s %-6s %-6s %-6s %-7s %-7s\n",
               "PID", "PROCESS", "ST", "PPID",
               "RAM(MB)", "CPU%", "VCTX", "NVCTX",
               "MINFLT", "MAJFLT");

        printf("-----------------------------------------------------------------------------------------------\n");

        if (record_count == 0) {
            printf("\nNo matching processes found.");

            if (filter)
                printf(" Filter: %s", filter);

            printf("\n");
        } else {
            for (size_t i = 0; i < record_count; i++)
                display_process(&records[i]);
        }

        printf("\nVCTX/NVCTX = context switches per second");
        printf("\nMINFLT/MAJFLT = page faults per second");
        printf("\nHistory file: kernelscope.log");
        printf("\nPress Ctrl+C to stop monitoring.\n");

        write_log_timestamp(log_file);

        fprintf(log_file, "System CPU Utilization: %.2f%%\n", system_usage);
        fprintf(log_file, "Total Processes Sampled: %zu\n", total_processes);

        if (record_count == 0) {
            fprintf(log_file, "No matching processes found.\n");
        } else {
            for (size_t i = 0; i < record_count; i++)
                log_process(log_file, &records[i]);
        }

        if (fflush(log_file) == EOF) {
            perror("Unable to write to kernelscope.log");
            exit_status = 1;
            goto cleanup;
        }

        fflush(stdout);

        free(first);

        first = second;
        first_count = second_count;
        first_capacity = second_capacity;

        second = NULL;
        second_count = 0;
        second_capacity = 0;

        first_cpu = second_cpu;
        start = end;
    }

cleanup:

    free(first);
    free(second);
    free(records);

    if (log_file) {
        fprintf(log_file, "\nKernelScope monitoring session ended.\n");
        fclose(log_file);
    }

    if (exit_status == 0)
        printf("\nKernelScope stopped. History saved in kernelscope.log\n");
    else
        printf("\nKernelScope exited with an error.\n");

    return exit_status;
}

