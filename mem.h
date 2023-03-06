#ifndef MEM_H_
#define MEM_H_

#define debug_malloc(X) my_debug_malloc( X, __FILE__, __LINE__, __FUNCTION__)
#define debug_free(X) my_debug_free( X )

void* my_debug_malloc(size_t size, const char *file, int line, const char *func);
void my_debug_free(void *ptr);

struct mem_addresses {
  void *ptr;
  char text[100];
  struct mem_addresses *next;
};

#endif
