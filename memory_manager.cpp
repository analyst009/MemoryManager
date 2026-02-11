#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <thread>

#ifdef _WIN32
    #include <io.h>
    #include <windows.h>
    #define SYNC_FUNC(fd) _commit(fd)
#else
    #include <unistd.h>
    #include <sys/time.h>
    #define SYNC_FUNC(fd) fsync(fd)
#endif

#include "memory_manager.h"

#define CHUNK_SIZE (10ULL * 1024ULL * 1024ULL * 1024ULL) /* 10 GB */
#define MAX_ALLOCATIONS 100
#define MAX_PATH_LENGTH 512
#define MAX_PATHS 32

typedef struct {
    void* ptr;
    size_t size;
} Allocation;

typedef struct {
    char filepath[MAX_PATH_LENGTH];
    int start_chunk;
    int end_chunk;
    double* elapsed_time;
} ThreadArgs;

static Allocation allocations[MAX_ALLOCATIONS];
static int allocation_count = 0;

void mm_allocate_chunk(void) {
    if (allocation_count >= MAX_ALLOCATIONS) {
        printf("[ERROR] Maximum allocations (%d) reached.\n", MAX_ALLOCATIONS);
        return;
    }

    void* ptr = malloc((size_t)CHUNK_SIZE);
    double chunk_gb = (double)CHUNK_SIZE / (1024.0 * 1024.0 * 1024.0);

    if (ptr == NULL) {
        printf("[ERROR] Failed to allocate %.2f GB. Insufficient memory or OS limit.\n", chunk_gb);
        return;
    }

    allocations[allocation_count].ptr = ptr;
    allocations[allocation_count].size = (size_t)CHUNK_SIZE;
    allocation_count++;

    printf("[SUCCESS] Allocated %.2f GB (Chunk #%d) at 0x%016llx\n",
           chunk_gb,
           allocation_count,
           (unsigned long long)(uintptr_t)ptr);
}

void mm_show_allocations(void) {
    if (allocation_count == 0) {
        printf("[INFO] No memory allocated yet.\n");
        return;
    }

    printf("\n===============================================\n");
    printf("%-6s %-18s %-10s\n", "Index", "Address", "Size(GB)");
    printf("-----------------------------------------------\n");

    double total_gb = 0.0;
    for (int i = 0; i < allocation_count; ++i) {
        double size_gb = (double)allocations[i].size / (1024.0 * 1024.0 * 1024.0);
        printf("%-6d 0x%016llx %8.2f\n",
               i + 1,
               (unsigned long long)(uintptr_t)allocations[i].ptr,
               size_gb);
        total_gb += size_gb;
    }

    printf("-----------------------------------------------\n");
    printf("Total: %d chunk(s), %.2f GB\n", allocation_count, total_gb);
    printf("===============================================\n");
}

/* Helper function for thread to dump assigned memory chunks */
static void thread_dump_chunks(ThreadArgs* args) {
#ifdef _WIN32
    LARGE_INTEGER freq, wall_start, wall_end;
    FILETIME start_creation, start_exit, start_kernel, start_user;
    FILETIME end_creation, end_exit, end_kernel, end_user;
    
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&wall_start);
    GetProcessTimes(GetCurrentProcess(), &start_creation, &start_exit, &start_kernel, &start_user);
#else
    struct timeval wall_start, wall_end;
    gettimeofday(&wall_start, NULL);
#endif

    unsigned long long total_bytes_written = 0ULL;

    printf("[INFO] Thread writing to: %s (chunks %d to %d)\n", 
           args->filepath, args->start_chunk + 1, args->end_chunk + 1);

    /* Create file even if no chunks assigned */
    FILE* dump_file = fopen(args->filepath, "wb");
    if (dump_file == NULL) {
        printf("[ERROR] Failed to create dump file: %s\n", args->filepath);
    } else {
        if (args->start_chunk <= args->end_chunk) {
            for (int i = args->start_chunk; i <= args->end_chunk; ++i) {
                size_t bytes_written = fwrite(allocations[i].ptr, 1, allocations[i].size, dump_file);
                if (bytes_written != allocations[i].size) {
                    printf("[WARNING] Chunk #%d: Wrote %zu bytes instead of %zu bytes\n", 
                           i + 1, bytes_written, allocations[i].size);
                }
                total_bytes_written += (unsigned long long)bytes_written;
            }

            fflush(dump_file);
            int sync_result = SYNC_FUNC(fileno(dump_file));
            if (sync_result != 0) {
                printf("[WARNING] Sync failed with error code: %d\n", sync_result);
            }
        }

        fclose(dump_file);
    }

#ifdef _WIN32
    QueryPerformanceCounter(&wall_end);
    GetProcessTimes(GetCurrentProcess(), &end_creation, &end_exit, &end_kernel, &end_user);

    double wall_time = (double)(wall_end.QuadPart - wall_start.QuadPart) / freq.QuadPart;
    
    ULARGE_INTEGER start_kernel_ull, start_user_ull, end_kernel_ull, end_user_ull;
    start_kernel_ull.LowPart = start_kernel.dwLowDateTime;
    start_kernel_ull.HighPart = start_kernel.dwHighDateTime;
    start_user_ull.LowPart = start_user.dwLowDateTime;
    start_user_ull.HighPart = start_user.dwHighDateTime;
    end_kernel_ull.LowPart = end_kernel.dwLowDateTime;
    end_kernel_ull.HighPart = end_kernel.dwHighDateTime;
    end_user_ull.LowPart = end_user.dwLowDateTime;
    end_user_ull.HighPart = end_user.dwHighDateTime;
    
    double kernel_time = (double)(end_kernel_ull.QuadPart - start_kernel_ull.QuadPart) / 10000000.0;
    double user_time = (double)(end_user_ull.QuadPart - start_user_ull.QuadPart) / 10000000.0;
    double cpu_time = kernel_time + user_time;
#else
    gettimeofday(&wall_end, NULL);
    double wall_time = (wall_end.tv_sec - wall_start.tv_sec) + 
                       (wall_end.tv_usec - wall_start.tv_usec) / 1000000.0;
    double cpu_time = 0.0;
#endif

    double size_gb = (double)total_bytes_written / (1024.0 * 1024.0 * 1024.0);

    printf("[SUCCESS] Thread completed writing:\n");
    printf("  File: %s\n", args->filepath);
    printf("  Total Data Written: %.2f GB\n", size_gb);
    printf("  Wall-Clock Time: %.6f seconds\n", wall_time);
    printf("  CPU Time: %.6f seconds\n", cpu_time);
    printf("===============================================\n");
    
    *args->elapsed_time = wall_time;
}

void mm_dump_all_memory(void) {
    if (allocation_count == 0) {
        printf("[INFO] No memory allocated yet. Nothing to dump.\n");
        return;
    }

    char paths_line[2048];
    char paths[MAX_PATHS][MAX_PATH_LENGTH];
    int path_count = 0;

    printf("\nEnter comma-separated directory paths to dump memory (e.g., ./out1,./out2): ");
    if (fgets(paths_line, sizeof(paths_line), stdin) == NULL) {
        printf("[ERROR] Failed to read paths. Dump cancelled.\n");
        return;
    }
    size_t llen = strlen(paths_line);
    if (llen > 0 && paths_line[llen - 1] == '\n') paths_line[llen - 1] = '\0';

    /* parse comma-separated values into paths[][] */
    char* saveptr = NULL;
    char* token = strtok_r(paths_line, ",", &saveptr);
    while (token != NULL && path_count < MAX_PATHS) {
        /* trim leading/trailing whitespace */
        while (*token == ' ' || *token == '\t') token++;
        size_t tlen = strlen(token);
        while (tlen > 0 && (token[tlen - 1] == ' ' || token[tlen - 1] == '\t')) {
            token[tlen - 1] = '\0';
            tlen--;
        }
        if (tlen > 0) {
            strncpy(paths[path_count], token, MAX_PATH_LENGTH - 1);
            paths[path_count][MAX_PATH_LENGTH - 1] = '\0';
            path_count++;
        }
        token = strtok_r(NULL, ",", &saveptr);
    }

    if (path_count == 0) {
        printf("[ERROR] No valid paths provided. Dump cancelled.\n");
        return;
    }

    time_t current_time = time(NULL);
    struct tm* time_info = localtime(&current_time);
    
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", time_info);
    
    /* Build filenames for each provided path */
    char filenames[MAX_PATHS][MAX_PATH_LENGTH];
    for (int i = 0; i < path_count; ++i) {
        snprintf(filenames[i], MAX_PATH_LENGTH, "%s/dump_file_%s.bin", paths[i], timestamp);
    }

#ifdef _WIN32
    LARGE_INTEGER freq, total_start, total_end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&total_start);
#else
    struct timeval total_start, total_end;
    gettimeofday(&total_start, NULL);
#endif

    /* Distribute chunks across path_count threads (±1) */
    int N = path_count;
    int base = allocation_count / N;
    int extra = allocation_count % N; /* first 'extra' threads get one more */

    ThreadArgs* args = new ThreadArgs[N];
    double* elapsed_times = new double[N];
    std::thread* threads = new std::thread[N];

    int start = 0;
    for (int i = 0; i < N; ++i) {
        int count = base + (i < extra ? 1 : 0);
        args[i].start_chunk = start;
        args[i].end_chunk = (count > 0) ? (start + count - 1) : (start - 1);
        args[i].elapsed_time = &elapsed_times[i];
        elapsed_times[i] = 0.0;
        /* copy filepath */
        args[i].filepath[0] = '\0';
        strncpy(args[i].filepath, filenames[i], MAX_PATH_LENGTH - 1);
        args[i].filepath[MAX_PATH_LENGTH - 1] = '\0';
        start += count;
    }

    printf("\n[INFO] Starting %d threads to dump memory...\n", N);
    for (int i = 0; i < N; ++i) {
        printf("  Thread %d: Chunks %d to %d -> %s\n", i + 1, args[i].start_chunk + 1,
               args[i].end_chunk + 1, args[i].filepath);
        threads[i] = std::thread(thread_dump_chunks, &args[i]);
    }

    for (int i = 0; i < N; ++i) threads[i].join();


#ifdef _WIN32
    QueryPerformanceCounter(&total_end);
    double total_wall_time = (double)(total_end.QuadPart - total_start.QuadPart) / freq.QuadPart;
#else
    gettimeofday(&total_end, NULL);
    double total_wall_time = (total_end.tv_sec - total_start.tv_sec) + 
                             (total_end.tv_usec - total_start.tv_usec) / 1000000.0;
#endif

    printf("\n===============================================\n");
    printf("[SUMMARY] All threads completed:\n");
    for (int i = 0; i < N; ++i) {
        printf("  Thread %d -> %s\n", i + 1, args[i].filepath);
        printf("    Time: %.6f seconds\n", elapsed_times[i]);
    }
    printf("  Total Time: %.6f seconds\n", total_wall_time);
    printf("===============================================\n");

    delete [] args;
    delete [] elapsed_times;
    delete [] threads;
}

void mm_free_all(void) {
    if (allocation_count == 0) {
        return;
    }
    for (int i = 0; i < allocation_count; ++i) {
        free(allocations[i].ptr);
        allocations[i].ptr = NULL;
        allocations[i].size = 0;
    }
    allocation_count = 0;
    printf("[INFO] Freed all allocated memory.\n");
}
