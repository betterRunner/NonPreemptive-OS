#include "nonOS.h"

#include <stdio.h>
#include <string.h>
#include "nonOS_common.h"
#include "os_cpu.h"
#include "smart_memory.h"

struct NOS_Msg_t
{
	enum NOS_Msg_e						MsgType;					// type of msg.
	uint32_t							nLength;					// length of msg.
	void*                           	pData;						// Pointer of data.
};

struct NOS_Evt_Sem_t
{
  uint8_t                         		nSemFree;					// Number of free sem, max 255.
};

struct NOS_Evt_MsgBox_t
{
	struct NOS_Evt_MsgBox_t*			p1stSend;					// Pointer of first msg to send.
	struct NOS_Evt_MsgBox_t*			pNext;						// Pointer of next msg.
	
	uint8_t								nWaitTaskCnt;				// Number of task waitting for one msg.
	struct NOS_Msg_t					sMsg;						// struct of meesage.
};

struct NOS_Evt_Timeout_t
{
	struct NOS_Evt_Timeout_t*			p1stElement;				// Pointer of first event timeout element.
	struct NOS_Evt_Timeout_t*			pNext;						// Pointer of next event timeout element.
	
	NOS_TASKNUM							nTaskPrio;					// Priority of task which owns this element.
	uint8_t								nIsTimeout;					// Is element reaches timeout.
	NOS_TICK							nTick;						// Tick count to reaches timeout.
};

struct NOS_Evt_t
{
	struct NOS_Evt_t*					p1stElement;				// Pointer of first event element.
	struct NOS_Evt_t*               	pNext;						// Pointer of next evnet element.
	
	uint8_t                         	nEvtType;					// Type of event.
	void*                           	pEvtObj;					// Pointer of object that event owns.
	struct NOS_Evt_Timeout_t*			pTimeout;					// List of pointer of timeout element. 
	struct NOS_Evt_t**					pAddr;						// Address of event ownner.
};

struct NOS_TaskInxList_t
{
	struct NOS_TaskInxList_t			*p1stElement;				// Pointer of first task index element.
	struct NOS_TaskInxList_t			*pNext;						// Pointer of next task index element.
	
	NOS_TASKNUM							nInx;						// Number of index (or priority).
}; 

struct NOS_Stack_t
{
	const uint8_t*                  	pSrc;						// Address of end of stack of task.
	uint16_t                        	nStack;						// Size of stack.
	uint8_t                         	arrStack[];					// Array to store stack value.
};


/*
*********************************************************************************************************
* Description	: These functions are memory allocating operations, you can replace it by you own memory
*                 management functions.
*
* Arguments  	: None.
*
* Return		: None.
*
* Note(s)   	: None.
*
*********************************************************************************************************/
#define __Nos_Mem_malloc					Mem_malloc
#define __Nos_Mem_calloc					Mem_calloc
#define __Nos_Mem_relloc					Mem_relloc
#define __Nos_Mem_free						Mem_free


/*
*********************************************************************************************************
* Description	: These functions are basic operations about list.
*
* Arguments  	: p1stElement				Pointer of 1st element of list.
*
*				  p1stElementAddr			Address of 1st element of list.
*								
*				  pElement					Pointer of element.
*
* Return		: None.
*
* Note(s)   	: (1) the input list should be in the form of NOS_List_t.
*
*********************************************************************************************************/
struct NOS_List_t
{
	struct NOS_List_t* p1stElement;
	struct NOS_List_t* pNext;
};
	
#define __nos_pushList(p1stElement, pElement) \
	do{ \
		pElement->pNext = p1stElement; \
		p1stElement = pElement; \
	} while(0)

#define __nos_popList(p1stElement) \
	do{ \
		if(p1stElement != NULL){ \
			void* mem_tmp = p1stElement; \
			p1stElement = p1stElement->pNext; \
			__Nos_Mem_free(mem_tmp); \
			mem_tmp = NULL; \
		} \
	} while(0)

#define __nos_deleteFromList(p1stElement, pElement) \
	do{ \
		if(p1stElement != NULL){ \
			struct NOS_List_t *ele_pre = NULL; \
			struct NOS_List_t *ele_cur = (struct NOS_List_t *)p1stElement; \
			while((ele_cur != NULL) && (ele_cur != (struct NOS_List_t *)pElement)){ \
				ele_pre = ele_cur; \
				ele_cur = ele_cur->pNext; \
			} \
			if(ele_cur != NULL){ \
				if(ele_pre == NULL){ \
					p1stElement = (void *)ele_cur->pNext; \
				} \
				else{ \
					ele_pre->pNext = ele_cur->pNext; \
				} \
				Mem_free(ele_cur); \
				ele_cur = NULL; \
			} \
		} \
	} while(0)
	
void nos_deleteFromList(struct NOS_List_t **p1stElementAddr, struct NOS_List_t *pElement)
{
	struct NOS_List_t *element_cur;
	struct NOS_List_t *element_pre;
	if((p1stElementAddr == NULL) || (pElement == NULL))
	{
		return;
	}
	element_cur = (*p1stElementAddr);
	element_pre = NULL;
	while(element_cur != NULL)
	{
		if(element_cur == pElement) 
		{
			break;
		}
		element_pre = element_cur;
		element_cur = element_cur->pNext;
	}
	if(element_pre == NULL)
	{
		(*p1stElementAddr) = NULL;
		Mem_free(pElement);
	}
	else if(element_cur != NULL)
	{
		element_pre->pNext = element_cur->pNext;
		Mem_free(pElement);
	}
}

/*
*********************************************************************************************************
* Description	: This function check if one task is in the task array.
*
* Arguments  	: arrTaskArray				Pointer of task array.
*
*				  nLen						Length of task array.
*
*				  nPrio						Priority of the task.
*
* Return		: return (-1) if task is not in the array, otherwise return the index in array.
*
* Note(s)   	: (1) OS call and you should not call it.
*
*********************************************************************************************************/
static int nos_isElementInTaskArray(struct NOS_Tcb_t* arrTaskArray[], NOS_TASKNUM nLen, NOS_TASKNUM nPrio)
{
  while(nLen)
  {
		nLen --;
    if(arrTaskArray[nLen]->nPrio == nPrio)
		{
      return nLen;
		}
  }
  return (-1);
}

/*
*********************************************************************************************************
* Description	: This function adjust the order of element of task array from tail of task array.
*
* Arguments  	: arrTaskArray				Pointer of task array.
*
*				  nInx						Index of element before adjustment.
*
* Return		: return index of element after adjustment.
*
* Note(s)   	: (1) Adjust method is according to the small root heap.
*
*				  (2) NOS_createTask(), NOS_deleteTask() and NOS_wakeupTask() will call it.
*
*				  (3) OS call and you should not call it.
*
*********************************************************************************************************/
static int nos_adjustTaskArrayFromTail(struct NOS_Tcb_t* arrTaskArray[], NOS_TASKNUM nInx)
{
  if(nInx > 0)
  {
    NOS_TASKNUM parent_index = (nInx - 1) >> 1;
    if(arrTaskArray[parent_index]->nPrio > arrTaskArray[nInx]->nPrio)
    {
      struct NOS_Tcb_t *pTmp = arrTaskArray[parent_index];
      arrTaskArray[parent_index] = arrTaskArray[nInx];
      arrTaskArray[nInx] = pTmp;
      nInx = nos_adjustTaskArrayFromTail(arrTaskArray, parent_index);
    }
  }
  return nInx;
}

/*
*********************************************************************************************************
* Description	: This function adjust the order of element of task array from head of task array.
*
* Arguments  	: arrTaskArray				Pointer of task array.
*
*				  nInx						Index of element before adjustment.
*
*				  nLen						Length of task array.
*
* Return		: return index of element after adjustment.
*
* Note(s)   	: (1) Adjust method is according to the small root heap.
*
*				  (2) NOS_runReadyTask() will call it.
*
*				  (3) OS call and you should not call it.
*
*********************************************************************************************************/
static int nos_adjustTaskArrayFromHead(struct NOS_Tcb_t* arrTaskArray[], NOS_TASKNUM nInx, NOS_TASKNUM nLen)
{
  if(nLen > 1)
	{
		NOS_TASKNUM child_index = (nInx << 1) + 1;
		if(child_index < nLen)
		{
			NOS_TASKNUM min_index = nInx;
			if(arrTaskArray[child_index]->nPrio < arrTaskArray[nInx]->nPrio)
			{
				min_index = child_index;
			}
			child_index ++;
			if((child_index < nLen) && (arrTaskArray[child_index]->nPrio < arrTaskArray[min_index]->nPrio))
			{
				min_index = child_index;
			}
			if(min_index != nInx)
			{
				struct NOS_Tcb_t *tmp_tcb = arrTaskArray[min_index];
				arrTaskArray[min_index] = arrTaskArray[nInx];
				arrTaskArray[nInx] = tmp_tcb;
				nInx = nos_adjustTaskArrayFromHead(arrTaskArray, min_index, nLen);
			}
		}
	}
  return nInx;
}

/*
*********************************************************************************************************
* Description	: This function use to wakeup (put in ready task array) the task.
*
* Arguments  	: nInx						Index of task in task array.
*
* Return		: return index of task after adjustment.
*
* Note(s)   	: (1) Only nos_runWakeupTask() will call it.
*
*				  (2) OS call and you should not call it.
*
*********************************************************************************************************/
static int nos_wakeupTask(int nInx)
{
	struct NOS_InnerMgr_t *task_mgr = NOS_getInnerMgr();
	
	if(nInx >= task_mgr->nTaskRdy)
	{
		if(nInx > task_mgr->nTaskRdy)
		{
			struct NOS_Tcb_t *tmp_tcb = task_mgr->arrTaskTcb[nInx];
			task_mgr->arrTaskTcb[nInx] = task_mgr->arrTaskTcb[task_mgr->nTaskRdy];
			task_mgr->arrTaskTcb[task_mgr->nTaskRdy] = tmp_tcb;
		}
		nInx = nos_adjustTaskArrayFromTail(task_mgr->arrTaskTcb, task_mgr->nTaskRdy);
		(task_mgr->nTaskRdy) ++;
	}
	return nInx;
}

/*
*********************************************************************************************************
* Description	: This function wakeup task right now or later.
*
* Arguments  	: nInx						Index of task in task array.
*
* Return		: return the index of task after adjustment.
*
* Note(s)   	: (1) When task_mgr->bPending = 0 it will just wakeup the task, otherwise means function 
*					  NOS_delayTick() is called, then the task that reach timeout will not wakeup right 
*					  now ,instead we will store the inx of waitting tasks to a list, and wakeup all of 
*					  them at the end of NOS_delayTick().
*
*				  (2) OS will call this function in below case:
*
*					(a) nos_sendEvt() to wake up the task that waiting for this event.
*
*					(b) NOS_deleteEvt() to wakeup the task that waiting for this event.
*
*					(c) NOS_onSysTick() to wakeup the task whose tick wait reaches.
*
*				  (3) OS call it and you should not call it.
*
*********************************************************************************************************/
static int nos_runWakeupTask(int nInx)
{
	struct NOS_InnerMgr_t *task_mgr = NOS_getInnerMgr();
	int ret = nInx;
	uint8_t b_created = 0;
	
	if(task_mgr->bPending == 0) // Wake up right now.
	{
		ret = nos_wakeupTask(nInx);
	}
	else // Wake up later.
	{
		struct NOS_TaskInxList_t *ele_cur = task_mgr->pWakeupTaskInxList->p1stElement;
		while(ele_cur != NULL)
		{
			if(ele_cur->nInx == nInx) // Do not create element with the same task inx.
			{
				b_created = 1;
				break;
			}				
			ele_cur = ele_cur->pNext;
		}
		if(b_created == 0)
		{
			struct NOS_TaskInxList_t *ele_new = __Nos_Mem_calloc(sizeof(struct NOS_TaskInxList_t));
			if(ele_new != NULL)
			{
				ele_new->nInx = nInx;
				__nos_pushList(task_mgr->pWakeupTaskInxList->p1stElement, ele_new);
			}
		}
	}
	
	return ret;
}

/*
*********************************************************************************************************
* Description	: This function get the index of task in task array that waiting for this event.
*
* Arguments  	: pEvt						Pointer of event to wait.
*
* Return		: return (-1) if no task waitting otherwise the index of task.
*
* Note(s)   	: (1) OS call it and you should not call it.
*
*********************************************************************************************************/
static int nos_getWaittingTaskIndex(struct NOS_Evt_t *pEvt)
{
	struct NOS_InnerMgr_t *task_mgr = NOS_getInnerMgr();
	int m = (-1);
	
	if(pEvt == NULL)
	{
		return (-1);
	}
	for(m=task_mgr->nTaskRdy; m<task_mgr->nTaskAll; m++)
	{
		struct NOS_Evt_t *ele_cur = task_mgr->arrTaskTcb[m]->pEvtWait;

		if(pEvt == ele_cur) 
		{
			break;
		}
	}
	m = (m == task_mgr->nTaskAll)? (-1): m;
	return m;
}

/*
*********************************************************************************************************
* Description	: This function release the space that this event creates.
*
* Arguments  	: pEvt						Pointer of event to release.
*
* Return		: None.
*
* Note(s)   	: (1) OS call it and you should not call it.
*
*********************************************************************************************************/
static void nos_releaseEvt(struct NOS_Evt_t *pEvt)
{
	struct NOS_InnerMgr_t *task_mgr = NOS_getInnerMgr();
	
	switch(pEvt->nEvtType)
	{
		case NOS_EVT_MsgBox:
			{
				struct NOS_Evt_MsgBox_t *msgbox = pEvt->pEvtObj;
				if(msgbox != NULL)
				{
					struct NOS_Evt_MsgBox_t *msg_cur = msgbox->p1stSend;
					while(msg_cur != NULL)
					{
						Mem_free(msg_cur);
						msg_cur = msg_cur->pNext;
					}
					msgbox->p1stSend = NULL;
				}
			}
			break;
		default:
			break;
	}
	Mem_free(pEvt->pTimeout);
	pEvt->pTimeout = NULL;
	Mem_free(pEvt);
}

/*
*********************************************************************************************************
* Description	: This function check if event reaches timeout and delete it from event list if so.
*
* Arguments  	: pEvt						Pointer of waitting event.
*
* Return		: return 0 if event does not reach timeout, otherwise return 1.
*
* Note(s)   	: None.
*
*********************************************************************************************************/
int nos_isEvtReachTimeout(struct NOS_Evt_t* pEvt)
{
	struct NOS_InnerMgr_t *task_mgr = NOS_getInnerMgr();
	int ret = 0;
	
	struct NOS_Evt_Timeout_t *ele_cur = pEvt->pTimeout->p1stElement;
	while(ele_cur != NULL)
	{
		if(ele_cur->nTaskPrio == task_mgr->pCurTcb->nPrio)
		{
			break;
		}
		ele_cur = ele_cur->pNext;
	}
	if((ele_cur != NULL) && (ele_cur->nIsTimeout == 1))
	{
		__nos_deleteFromList(pEvt->pTimeout->p1stElement, ele_cur);
		ret = 1;
	}
	return ret;
}

/*
*********************************************************************************************************
* Description	: This function renew the waitting timeout list.
*
* Arguments  	: pEvt						Pointer of waitting event.
*
*				  nTaskPrio					Priority of waitting task.
*
*				  nTimeout					Waitting timeout.
*
* Return		: None.
*
* Note(s)   	: None.
*
*********************************************************************************************************/
void nos_renewEvtTimeoutList(struct NOS_Evt_t* pEvt, NOS_TASKNUM nTaskPrio, NOS_TICK nTimeout)
{
	struct NOS_Evt_Timeout_t *ele_cur = pEvt->pTimeout->p1stElement;
	int b_created = 0;
	
	while(ele_cur != NULL)
	{
		if(ele_cur->nTaskPrio == nTaskPrio)
		{
			ele_cur->nTick = nTimeout;
			b_created = 1;
			break;
		}
		ele_cur = ele_cur->pNext;
	}

	if(b_created == 0) // Has not been created, create a new one.
	{
		struct NOS_Evt_Timeout_t *ele_new = __Nos_Mem_calloc(sizeof(struct NOS_Evt_Timeout_t));
		if(ele_new != NULL)
		{
			ele_new->nTick = nTimeout;
			ele_new->nTaskPrio = nTaskPrio;
			__nos_pushList(pEvt->pTimeout->p1stElement, ele_new);
		}
	}
}

/*
*********************************************************************************************************
* Description	: This function send the event and wakeup the task that waitting.
*
* Arguments  	: pEvt						Pointer of event to send.
*
*				  eMsgType					Type of msg in memory aspect.
*
*				  pMsg						Pointer of msg if event contains msg.
*
* Return		: NOS_ERROR_None			No error.
*				  NOS_ERROR_NullPointer		Pointer of event is null.
*
* Note(s)   	: (1) __NOS_sendSem() and __NOS_sendMsgBox() will call it.
*
*				  (2) If sending MsgBox, you can send it to multi tasks, we use nWaitTaskCnt to record
*					  how many tasks are waitting for this MsgBox.
*
*				  (3) OS call it and you should not call it.
*
*********************************************************************************************************/
int nos_sendEvt(struct NOS_Evt_t *pEvt, enum NOS_Msg_e eMsgType, void *pMsg)
{
	struct NOS_InnerMgr_t *task_mgr = NOS_getInnerMgr();
	int ret = NOS_ERROR_None;
	
	if(pEvt == NULL)
	{
		return NOS_ERROR_NullPointer;
	}
	
	switch(pEvt->nEvtType)
	{
		case NOS_EVT_Sem:
			{
				struct NOS_Evt_Sem_t *sem = pEvt->pEvtObj;
				if(sem != NULL)
				{
					int task_index;
					if((task_index = nos_getWaittingTaskIndex(pEvt)) > (-1)) // If one task is waitting this event, wake it up.
					{
						nos_runWakeupTask(task_index);
					}
					sem->nSemFree = (sem->nSemFree < 255)? sem->nSemFree + 1: 255;
				}
			}
			break;
		case NOS_EVT_MsgBox:
			{
				struct NOS_Evt_MsgBox_t *pMsgBox = pEvt->pEvtObj;
				if(pMsgBox != NULL)
				{				
					int task_index, wait_cnt = 0;
					while((task_index = nos_getWaittingTaskIndex(pEvt)) > -1) // If tasks are waitting this event, wake them up.
					{
						nos_runWakeupTask(task_index);
						wait_cnt ++; // Record how many task are waitting.
					}	
					if(wait_cnt > 0) // Only if any task is waitting for this msg will sent.
					{
						struct NOS_Evt_MsgBox_t *msgbox = __Nos_Mem_calloc(sizeof(struct NOS_Evt_MsgBox_t));
						if(msgbox != NULL)
						{
							msgbox->nWaitTaskCnt = wait_cnt;
							msgbox->sMsg.eMsgType = eMsgType;
							msgbox->sMsg.pData = pMsg;
							__nos_pushList(pMsgBox->p1stSend, msgbox);
						}
						else
						{
							ret = NOS_ERROR_NullMemory;
						}
					}
				}
			}
			break;
		default:
			break;
	}
	return ret;
}

/*
*********************************************************************************************************
* Description	: This function wait an event with a timeout.
*
* Arguments  	: pEvt						Pointer of event to wait.
*
*				  nTimeout					Wait timeout, (-1) means wait forever, 0 means not wait.
*
*				  pMsgAddr					Pointer of address to store msg.
*
* Return		: NOS_ERROR_None			No error.
*				  NOS_ERROR_NullPointer		Pointer of event is null.
*				  NOS_ERROR_NullTcb			No task is running
*				  NOS_ERROR_Pended			Task do not recv the event so pend up.
*				  NOS_ERROR_NullMemory		Memory is not enough.
*				  NOS_ERROR_NullEvt			Only when timeout is 0 and wait evt is not ready.
*
* Note(s)   	: (1) If having not received the event but reach timeout, will not pend up the task.
*
*				  (2) __nos_waitObj() will call it.
*
*				  (3) OS call it and you should not call it.
*
*********************************************************************************************************/
int nos_waitEvt(struct NOS_Evt_t* pEvt, NOS_TICK nTimeout, void ** pMsgAddr)
{
  struct NOS_InnerMgr_t *task_mgr = NOS_getInnerMgr();
	int b_timeout = 0;
  int ret = NOS_ERROR_None;
	static int b_readLock = 0;
	
	if(pEvt == NULL) return NOS_ERROR_NullEvt;
  if(task_mgr->pCurTcb == NULL) return NOS_ERROR_NullTcb;
	
	if(pEvt->nEvtType != NOS_EVT_None) // If the evt is valid.
	{
		b_timeout = nos_isEvtReachTimeout(pEvt);
		
		if(b_timeout == 0) // Evt is not called by timeout.
		{
			if(nTimeout > 0) // If new set timeout is bigger than 0, renew the list, so if timeout is (-1) it would not pu into timeout list.
			{
				nos_renewEvtTimeoutList(pEvt, task_mgr->pCurTcb->nPrio, nTimeout);
			}
			
			ret = (nTimeout == 0)? NOS_ERROR_NullEvt: NOS_ERROR_Pended;

			switch(pEvt->nEvtType)
			{
				case NOS_EVT_Sem:
					{
						if(b_readLock != 1) // One task can not get the same sem unless it resumes again.
						{
							struct NOS_Evt_Sem_t *sem = pEvt->pEvtObj;
							if((sem != NULL) && (sem->nSemFree > 0))
							{
								(sem->nSemFree) --;											
								ret = NOS_ERROR_None;
							}
							b_readLock = 1;
						}
					}
					break;
				case NOS_EVT_MsgBox:
					{		
						if(b_readLock != 2) // One task can not read the same msg unless it resumes again.
						{
							struct NOS_Evt_MsgBox_t *msgbox = pEvt->pEvtObj;			
							if((msgbox != NULL) && (msgbox->p1stSend != NULL)) // msg is sent.
							{					
								if(pMsgAddr != NULL) // get the msg and its type.
								{
									ret = NOS_ERROR_None;
									(*pMsgAddr) = Mem_malloc(msgbox->p1stSend->sMsg.nLength);
									if((*pMsgAddr) != NULL)
									{
										memmove((*pMsgAddr), msgbox->p1stSend->sMsg.pData, msgbox->p1stSend->sMsg.nLength);
									}
									else
									{
										ret = NOS_ERROR_NullMemory;
									}
								}
								msgbox->p1stSend->nWaitTaskCnt = (msgbox->p1stSend->nWaitTaskCnt > 0)? (msgbox->p1stSend->nWaitTaskCnt - 1): 0;					
								if((msgbox->p1stSend->nWaitTaskCnt) == 0)
								{
									if(msgbox->p1stSend->sMsg.eMsgType == NOS_MSG_RecvFree)
									{ // if msg type is "NOS_MSG_RecvFree" and all waitting tasks read it, should free the memory.
										Mem_free(msgbox->p1stSend->sMsg.pData);
										msgbox->p1stSend->sMsg.pData = NULL;
									}
									__nos_popList(msgbox->p1stSend);
								}
								
								b_readLock = 2;
							}
						}
					}
					break;
				default:
					break;
			}
		}
			
		if((ret == NOS_ERROR_None) || (b_timeout == 1)) // Recv the msg or reach the timeout
		{		
			task_mgr->pCurTcb->pEvtWait = NULL;
		}
		else if(ret == NOS_ERROR_Pended) // Task needs to pend, put the evt into the task and push the task back into task array.
		{
			task_mgr->pCurTcb->pEvtWait = pEvt;
			__nos_pushTaskBackToArray();
			
			b_readLock = 0; // Task pends up, can unlock the read lock.
		}
	}
	
	return ret;
}

/*
*********************************************************************************************************
* Description	: This function store the value of stack of one task when pends up.
*
* Arguments  	: pCurTcb					Tcb of running task.
*
*				  pVars						Address of start of stack.
*
*				  nCountOfBytes				Size of stack.
*
* Return		: NOS_ERROR_None			No error.
*				  NOS_ERROR_NullMemory		Not enough space.
*
* Note(s)   	: (1) __nos_storeTaskInfo() will use it.
*
*				  (2) To make this work (let the compiler not optimizing it), the value you create in task
*					  should be the type of 'volatile'.
*
*				  (3) OS call it and you should not call it.
*
*********************************************************************************************************/
int nos_storeStackValue(struct NOS_Tcb_t *pCurTcb, const void* pVars, int nCountOfBytes)
{
  if((pCurTcb->pStack != NULL) && (pCurTcb->pStack->nStack != nCountOfBytes))
  {
    Mem_free(pCurTcb->pStack);
    pCurTcb->pStack = NULL;
  }
  if(nCountOfBytes > 0)
  {
    if(pCurTcb->pStack == NULL)
    {
      pCurTcb->pStack = __Nos_Mem_calloc(sizeof(struct NOS_Stack_t) + nCountOfBytes);
      if(pCurTcb->pStack == NULL)
      {
        return NOS_ERROR_NullMemory;
      }
      pCurTcb->pStack->nStack = nCountOfBytes;
    }
    pCurTcb->pStack->pSrc = pVars;
    memcpy(pCurTcb->pStack->arrStack, pVars, pCurTcb->pStack->nStack);
  }
	
	return NOS_ERROR_None;
}
 
/*
*********************************************************************************************************
* Description	: This function restore the value of stack of one task when resuming.
*
* Arguments  	: pCurTcb					Tcb of the running task.
*
*				  pVarsEnd					Address of end of stack.
*
* Return		: NOS_ERROR_None			No error.
*				  NOS_ERROR_NullStack		The task does not store stack value.
*
* Note(s)   	: (1) __nos_pendTask() will use it.
*
*				  (2) OS will use it and you should not use it.
*
*********************************************************************************************************/
int nos_restoreStackValue(struct NOS_Tcb_t *pCurTcb, void* pVarsEnd) 
{
	if(pCurTcb->pStack == NULL)
	{
		return NOS_ERROR_NullStack;
	}
	
	memcpy((uint8_t *)pVarsEnd - pCurTcb->pStack->nStack, pCurTcb->pStack->arrStack, pCurTcb->pStack->nStack);
	return NOS_ERROR_None;
}

/*
*********************************************************************************************************
* Description	: This function calculte the cpu usage percentage of each task.
*
* Arguments  	: None.
*
* Return		: None.
*
* Note(s)   	: (1) Calculate form: CpuUsage = nTaskTick / nOSTick.
*
*********************************************************************************************************/
void nos_calTaskCpuUsageRatio(void)
{
	struct NOS_InnerMgr_t *task_mgr = NOS_getInnerMgr();
	NOS_TASKNUM m;
	
	__NOS_lockTaskMgr();
	for(m=0; m<task_mgr->nTaskAll; m++)
	{
		task_mgr->arrTaskTcb[m]->nCpuUsageRatio = (task_mgr->arrTaskTcb[m]->nTickCnt * 100) / task_mgr->nTickCnt;
	}
	__NOS_unlockTaskMgr();
}

/*
*********************************************************************************************************
* Description	: This function return the pointer of the manager struct that manage the OS.
*
* Arguments  	: None.
*
* Return		: return pointer of manager sturct.
*
* Note(s)   	: None.
*
*********************************************************************************************************/
struct NOS_InnerMgr_t *NOS_getInnerMgr(void)
{
  static struct NOS_InnerMgr_t s_instance = {0};
	
	if(s_instance.bInited == 0)
	{
		s_instance.pWakeupTaskInxList = __Nos_Mem_calloc(sizeof(struct NOS_TaskInxList_t));
		
		s_instance.bInited = 1;
	}

  return &s_instance;
};

/*
*********************************************************************************************************
* Description	: This function create task by user.
*
* Arguments  	: pTask						Pointer of function of task, see NOS_Task.
*
*				  pUser						Some msg of user that want to give this task.
*
*				  nPrio						Priority of this task, 0 means highest priority.
*											Also, nPio should be smaller than NOS_MAX_TASKNUM - 1.
*
* Return		: NOS_ERROR_None			No error.
*				  NOS_ERROR_NullTaskFunc	Pointer of task func is null.
*				  NOS_ERROR_WrongPrio		Priority of task is illegal.
*				  NOS_ERROR_FullTaskList	Task array is full.
*				  NOS_ERROR_NullMemory		Not enough memory.
*
* Note(s)   	: None.
*
*********************************************************************************************************/
int NOS_createTask(NOS_Task pTask, void* pUser, NOS_TASKNUM nPrio)
{
  int ret = NOS_ERROR_None;
  struct NOS_InnerMgr_t *task_mgr = NOS_getInnerMgr();
  struct NOS_Tcb_t *task_tcb = NULL;

	if(pTask == NULL)
	{
		return NOS_ERROR_NullTaskFunc;
	}
  if(nPrio >= NOS_MAX_TASKNUM)
  {
    return NOS_ERROR_WrongPrio;
  }

	__NOS_lockTaskMgr();
  ret = NOS_ERROR_FullTaskList;
  if(task_mgr->nTaskAll <= NOS_MAX_TASKNUM)
  {
    ret = NOS_ERROR_NullMemory;
    task_tcb = Mem_calloc(sizeof(struct NOS_Tcb_t));
    if(task_tcb != NULL) 
    {
      task_tcb->nPrio = nPrio;
      task_tcb->pUser = pUser;
      task_tcb->pTask = pTask;
			task_tcb->nCodeLine = (-1); // -1 means no jumping to other code line.
			task_tcb->nTickToWait = 0; // 0 means no need to wait, -1 means wait forever.
    }
    if(task_mgr->nTaskAll > task_mgr->nTaskRdy) // Put in task list. 
    {
      task_mgr->arrTaskTcb[task_mgr->nTaskAll] = task_mgr->arrTaskTcb[task_mgr->nTaskRdy];
    }
    (task_mgr->nTaskAll) ++;
    task_mgr->arrTaskTcb[task_mgr->nTaskRdy] = task_tcb;
    nos_adjustTaskArrayFromTail(task_mgr->arrTaskTcb, task_mgr->nTaskRdy);
    (task_mgr->nTaskRdy) ++;

    ret = 0;
  }
	__NOS_unlockTaskMgr();

  return ret;
}

/*
*********************************************************************************************************
* Description	: This function delete task from task array.
*
* Arguments  	: nPrio						Priority of task.
*
* Return		: NOS_ERROR_None			No error.
*				  NOS_ERROR_InvalidOper		Should not call this function in ISR or while task 
*											is running.
*
* Note(s)   	: (1) Task can be deleted from other task, but can not from ISR or task itself.
*
*				  (2) If task is the only source of one event that other tasks are waitting, you should 
*					  delete the event by yourself by calling NOS_deleteEvt(). 
*
*********************************************************************************************************/
int NOS_deleteTask(NOS_TASKNUM nPrio) 
{
	struct NOS_InnerMgr_t *task_mgr = NOS_getInnerMgr();
	struct NOS_Tcb_t *task_tcb = NULL;
	NOS_TASKNUM task_inx = 0, task_inx2 = 0;
	int nRet = NOS_ERROR_None;
		
	if(task_mgr->nIntNested > 0) // Should not call in ISR.
	{
		return NOS_ERROR_InvalidOper;
	}
	if((task_mgr->pCurTcb != NULL) && (task_mgr->pCurTcb->nPrio == nPrio)) // Should not call in task itself.
  {
    return NOS_ERROR_InvalidOper;
  }

	if((task_inx = nos_isElementInTaskArray(task_mgr->arrTaskTcb, task_mgr->nTaskAll, nPrio)) != (-1)) // Task is in the list.
  {
    task_tcb = task_mgr->arrTaskTcb[task_inx];
		__NOS_lockTaskMgr();
    if((task_inx2 = nos_isElementInTaskArray(task_mgr->arrTaskTcb, task_mgr->nTaskRdy, nPrio)) != (-1))
    { // If task is in ready list, need to reconstruct the task array.
      NOS_TASKNUM m;
      task_tcb = task_mgr->arrTaskTcb[task_inx2];
			(task_mgr->nTaskRdy) --;
			if(task_mgr->nTaskRdy > 0)
			{
				task_mgr->arrTaskTcb[task_inx2] = task_mgr->arrTaskTcb[task_mgr->nTaskRdy];
			}
      for(m=0; m<task_mgr->nTaskRdy; m++)
      {
        nos_adjustTaskArrayFromTail(task_mgr->arrTaskTcb, m);
      }
      task_inx = task_mgr->nTaskRdy;
    }
    memmove(task_mgr->arrTaskTcb + task_inx, task_mgr->arrTaskTcb + task_inx + 1, NOS_MAX_TASKNUM - task_inx - 1);
    (task_mgr->nTaskAll) --;
		__NOS_unlockTaskMgr();
  }
  else // Task is not in the array.
  {
    nRet = NOS_ERROR_WrongPrio;
  }

  if(task_tcb != NULL)
  {	
		memset(task_tcb->pStack->arrStack, 0, (task_tcb->pStack->nStack));
		Mem_free(task_tcb->pStack);
		memset(task_tcb, 0, sizeof(struct NOS_Tcb_t));
		Mem_free(task_tcb);
		task_tcb = NULL;
  }
	return nRet;
}

/*
*********************************************************************************************************
* Description	: this function create an event by user.
*
* Arguments  	: eType						Type of event you want to create, see enum NOS_EvtType_e.
*
*				  pEvtAddr					Address of event, value it and also record the address.
*
*				  pOthers					The content of an event if it needs (such as the free number
*											of sem for sem event).
*
* Return		: NOS_ERROR_None			No error.
*				  NOS_ERROR_WrongParm		Wrong event type.
*				  NOS_ERROR_NullMemory		Memory is not enough.
*
* Note(s)   	: (1) The space of event is malloc here, so if you dont't use the event, remember to use
*					  NOS_deleteEvt() to free the space.
*
*********************************************************************************************************/
int NOS_createEvt(enum NOS_EvtType_e eType, struct NOS_Evt_t **pEvtAddr, void* pOthers)
{
	struct NOS_InnerMgr_t *task_mgr = NOS_getInnerMgr();
	struct NOS_Evt_t *evt = NULL;
	void *obj = NULL;
	int ret = NOS_ERROR_None;
	
	if(pEvtAddr == NULL)
	{
		return NOS_ERROR_NullPointer;
	}
	if(eType >= NOS_EVT_NUM)
	{
		return NOS_ERROR_WrongParm;
	}

	(*pEvtAddr) = NULL;
	__NOS_lockTaskMgr();
	ret = NOS_ERROR_NullMemory;
  evt = __Nos_Mem_calloc(sizeof(struct NOS_Evt_t));
	if(evt != NULL)
	{
		evt->pAddr = pEvtAddr;
		evt->pTimeout = __Nos_Mem_calloc(sizeof(struct NOS_Evt_Timeout_t));
		switch(eType)
		{
			case NOS_EVT_Sem:
				{
					struct NOS_Evt_Sem_t *pSem = __Nos_Mem_calloc(sizeof(struct NOS_Evt_Sem_t));
					if(pSem != NULL)
					{
						pSem->nSemFree = (int)pOthers; // number of sem.
						obj = pSem;
					}
				}
				break;
			case NOS_EVT_MsgBox:
				{
					struct NOS_Evt_MsgBox_t *pMsgBox = __Nos_Mem_calloc(sizeof(struct NOS_Evt_MsgBox_t));
					if(pMsgBox != NULL)
					{
						obj = pMsgBox;
					}
				}
				break;
			default:
				break;
		}
		if(obj != NULL)
		{
			evt->nEvtType = eType;
			evt->pEvtObj = obj;
			(*pEvtAddr) = evt;
			ret = NOS_ERROR_None;
		}
	}
	__NOS_unlockTaskMgr();
	
  return ret;
}

/*
*********************************************************************************************************
* Description	: This function delete the created event.
*
* Arguments  	: pEvt						Addr of event to delete.
*
* Return		: NOS_ERROR_None			No error.
*				  NOS_ERROR_NullPointer		Event is null.
*
* Note(s)   	: (1) Before the event being deleted, it will wakeup all task that waitting for it.
*
*********************************************************************************************************/
int NOS_deleteEvt(struct NOS_Evt_t **pEvtAddr)
{
	struct NOS_InnerMgr_t *task_mgr = NOS_getInnerMgr();
	struct NOS_Evt_t *pEvt;
	int inx_task;
	int nRet = NOS_ERROR_None;
	
	if(pEvtAddr == NULL)
	{
		return NOS_ERROR_NullPointer;
	}
	pEvt = (*pEvtAddr);
	if(pEvt == NULL)
	{
		return NOS_ERROR_NullEvt;
	}
	
	__NOS_lockTaskMgr();
	/* Wakeup waitting tasks. */
	while((inx_task = nos_getWaittingTaskIndex(pEvt)) > -1)
	{
		nos_runWakeupTask(inx_task);
		task_mgr->arrTaskTcb[inx_task]->pEvtWait = NULL;
	}
	__NOS_unlockTaskMgr();
	
	nos_releaseEvt(pEvt);
	(*pEvtAddr) = NULL;
	
	return nRet;
}

/*
*********************************************************************************************************
* Description	: This function do a tick delay while the OS will be pend up temperorily.
*
* Arguments  	: nTick						Tick to delay.
*
*				  func						Function you want to do while waitting the delay.
*
* Return		: NOS_ERROR_None			No error.
*
*				  NOS_ERROR_InvalidOper		Error operation: call this function in ISR.
*
* Note(s)   	: (1) This function will pend up OS temperorily, but the ISR are still working, so system
*					  tick will go on but all task that reach timeout will resumes when delay finishes, 
*					  and if you send event in ISR, it will also wakeup tasks when delay finishes.
*
*				  (2) The func should not operates anything about OS. You can enter low power and so on.
*
*				  (3) When delay finishes, we will wakeup all tasks and free the space where created to 
*					  store the inx of them.
*
*********************************************************************************************************/
int NOS_delayTick(NOS_TICK nTick, NOS_Func func)
{
	struct NOS_InnerMgr_t *task_mgr = NOS_getInnerMgr();
	int ret = NOS_ERROR_None;
	
	if(task_mgr->nIntNested > 0) // Should not call in ISR.
	{
		return NOS_ERROR_InvalidOper;
	}
	__NOS_lockTaskMgr();
	task_mgr->bRunning = 0;
	task_mgr->bPending = 1;
	task_mgr->nDelayTickCnt = nTick;
	task_mgr->pWakeupTaskInxList = __Nos_Mem_calloc(sizeof(struct NOS_TaskInxList_t));
	__NOS_unlockTaskMgr();
	
	while(task_mgr->nDelayTickCnt > 0)
	{
		if((func != NULL) && (task_mgr->bRunning == 0))
		{
			func();
		}
	}
	
	__NOS_lockTaskMgr();
	task_mgr->bPending = 0;
	task_mgr->bRunning = 1;
	while(task_mgr->pWakeupTaskInxList->p1stElement != NULL)
	{
		nos_runWakeupTask(task_mgr->pWakeupTaskInxList->p1stElement->nInx);
		__nos_popList(task_mgr->pWakeupTaskInxList->p1stElement);
	}
	Mem_free(task_mgr->pWakeupTaskInxList);
	task_mgr->pWakeupTaskInxList = NULL;
	__NOS_unlockTaskMgr();
	
	return ret;
}

/*
*********************************************************************************************************
* Description	: This function resume the task that are ready.
*
* Arguments  	: None.
*
* Return		: retrun (-1) if no ready task otherwise the priority of task.
*
* Note(s)   	: (1) This function should be use together with NOS_onIdle(),
*					  that is if NOS_runReadyTask() return (-1) then you should run NOS_onIdle().
*
*********************************************************************************************************/
int NOS_runReadyTask(void)
{
	struct NOS_InnerMgr_t *task_mgr = NOS_getInnerMgr();
	
	__NOS_lockTaskMgr();
	if(task_mgr->nTaskRdy > 0)
	{
		task_mgr->pCurTcb = task_mgr->arrTaskTcb[0];
		(task_mgr->nTaskAll) --;
		(task_mgr->nTaskRdy) --;
		task_mgr->arrTaskTcb[0] = task_mgr->arrTaskTcb[task_mgr->nTaskRdy];
		task_mgr->arrTaskTcb[task_mgr->nTaskRdy] = task_mgr->arrTaskTcb[task_mgr->nTaskAll];
		task_mgr->arrTaskTcb[task_mgr->nTaskAll] = NULL;
		nos_adjustTaskArrayFromHead(task_mgr->arrTaskTcb, 0, task_mgr->nTaskRdy);
	}
	__NOS_unlockTaskMgr();
	if(task_mgr->pCurTcb != NULL)
	{
		task_mgr->pCurTcb->pTask(task_mgr->pCurTcb->pUser);
		__NOS_lockTaskMgr();
		__nos_pushTaskBackToArray();
		__NOS_unlockTaskMgr();
		return task_mgr->pCurTcb->nPrio;
	}
	
	return -1;
}

/*
*********************************************************************************************************
* Description	: This function is a callback function of of system tick IRQ Handler.
*
* Arguments  	: None.
*
* Return		: None.
*
* Note(s)   	: (1) Job1: count the tick of OS.
*					  Job2: count the tick of current running task (will be clear to 0 when pends up).
*					  Job3: decrease the tick of delay wait if NOS_delayTick() is called.
*					  Job4: decrease the tick wait value of each task, and wakeup the one whose tick wait
*							value reach 0.
*					  Job5: decrease the tick cnt of event of each task, and wakeup the one whose tick wait
*							value reach 0.
*
*				  (2) This function should be called in system tick IRQ Handler.
*
*********************************************************************************************************/
void NOS_onSysTick(void) 
{
	NOS_TASKNUM m;
	
	__NOS_enterInt();
	__NOS_lockTaskMgr();
	
	(task_mgr->nTickCnt) ++;	// tick for the whole OS.
	if(task_mgr->pCurTcb != NULL) // tick for each task.
	{
		(task_mgr->pCurTcb->nTickCnt) ++;	
	}
	if(task_mgr->bPending == 1) // tick for NOS_delayTick().
	{
		task_mgr->nDelayTickCnt = (task_mgr->nDelayTickCnt > 0)? task_mgr->nDelayTickCnt - 1: 0;
	}

	for(m=task_mgr->nTaskRdy; m<task_mgr->nTaskAll; m++)
	{
		if(task_mgr->arrTaskTcb[m]->nTickToWait > 0) // 1.tick wait.
		{
			(task_mgr->arrTaskTcb[m]->nTickToWait) --;
			if(task_mgr->arrTaskTcb[m]->nTickToWait == 0)
			{
				nos_runWakeupTask(m);
			}
		}
		else // 2.evt wait.
		{
			struct NOS_Evt_t *wait_evt = task_mgr->arrTaskTcb[m]->pEvtWait;
			if(wait_evt != NULL)
			{
				struct NOS_Evt_Timeout_t *timeout = wait_evt->pTimeout->p1stElement;
				while(timeout != NULL)
				{
					if(timeout->nTaskPrio == task_mgr->arrTaskTcb[m]->nPrio)
					{
						break;
					}
					timeout = timeout->pNext;
				}
				if((timeout != NULL) && (timeout->nIsTimeout == 0))
				{
					if(timeout->nTick > 0)
					{
						(timeout->nTick) --;
						if(timeout->nTick == 0)
						{
							timeout->nIsTimeout = 1;
							nos_runWakeupTask(m);
						}
					}
				}
			}
		}
	}
	__NOS_unlockTaskMgr();
	__NOS_exitInt();
}

/*
*********************************************************************************************************
* Description	: This function will run when no task is running.
*
* Arguments  	: func						Function provided by user.
*
* Return		: None.
*
* Note(s)   	: (1) This function should be used together with NOS_runReadyTask(), that is 
*					  if NOS_runReadyTask() return (-1) then you should run NOS_onIdle().
*
*				  (2) You can do such jobs in this function:
*					(a) Enter low power.
*					(b) Calculate the cpu usage of each task.
*					(c)	Feed the watch dog.
*
*********************************************************************************************************/
void NOS_onIdle(NOS_Func func)
{
	struct NOS_InnerMgr_t *task_mgr = NOS_getInnerMgr();
	nos_calTaskCpuUsageRatio();
	
	if(func != NULL)
	{
		func();
	}		
}
