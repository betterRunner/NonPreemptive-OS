#include "smart_memory.h"

#include <stdio.h>
#include <string.h>

struct MemBlock_t
{
  uint32_t           				nFree;
  struct MemBlock_t* 				pPre;
  struct MemBlock_t* 				pNext;
};

struct MemMgr_t
{
  uint8_t                   		nAlign;
  uint8_t                  			nAlignMask;
  uint32_t           				nFreeSum;
  uint32_t							nAddrStart;

  struct MemBlock_t* 				pFreeBlockList;
};

static struct MemMgr_t g_sMemMgr = {0};

/*
*********************************************************************************************************
* Description	: this function  pop the need block from the free block list.
*
* Arguments  	: pElement					the block that needed.
*			      nSizeNeed					Size of block.
*
* Return		: None.
*
* Note(s)   	: (1) should be used in Mem_malloc().
*
*********************************************************************************************************
*/
static void mem_popFreeBlockList(struct MemBlock_t *pElement, uint32_t nSizeNeed)
{
	struct MemBlock_t *block_next = pElement->pNext;
	struct MemBlock_t *block_pre = pElement->pPre;
	uint32_t size_left = pElement->nFree - nSizeNeed;

	if(size_left < g_sMemMgr.nAlign + sizeof(struct MemBlock_t)) // this free block only left space that not enough for next malloc, so remove whole block.
	{
		g_sMemMgr.nFreeSum -= pElement->nFree;

		if(block_pre != NULL) block_pre->pNext = block_next;
		else g_sMemMgr.pFreeBlockList = block_next;
		if(block_next != NULL) block_next->pPre = block_pre;
	}
	else // this free block left space that enough for next malloc, so resize the block.
	{
    g_sMemMgr.nFreeSum -= nSizeNeed;
    pElement->nFree = nSizeNeed;

    struct MemBlock_t *block_new = (struct MemBlock_t *)((uint32_t)pElement + nSizeNeed);
    block_new->nFree = size_left;
    block_new->pPre = block_pre;
    block_new->pNext = block_next;
    if(block_pre != NULL) block_pre->pNext = block_new;
    else g_sMemMgr.pFreeBlockList = block_new;
    if(block_next != NULL) block_next->pPre = block_new;
	}
}

/*
*********************************************************************************************************
* Description	: this function  push the block to free block list.
*
* Arguments  	: pElement					the block that push back.
*			 	  nSizeNeed					Size of the block.
*
* Return	    : None.
*
* Note(s)   	: (1) should be used in Mem_free().
*
*********************************************************************************************************
*/
static void mem_pushFreeBlockList(struct MemBlock_t *pElement)
{
	struct MemBlock_t *block_next = g_sMemMgr.pFreeBlockList;
	struct MemBlock_t *block_pre = NULL;
	while((block_next != NULL) && ((uint32_t)block_next < (uint32_t)pElement)) // used block list should be in order by address.
	{
		block_pre = block_next;
		block_next = block_next->pNext;
	}

	pElement->pPre = block_pre;
	pElement->pNext = block_next;
	if((block_next == NULL) && (block_pre == NULL)) // the free list is empty.
	{
		g_sMemMgr.pFreeBlockList = pElement;
	}
	else
	{
		if(block_next == NULL) // element push to the tail of list.
		{

		}
		else if((uint32_t)pElement + pElement->nFree == (uint32_t)block_next) // element can combile with the next block.
		{
			pElement->nFree += block_next->nFree;
			pElement->pNext = block_next->pNext;
		}
		else // element can not combile with the next block.
		{
			block_next->pPre = pElement;
		}

		if(block_pre == NULL) // element push to the head of list.
		{
			g_sMemMgr.pFreeBlockList = pElement;
		}
		else if((uint32_t)block_pre + block_pre->nFree == (uint32_t)pElement) // element can combile with the pre block.
		{
			block_pre->nFree += pElement->nFree;
			block_pre->pNext = pElement->pNext;
		}
		else // element can not combile with the pre block.
		{
			block_pre->pNext = pElement;
		}
	}
}

/*
*********************************************************************************************************
* Description	: this function takes the memory with user-design start address and size as the Memory Pool.
*
* Arguments  	: nAddr						Start address of Memory Pool.
*			  	  nSize						Size of Memory Pool.
*			  	  nAlign					Align bytes of Memory Pool.
*
* Return	    : return (0) if init success or (-1) if not.
*
* Note(s)   	: None.
*********************************************************************************************************
*/
int Mem_init(uint32_t nAddr, uint32_t nSize, uint8_t nAlign)
{
	uint32_t addr_start = nAddr;

	memset((void *)addr_start, 0, nSize);
	g_sMemMgr.nAlign = (nAlign == 0)? sizeof(uint32_t): nAlign;
	g_sMemMgr.nAlignMask = g_sMemMgr.nAlign - 1;

	if(addr_start & g_sMemMgr.nAlignMask) // make sure the address is aligned.
	{
		addr_start += g_sMemMgr.nAlignMask;
		addr_start &= ~g_sMemMgr.nAlignMask;
		nSize = (nSize < addr_start - nAddr)? 0: nSize - (addr_start - nAddr);
	}
	if(nSize >= g_sMemMgr.nAlign)
	{
		uint32_t addr_end = addr_start + nSize;
		addr_end &= ~g_sMemMgr.nAlignMask;
		g_sMemMgr.nAddrStart = addr_start;
		g_sMemMgr.nFreeSum = addr_end - addr_start;
		/* the first free block starts as the whole block of Memory Pool. */
		g_sMemMgr.pFreeBlockList = (void *)addr_start;
		g_sMemMgr.pFreeBlockList->nFree = g_sMemMgr.nFreeSum;
		g_sMemMgr.pFreeBlockList->pPre = NULL;
		g_sMemMgr.pFreeBlockList->pNext = NULL;

		return 0;
	}
	return -1;
}

/*
*********************************************************************************************************
* Description	: this function malloc a memory.
*
* Arguments  	: nSize						Size of memory user needs.
*
* Return		: None.
*
* Note(s)   	: None.
*********************************************************************************************************
*/
void* Mem_malloc(uint32_t nSize)
{
	struct MemBlock_t *block_need;

	if(nSize == 0) return NULL;
	if(g_sMemMgr.pFreeBlockList == NULL)  return NULL;

	nSize += sizeof(struct MemBlock_t); // each block contains a MemBlock_t struct to manage this block.
	if(nSize & g_sMemMgr.nAlignMask)
	{
	nSize += g_sMemMgr.nAlignMask;
	nSize &= ~g_sMemMgr.nAlignMask;
	}

	block_need = g_sMemMgr.pFreeBlockList;
	while((block_need != NULL) && (block_need->nFree < nSize)) // find the free block that large enough.
	{
		block_need = block_need->pNext;
	}
	if(block_need == NULL) return NULL;
	mem_popFreeBlockList(block_need, nSize);
	//mem_pushUsedBlockList(block_need);

	return (void *)&(block_need[1]); // return the space, (block head is not included).
}

/*
*********************************************************************************************************
* Description	: this function free a memory.
*
* Arguments  	: pMemory						pointer of memory needed to free.
*
* Return		: None.
*
* Note(s)   	: None.
*********************************************************************************************************
*/
void Mem_free(void *pMemory)
{
	struct MemBlock_t *block_need;
	if((uint32_t)pMemory < g_sMemMgr.nAddrStart + sizeof(struct MemBlock_t)) // the memory is not in the Memory Pool.
		return;

	block_need = (struct MemBlock_t *)((uint32_t)pMemory - sizeof(struct MemBlock_t));
	//mem_popUsedBlockList(block_need);
	mem_pushFreeBlockList(block_need);
}

/*
*********************************************************************************************************
* Description	: this function calloc a memory.
*
* Arguments  	: nSize							Size of memory needed to calloc.
*
* Return		: None.
*
* Note(s)   	: None.
*********************************************************************************************************
*/
void *Mem_calloc(uint32_t nSize)
{
  void *ret_memory;
  ret_memory = Mem_malloc(nSize);
  if(ret_memory != NULL)
  {
    memset(ret_memory, 0, nSize);
  }
  return ret_memory;
}

/*
*********************************************************************************************************
* Description	: this function relloc a memory.
*
* Arguments  	: nMemory						Address of original memory.
*			  	  nSize							Size of memory needed to relloc.
*
* Return		: return address of memory.
*
* Note(s)   	: None.
*********************************************************************************************************
*/
void* Mem_relloc(void *nMemory, uint32_t nSize)
{
	void *memory_ret = NULL;

	if((uint32_t)nMemory < g_sMemMgr.nAddrStart + sizeof(struct MemBlock_t)) // the memory is not in the Memory Pool.
		return NULL;

	struct MemBlock_t *block_original = (struct MemBlock_t *)((uint32_t) nMemory - sizeof(struct MemBlock_t));
	uint32_t size_original = block_original->nFree;
	uint32_t size_copy = (size_original > nSize)? nSize: size_original;

	memory_ret = Mem_malloc(size_original);
	if(memory_ret != NULL)
	{
		memmove(memory_ret, nMemory, size_copy);
	}
	Mem_free(nMemory);
	nMemory = NULL;

	return memory_ret;
}

/*
*********************************************************************************************************
* Description	: this function return the free size.
*
* Arguments  	: None.
*
* Return		: return free size.
*
* Note(s)   	: None.
*********************************************************************************************************
*/
uint32_t Memory_getFreeSize(void)
{
  return g_sMemMgr.nFreeSum;
}

/*
*********************************************************************************************************
* Description	: this function test the Mem_.
*
* Arguments  	: None.
*
* Return		: return (0) if no error, (<0) otherwise.
*
* Note(s)   	: (1) test1:
*                ------ ---------------------
*               |  m1  |          ...        |
*                ------ ---------------------
*                 ^
*                 |
*                ------
*               |  m2  |
*                ------
*               (2) test2:
*                      ^
*                      |
*                ----- ----- ----------------
*               |  m1 |  m2 |       ...      |
*                ----- ----- ----------------
*                 ^           ^
*                 |           |
*                -----       ----------
*               |  m3 |     |    m3    |
*                -----       ----------
*
*               (3) test3:
*
*                ^                 ^
*                |                 |
*                ----- ----- ----- ----- ------
*               |  m1 |  m2 |  m3 |  m4 | ...  |
*                ----- ----- ----- ----- ------
*                      ^                 ^
*                      |                 |
*                      -----------       --------------
*                     |     m5    |     |        m5    |
*                      -----------        -------------
*********************************************************************************************************
*/
int Mem_test(void)
{
  void *mem1, *mem2, *mem3, *mem4, *mem5;

  /* test1 */
  mem1 = Mem_malloc(7);
  Mem_free(mem1);
  mem2 = Mem_malloc(7);
  if((uint32_t)mem1 != (uint32_t)mem2)
  {
    return -1;
  }
  Mem_free(mem2);

  /* test2 */
#define	SIZE1             15
#define	SIZE2             30
  mem1 = Mem_malloc(SIZE1);
  mem2 = Mem_malloc(SIZE1);
  Mem_free(mem1);
  mem3 = Mem_malloc(SIZE1);
  if((uint32_t)mem1 != (uint32_t)mem3)
  {
    return -2;
  }
  Mem_free(mem2);
  Mem_free(mem3);
  mem1 = Mem_malloc(SIZE1);
  mem2 = Mem_malloc(SIZE1);
  Mem_free(mem1);
  mem3 = Mem_malloc(SIZE2);
  if((uint32_t)mem3 != (uint32_t)mem2 + (((SIZE1+sizeof(struct MemBlock_t))/g_sMemMgr.nAlign)+1)*g_sMemMgr.nAlign)
  {
    return -3;
  }
  Mem_free(mem2);
  Mem_free(mem3);

  /* test3 */
  mem1 = Mem_malloc(10);
  mem2 = Mem_malloc(10);
  mem3 = Mem_malloc(10);
  mem4 = Mem_malloc(10);
  Mem_free(mem2);
  Mem_free(mem3);
  mem5 = Mem_malloc(15);
  if((uint32_t)mem2 != (uint32_t)mem5)
  {
    return -4;
  }
  Mem_free(mem5);
  mem5 = Mem_malloc(48);
  if((uint32_t)mem2 == (uint32_t)mem5)
  {
    return -5;
  }
  Mem_free(mem1);
  Mem_free(mem4);
  Mem_free(mem5);

  return 0;
}