std::queue<Packet*> dump_queue;

    for(int i = 0; i < allocation_count; ++i)
    {
        unsigned char * ptr = (unsigned char *)allocations[i].ptr;
        size_t chunk_size = CHUNK_SIZE;
        while(chunk_size)
        {
            Packet * pckt_ptr = new Packet;
            if(chunk_size > PACKET_SIZE)
                {
                    pckt_ptr->start_address = ptr;
                    pckt_ptr->size = PACKET_SIZE;
                    ptr = ptr + PACKET_SIZE;
                    chunk_size = chunk_size - PACKET_SIZE;

                }
            else
                {
                    pckt_ptr->start_address = ptr;
                    pckt_ptr->size = chunk_size;
                    chunk_size = 0;
                }
            dump_queue.push(pckt_ptr);
        }
    }


#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <stddef.h>

void mm_allocate_chunk(void);    /* Allocates a 2 GB chunk */
void mm_show_allocations(void);  /* Display each chunk on a single formatted row */
void mm_free_all(void);          /* Free all allocated chunks */
void mm_dump_all_memory(void);   /* Dump all memory allocations to the console */

#endif /* MEMORY_MANAGER_H */
