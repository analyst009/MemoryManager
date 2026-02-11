#include <stdio.h>
#include "memory_manager.h"

static void display_menu(void) {
    printf("\n========== MEMORY ALLOCATION MANAGER ==========\n");
    printf("1. Allocate Memory (10 GB chunk)\n");
    printf("2. Show All Allocated Memory\n");
    printf("3. Dump All Memory\n");
    printf("4. Exit\n");
    printf("==============================================\n");
    printf("Enter your choice (1-4): ");
}

int main(void) {
    int choice;

    printf("Welcome to Memory Allocation Manager!\n");

    while (1) {
        display_menu();
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n'); /* clear invalid input */
            printf("[ERROR] Invalid input. Enter a number between 1 and 4.\n");
            continue;
        }

        switch (choice) {
            case 1:
                mm_allocate_chunk();
                break;
            case 2:
                mm_show_allocations();
                break;
            case 3:
                mm_dump_all_memory();
                break;
            case 4:
                printf("[INFO] Exiting application...\n");
                mm_free_all();
                return 0;
            default:
                printf("[ERROR] Invalid choice. Please enter 1, 2, 3, or 4.\n");
        }
    }
    return 0;
}
