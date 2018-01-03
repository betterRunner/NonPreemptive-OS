#ifndef _NONOS_H_
#define	_NONOS_H_

#include "nonOS_common.h"
#include "os_cpu.h"

enum NOS_Error_e
{
  NOS_ERROR_None = 0,								// No error.
  NOS_ERROR_NullTcb,								// Tcb of task is null.
  NOS_ERROR_NullPointer,							// Handling null pointer.
  NOS_ERROR_NullMemory,								// Malloc memory is not enough.
  NOS_ERROR_NullStack,								// Handling null stack. 
  NOS_ERROR_NullTaskFunc,							// Handling null task func.
  NOS_ERROR_NullEvt,								// Handling null event.
  NOS_ERROR_WrongPrio,								// Prio of task is wrong.
  NOS_ERROR_WrongParm,								// Parmeter is wrong.
  NOS_ERROR_FullTaskList,							// Task list is full.
  NOS_ERROR_NotInList,								// List does not contain the element.
  NOS_ERROR_Pended,									// The task is pended.
  NOS_ERROR_InvalidOper,							// Invalid operation.
};

enum NOS_EvtType_e
{
  NOS_EVT_None = 0,
  NOS_EVT_Sem,
  NOS_EVT_MsgBox,

  NOS_EVT_NUM,
};

enum NOS_Msg_e
{
	NOS_MSG_NoFree = 0, // msg's memory has no need to be freed.
	NOS_MSG_SendFree, // msg's memory has to be freed by sender.
	NOS_MSG_RecvFree, // msg's memory has to be freed by receiver.
	NOS_MSG_NUM,
};

typedef int (*NOS_Task)(void * pUser);
typedef int (*NOS_Func)(void);

struct NOS_InnerMgr_t
{
#define NOS_MAX_TASKNUM           10												// Max number of OS's task, according to your mcu.
	uint16_t bInited:			1;													// Is structure inited.
  uint16_t bRunning:			1;													// Is OS running.
  uint16_t bCalling:			1;													// Is one task creating another task.
	uint16_t bPending:			1;													// Is OS pended up by delay.
  uint16_t bitsReserved:		12;

  uint8_t						nIntNested;											// Number of running ISR.
  NOS_TASKNUM                   nTaskAll;											// Number of all registered task.
  NOS_TASKNUM                   nTaskRdy;											// Number of ready task.
  NOS_TICK						nTickCnt;											// Tick count of OS.
  NOS_TICK						nDelayTickCnt;										// Tick count of delay (used for delay).
  struct NOS_Tcb_t*             pCurTcb;											// Pointer of current task's Tcb.
  struct NOS_Tcb_t*             arrTaskTcb[NOS_MAX_TASKNUM];						// Array of pointer of all tasks' Tcb.
  struct NOS_TaskInxList_t*	  	pWakeupTaskInxList;									// List of task ready to wakeup (used for delay).
};

struct NOS_Tcb_t
{
  uint8_t bitsReserved:         8;

  uint8_t						nCpuUsageRatio;										// Percentage of CPU usage of task.
  NOS_TASKNUM                   nPrio;												// Priority of task.
  NOS_TICK                      nTickCnt;											// Tick count of task.
  NOS_TICK                      nTickToWait;										// Tick count to wake up.
  int			                nCodeLine;											// Code Line where task pends up,
																					// which is also where will run when resumes.
  NOS_Task                      pTask;												// Pointer of Function of task.
  void*                         pUser;												// Pointer of User msg.
  struct NOS_Evt_t*				pEvtWait;											// Pointer of event that task waitting.
  struct NOS_Stack_t*           pStack;												// Pointer of Stack of task, which will be stored
																					// when pends up, restored when resumes.
  struct NOS_Tcb_t*             pNext;												// Pointer of Next Task's Tcb.
};

struct NOS_Tcb_t;
struct NOS_Evt_t;
struct NOS_InnerMgr_t *NOS_getInnerMgr(void);

/*
*********************************************************************************************************
* Description	: this function push task back to task array.
*
* Arguments  	: None.
*
* Return		: None.
*
* Note(s)   	: (1) OS will call it and you should not call it.
*
*********************************************************************************************************/
#define __nos_pushTaskBackToArray() \
  if(task_mgr->pCurTcb != NULL){ \
    task_mgr->arrTaskTcb[task_mgr->nTaskAll] = task_mgr->pCurTcb; \
    task_mgr->nTaskAll ++; \
    task_mgr->pCurTcb = NULL; \
  }

/*
*********************************************************************************************************
* Description	: this function wait for object (including Sem, MsgBox and so on).
*
* Arguments  	: pObj						the object to wait.
*
*				  nTimeout					wait timeout, (-1) means wait forever, 0 means not wait.
*
*				  pMsgAddr					msg addr if needed, (such as MsgBox).
*
* Return		: None.
*
* Note(s)   	: (1) __NOS_waitTick(), __NOS_waitSem(), __NOS_waitMsgBox() will call it.
*
*				  (2) explaination about some point: 
*
*					(a) case __LINE__: ... is the start point when a task resumes.
*
*					(b) 'bNotJump == 0' is to differ two suituation: 1) code run normally, 2) code jump
*						from case __LINE__ when task resumes.
*						'pObj != NULL' is to differ two type of wait: 1) wait tick, 2) wait object.
*
*					(2)	OS wil call it and you should not call it.
*
*********************************************************************************************************/
#define __nos_waitObj(pObj, nTimeout, pMsgAddr) \
	do{ \
		if(task_mgr->nIntNested == 0){ \
			 __NOS_lockTaskMgr(); \
			if((pObj == NULL) && (nTimeout != 0)){ \
				tcb_cur->pEvtWait = NULL; \
				tcb_cur->nTickToWait = nTimeout; \
				__nos_pushTaskBackToArray(); \
			} \
			else if(pObj != NULL){ \
				nos_waitEvt(pObj, nTimeout, pMsgAddr); \
			} \
			__nos_storeTaskInfo(); \
			__NOS_unlockTaskMgr(); \
			if(task_mgr->pCurTcb != tcb_cur){ \
				task_mgr->bRunning = 0; \
				return NOS_ERROR_Pended; \
			} \
			bNotJump = 1; \
			case __LINE__: nos_restoreStackValue(tcb_cur, &tcb_cur); \
			if((bNotJump == 0) && (pObj != NULL)){ \
				__NOS_lockTaskMgr(); \
				nos_waitEvt(pObj, nTimeout, pMsgAddr); \
				__nos_storeTaskInfo(); \
				__NOS_unlockTaskMgr(); \
				if(task_mgr->pCurTcb != tcb_cur){ \
					task_mgr->bRunning = 0; \
					return NOS_ERROR_Pended; \
				} \
			} \
			else{ \
				bNotJump = 0; \
			} \
		} \
	} while(0)	

/*
*********************************************************************************************************
* Description	: this function select state of state machine according to NVIC signal or Key Operation.
*
* Arguments  	: None.
*
* Return		: None.
*
* Note(s)   	: Because stack grows downword in ARM, so use the address of the first parm (pCurTcb) to
*				  sub the last parm (m) equals size of stack.
*
*********************************************************************************************************/
#define __nos_storeTaskInfo() \
	if(task_mgr->pCurTcb != tcb_cur){ \
		int m; \
		m = (int)&tcb_cur - sizeof(m) - (int)&m; \
		nos_storeStackValue(tcb_cur, (uint8_t *)&m + sizeof(m), m); \
		tcb_cur->nCodeLine = __LINE__; \
	}
	
/*
*********************************************************************************************************
* Description	: this function together with __NOS_endTask() to combine a complete task structure.
*
* Arguments  	: task_name					the name of task designed by user.
*
* Return		: NOS_ERROR_None			no error.
*				  NOS_ERROR_InvalidOper		Should not start task when in ISR or OS is not running.
*
* Note(s)   	: (1) how to use: 
*					__NOS_startTask(task1)
*					{
*						user jobs.
*					}
*					__NOS_endTask()
*													
*				  (2) details of the structure:
*					(a) if you pend up the task in 'user jobs' such as waitting a sem, we will store the 
*						code line where pended, so every time we enter __NOS_startTask() if the stored 
*						code line is not -1 we will jump to it, that makes task schedule possiable. 
*
*					(b) not letting compiler to otimize, the variables you declare in 'user jobs' should
*												be in form of 'volatile'.
*
*                 (3) tcb_cur should be the last parameter in __NOS_startTask() because its address is start
*                     address to store and resotre the value in stack.
*
*********************************************************************************************************/
#define __NOS_startTask(task_name) \
	int (task_name)(void *pUser) \
	{ \
		uint8_t bNotJump = 0; \
		struct NOS_InnerMgr_t *task_mgr = NOS_getInnerMgr(); \
		struct NOS_Tcb_t *tcb_cur = task_mgr->pCurTcb; \
		if((task_mgr->bRunning) || (task_mgr->nIntNested > 0)) \
		{ \
			return NOS_ERROR_InvalidOper; \
		} \
		task_mgr->bRunning = 1; \
		switch(tcb_cur->nCodeLine) \
		{ \
			case -1: \
			
#define __NOS_endTask \
			default: \
				break; \
		} \
		task_mgr->bRunning = 0; \
		return NOS_ERROR_None; \
	}

/*
*********************************************************************************************************
* Description	: this function lock and unlock the irq of ARM to insure the jobs will not be interrupted.
*
* Arguments  	: None.
*
* Return		: None.
*
* Note(s)   	: (1) how to use: 
*					__NOS_lockTaskMgr();
*					user jobs
*					__NOS_unlockTaskMgr();
*
*********************************************************************************************************/
#define __NOS_lockTaskMgr()             						{unsigned int  cpu_sr = local_irq_save();
#define __NOS_unlockTaskMgr()       		    				local_irq_restore(cpu_sr);}

/*
*********************************************************************************************************
* Description	: this function record if OS is in IRQ.
*
* Arguments  	: None.
*
* Return		: None.
*
* Note(s)   	: (1) how to use:
*					IRQ Handler()
*					{
*						__NOS_enterInt()
*						user jobs
*						__NOS_exitInt()
*					}
*
*				  (2) this function should be called in the IRQ which will change the variables of OS.
*					  If in IRQ such as RTC IRQ that do not change the variables of OS, you may not use.
*
*********************************************************************************************************/
#define __NOS_enterInt()														{struct NOS_InnerMgr_t * task_mgr = NOS_getInnerMgr(); (task_mgr->nIntNested) ++;
#define __NOS_exitInt()															(task_mgr->nIntNested) --;}

/*
*********************************************************************************************************
* Description	: this function wait or send event.
*
* Arguments  	: None.
*
* Return		: None.
*
* Note(s)   	: (1) see __nos_waitObj() and nos_sendEvt() for details.
*
*********************************************************************************************************/
#define __NOS_waitTick(nTimeout) 								__nos_waitObj(NULL, nTimeout, NULL)
#define __NOS_waitSem(pEvt, nTimeout) 							__nos_waitObj(pEvt, nTimeout, NULL)
#define __NOS_waitMsgBox(pEvt, nTimeout, pMsgAddr) 				__nos_waitObj(pEvt, nTimeout, pMsgAddr)
#define __NOS_sendSem(pEvt) 									nos_sendEvt(pEvt, NOS_MSG_NoFree, NULL)
#define __NOS_sendMsgBox(pEvt, type, msg) 						nos_sendEvt(pEvt, type, msg)


int 	nos_sendEvt(struct NOS_Evt_t *pEvt, enum NOS_Msg_e eMsgType, void *pMsg);
int		nos_waitEvt(struct NOS_Evt_t *pEvt, NOS_TICK nTimeout, void ** pMsgAddr);
int 	nos_storeStackValue(struct NOS_Tcb_t *pCurTcb, const void* pVars, int nCountOfBytes);
int 	nos_restoreStackValue(struct NOS_Tcb_t *pCurTcb, void* pVarsEnd);

int 	NOS_createTask(NOS_Task pTask, void* pUser, NOS_TASKNUM nPrio);
int 	NOS_deleteTask(NOS_TASKNUM nPrio);
int 	NOS_createEvt(enum NOS_EvtType_e eType, struct NOS_Evt_t **pEvtAddr, void* pOthers);
int 	NOS_deleteEvt(struct NOS_Evt_t **pEvtAddr);
int 	NOS_delayTick(NOS_TICK nTick, NOS_Func func);
int 	NOS_runReadyTask(void);
void 	NOS_onSysTick(void) ;
void 	NOS_onIdle(NOS_Func func);

#endif
