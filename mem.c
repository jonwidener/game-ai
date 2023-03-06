#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include "mem.h"

struct mem_addresses *g_mem_addresses = NULL;

void* my_debug_malloc(size_t size, const char *file, int line, const char *func)
{
  void *p = malloc(size);
    
  //printf("Allocated = %s, %i, %s, %p[%li]\n", file, line, func, p, size);

  //Link List functionality goes in here
  struct mem_addresses *new_address = (struct mem_addresses*)malloc(sizeof(struct mem_addresses));
  new_address->ptr = p;
  sprintf(new_address->text, "Allocated = %s, %i, %s, %p[%li]\n", file, line, func, p, size);
  new_address->next = g_mem_addresses;
  g_mem_addresses = new_address;    

  return p;
}

void my_debug_free(void *ptr) {  
  if (g_mem_addresses != NULL) {
    for (struct mem_addresses *cur = g_mem_addresses, *prev = NULL; cur != NULL; cur = cur->next) {  
      if (cur->ptr == ptr) {
        if (prev == NULL) {
          g_mem_addresses = cur->next;
        } else if (prev != NULL) {
          prev->next = cur->next;
        }      
        free(cur->ptr);
        free(cur);
        break;
      }
      prev = cur;
    }
  }
}
