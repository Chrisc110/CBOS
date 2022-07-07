#include "CBOS_functions.h"
#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

//Initializing the thread status structure
CBOS_status_t CBOS_threadStatus = {
	0,
	0,
	{0}
};

void trigger_pendsv(void)
{
	//Determine whether a switch is even necessary – if there is only one task running, why bother?
	if (CBOS_threadStatus.thread_count==1)
		return;

	//Trigger the PendSV interrupt 
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
	
	//Ensure the 24th bit is set
	*pspCopy = 1 << 24;
	pspCopy--;
	
	//store the function pointer address
	*pspCopy = (uint32_t)(CBOS_threadStatus.threadInfo[CBOS_threadStatus.thread_count - 1].funct_ptr);
	pspCopy--;
	
	//Setting LR, R12, R3, R2, R1, R0 to something obvious
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
	
	//Setting the thread's stack pointer to the copy that was decremented
	CBOS_threadStatus.threadInfo[CBOS_threadStatus.thread_count - 1].stackPtr_address = (uint32_t)pspCopy;
}

void CBOS_kernel_start(void){
	__set_CONTROL(1<<1);
	
	/*Setting PSP to first thread address + 14 * 4 becuase 
		when the thread was created, it was initially ready to be run, but
		now, by setting its PSP then forcing a trigger_pendsv() we pushing
		instead of popping, so we are setting an offset to ensure we are back 
		where we should be
	*/
	__set_PSP(CBOS_threadStatus.threadInfo[0].stackPtr_address + 14 *4);
	
	//Start the first switch
	trigger_pendsv();
}


void set_PSP_new_stackPtr(){
	printf("Switching Context!\n");
	/*-Saving the current (about to switch out of) thread PSP
		-we do not have to save an offset bec by saving it here
			it has the offset included
	*/
	CBOS_threadStatus.threadInfo[CBOS_threadStatus.current_thread].stackPtr_address =__get_PSP(); 
	
	//Incrementing the current thread index by 1 to ready the next thread
	//We are currently doing a pure round robin scheduler
	CBOS_threadStatus.current_thread = (CBOS_threadStatus.current_thread+1)%CBOS_threadStatus.thread_count;
	
	//Finally, set the PSP for the new thread
	__set_PSP((uint32_t)CBOS_threadStatus.threadInfo[CBOS_threadStatus.current_thread].stackPtr_address);
}


