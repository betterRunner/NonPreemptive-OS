### NonPreemptive-OS
One easy OS based on roll poling instead of preemptive cpu (mostly be used in MCU).
The code was tested in stm32f103.

# structure
nonOS.c								--			source code of OS.
nonOS.h								--			h file of OS, include it in your code.
nonOS_common.h						--			lists basic type of OS.
smart_memory.c/smart_meory.h		--			smart memory using memory pool.

# how to use
```cpp
#include "nonOS.h"

main()
{
	/// 1. Init the memory pool adress and size.
	uint32_t nos_memory_size;
	uint32_t nos_addr = (NOS_MEMORY_ADDR)(your_free_ram_address);
	nos_memory_size = 0x20000000 + 0x5000 - nos_addr;
	Mem_init(nos_addr, (NOS_MEMORY_SIZE)nos_memory_size, 8);
	
	/// 2. Create some sem or messagebox.
	NOS_createEvt(NOS_EVT_Sem, &Sem_System_test, (void *)sem_num);
	NOS_createEvt(NOS_EVT_MsgBox, &Msg_System_ErrorCode, NULL);
	
	/// 3. Create some tasks.
	NOS_createTask(func1, NULL, 0);
	NOS_createTask(func2, NULL, 1);
	
	/// 4. Call NOS_onSysTick() in your system tick handler.
	
	/// 5. Manage the tasks in a loop.
	while(1)
	{
		struct NOS_InnerMgr_t * mgr = NOS_getInnerMgr();
		while(mgr->nTaskAll > 0)
		{
			if(NOS_runReadyTask() == (-1))
			{
				NOS_onIdle((NOS_Func)MOS_OnIdle);
			}
		}
	}
}

/// Task 1
__NOS_startTask(func1)
{

}
__NOS_endTask

/// Task 2
__NOS_startTask(func2)
{

}
__NOS_endTask

```

