#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

#ifdef _WIN32
    #include <io.h>
    #include <windows.h>
    #define SYNC_FUNC(fd) _commit(fd)
#else
    #include <unistd.h>
    #include <sys/time.h>
    #define SYNC_FUNC(fd) fsync(fd)
#endif

#define CHUNK_SIZE (10ULL * 1024ULL * 1024ULL * 1024ULL) /* 10 GB */
#define MAX_ALLOCATIONS 100

typedef struct {
    void* ptr;
    size_t size;
} Allocation;

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

void mm_dump_all_memory(void) {
    if (allocation_count == 0) {
        printf("[INFO] No memory allocated yet. Nothing to dump.\n");
        return;
    }

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
    
    time_t current_time = time(NULL);
    struct tm* time_info = localtime(&current_time);
    
    char filename[256];
    strftime(filename, sizeof(filename), "dump_file_%Y%m%d_%H%M%S.bin", time_info);

    FILE* dump_file = fopen(filename, "wb");
    if (dump_file == NULL) {
        printf("[ERROR] Failed to create dump file: %s\n", filename);
        return;
    }

    unsigned long long total_bytes_written = 0ULL;
    
    printf("\n[INFO] Dumping memory to file: %s\n", filename);
    
    for (int i = 0; i < allocation_count; ++i) {
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

    fclose(dump_file);

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

    printf("[SUCCESS] Memory dump completed:\n");
    printf("  File: %s\n", filename);
    printf("  Total Data Written: %.2f GB\n", size_gb);
    printf("  Wall-Clock Time: %.6f seconds\n", wall_time);
    printf("  CPU Time: %.6f seconds\n", cpu_time);
    printf("===============================================\n");
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
