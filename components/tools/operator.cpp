#include <stdlib.h> // for malloc and free
void* operator new(unsigned int size) { return malloc(size); }
void operator delete(void* ptr) { if (ptr) free(ptr); }
