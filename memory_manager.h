#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <stddef.h>

void mm_allocate_chunk(void);    /* Allocates a 2 GB chunk */
void mm_show_allocations(void);  /* Display each chunk on a single formatted row */
void mm_free_all(void);          /* Free all allocated chunks */
void mm_dump_all_memory(void);   /* Dump all memory allocations to the console */

#endif /* MEMORY_MANAGER_H */