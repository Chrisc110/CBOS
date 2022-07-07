#include "CBOS_functions.h"
#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>


CBOS_status_t CBOS_threadStatus = {
	0,
	0,
	{0}
};

void trigger_pendsv(void)
{
	//1. Determine whether a switch is even necessary – if there is only one task running, why bother?
	if (CBOS_threadStatus.thread_count==1)
		return;

	//3. Save a useful offset of the current thread’s stack pointer somewhere that it can access it again once it is scheduled to run
	CBOS_threadStatus.threadInfo[CBOS_threadStatus.current_thread].stackPtr_address = __get_PSP();//CBOS_threadStatus.threadInfo[CBOS_threadStatus.current_thread].stackPtr_address; //I REMOVED -17*4 DECREMENTATION
	printf("%X\n", __get_PSP()); 
	//2. If it is necessary, determine which thread to switch to
	
	CBOS_threadStatus.current_thread = (CBOS_threadStatus.current_thread+1)%CBOS_threadStatus.thread_count;
	
	//4. Trigger the PendSV interrupt 
	volatile uint32_t *icsr = (void*)0xE000ED04; //Interrupt Control/Status Vector
	*icsr = 0x1<<28; //tell the chip to do the pendsv by writing 1 to the PDNSVSET bit



	//flush things
		__ASM("isb"); //This just prevents other things in the pipeline from running before we return
	
	return;
}

void CBOS_create_thread(void (*funct_ptr)())
{
	
	//increment thread count
	CBOS_threadStatus.thread_count++;
	
	//update structure with function pointer 
	CBOS_threadStatus.threadInfo[CBOS_threadStatus.thread_count - 1].funct_ptr = funct_ptr;
	
	//update structure with stack pointer
	CBOS_threadStatus.threadInfo[CBOS_threadStatus.thread_count - 1].stackPtr_address = CBOS_threadStatus.initial_MSP_addr - THREAD_STACK_SIZE * CBOS_threadStatus.thread_count;
	
	//make a copy of stack pointer for the thread we just made
	uint32_t * pspCopy = (uint32_t*)CBOS_threadStatus.threadInfo[CBOS_threadStatus.thread_count - 1].stackPtr_address;
	
	*pspCopy = 1 << 24;
	pspCopy--; //this is "1" but we may have to do 4
	
	*pspCopy = (uint32_t)(CBOS_threadStatus.threadInfo[CBOS_threadStatus.thread_count - 1].funct_ptr);
	pspCopy--;

	*pspCopy = 0xE;
	pspCopy--;
	
	*pspCopy = 0xD;
	pspCopy--;
	
	*pspCopy = 0xC;
	pspCopy--;
	
	*pspCopy = 0xB;
	pspCopy--;
	
	*pspCopy = 0xA;
	pspCopy--;
	
	*pspCopy = 0x9;
	pspCopy--;
	
	//set R11
	*pspCopy = 0xB;
	pspCopy--;
	
	//set R10
	*pspCopy = 0xA;
	pspCopy--;
	
	//set R9
	*pspCopy = 0x9;
	pspCopy--;
	
	//Set R8
	*pspCopy = 0x8;
	pspCopy--;
	
	 //Set R7
	*pspCopy = 0x7;
	pspCopy--;
	
	//Set R6
	*pspCopy = 0x6;
	pspCopy--;
	
	//Set R5
	*pspCopy = 0x5;
	pspCopy--;
	
	//Set R4
	*pspCopy = 0x4;
}

void idle_thread()
{
	while(1)
	{
		printf("bitchin");
		trigger_pendsv();
	}
}

void CBOS_kernel_initialize()
{
		//CBOS_create_thread(idle_thread);
}

void CBOS_kernel_start(void){
	__set_CONTROL(1<<1);
	
	__set_PSP(CBOS_threadStatus.threadInfo[0].stackPtr_address);
	trigger_pendsv();
}


void set_PSP_new_stackPtr(){
	printf("interrupting!\n");
	
	__set_PSP((uint32_t)CBOS_threadStatus.threadInfo[CBOS_threadStatus.current_thread].stackPtr_address);

}


