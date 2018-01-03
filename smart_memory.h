#ifndef _SMART_MEMORY_H_
#define	_SMART_MEMORY_H_

#include <stdint.h>

int Mem_init(uint32_t nAddr, uint32_t nSize, uint8_t nAlign);
void* Mem_malloc(uint32_t nSize);
void Mem_free(void *pMemory);
void *Mem_calloc(uint32_t nSize);
void *Mem_relloc(void* pMemory, uint32_t nSize);
uint32_t Mem_getFreeSize(void);
int Mem_test(void);

#endif
