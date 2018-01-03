#ifndef _OS_CPU_H_
#define _OS_CPU_H_

#ifdef __cplusplus
 extern "C" {
#endif /* __cplusplus */
	 #define local_irq_save OS_CPU_SR_Save
	 #define local_irq_restore OS_CPU_SR_Restore
	typedef unsigned int   OS_CPU_SR;
	OS_CPU_SR  OS_CPU_SR_Save(void);
	void       OS_CPU_SR_Restore(OS_CPU_SR cpu_sr);

#ifdef __cplusplus
 };
#endif /* __cplusplus */
	 
#endif
