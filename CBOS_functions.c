#include "CBOS_functions.h"
#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

//Initializing the thread status structure
CBOS_status_t CBOS_threadStatus = {
	0,
	0,
	0,
	{NULL}
};

void trigger_pendsv(void)
{
	//Determine whether a switch is even necessary � if there is only one task running of that priority, why bother?
	//if there are no other threads in the same priority queue as the current thread, no context switch is needed 
	//in the case that a higher priority thread has been unblocked, this will be taken care of when the systick handler is called
	if (CBOS_threadStatus.priorityArray[CBOS_threadStatus.current_thread->priority] == NULL)//->thread info?
		return;

	//Trigger the PendSV interrupt 
	volatile uint32_t *icsr = (void*)0xE000ED04; //Interrupt Control/Status Vector
	*icsr = 0x1<<28; //tell the chip to do the pendsv by writing 1 to the PDNSVSET bit

	//flush things
		__ASM("isb"); //This just prevents other things in the pipeline from running before we return
	
	return;
}

void CBOS_add_priority_queue(CBOS_threadInfo_t * thread)
{
	if (CBOS_threadStatus.priorityArray[thread->priority] == NULL)
		CBOS_threadStatus.priorityArray[thread->priority] = thread;
	else
	{
		CBOS_threadInfo_t * queue_current = CBOS_threadStatus.priorityArray[thread->priority];
		while (!(queue_current->next == NULL))
			queue_current = queue_current->next;
		queue_current->next = thread;
	}
}

void CBOS_create_thread(void (*funct_ptr)(), uint8_t priority)
{
	
	//increment thread count
	CBOS_threadStatus.thread_count++;
	
	//update structure with function pointer and priority
	CBOS_threadInfo_t * new_thread = (CBOS_threadInfo_t*)malloc(sizeof(CBOS_threadInfo_t));;
	new_thread->funct_ptr = funct_ptr;
	new_thread->priority = priority;

	//update priority queue with thread
	CBOS_add_priority_queue(new_thread);	
	
	//update structure with stack pointer
	new_thread->stackPtr_address = CBOS_threadStatus.initial_MSP_addr - THREAD_STACK_SIZE * CBOS_threadStatus.thread_count;
	
	//make a copy of stack pointer for the thread we just made
	uint32_t * pspCopy = (uint32_t *)new_thread->stackPtr_address;
	
	//Ensure the 24th bit is set
	*pspCopy = 1 << 24;
	pspCopy--;
	
	//store the function pointer address
	*pspCopy = (uint32_t)(new_thread->funct_ptr);
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
	new_thread->stackPtr_address = (uint32_t)pspCopy;
}

void CBOS_kernel_initialize(void)
{
	/* set the PendSV interrupt priority to the lowest level 0xFF,
		 which makes it very negative. In the ARM Cortex, negative priorities are 
		 high. It's frustrating. Think of them as "strong" priorities or something
	
		 The constant 0xE000ED20 is the location of the priority table for PendSV
	*/
	*(uint32_t volatile *)0xE000ED20 |= (0xFFU << 16);
		
	//find and store the MSP address
	uint32_t* MSR_Original = (uint32_t*)0;
	uint32_t msrAddr = *MSR_Original;
	CBOS_threadStatus.initial_MSP_addr = msrAddr;
}

void CBOS_kernel_start(void){
	__set_CONTROL(1<<1);
	
	/*Setting PSP to first thread address + 14 * 4 (this offset changed to 12*4 for
		when we tailchained trigger_pendsv() with the systick_handler().
		When the thread was created, it was initially ready to be run, but
		now, by setting its PSP then forcing a trigger_pendsv() we are pushing
		instead of popping, so we are setting an offset to ensure we are back 
		where we should be
	*/
	// find highest priorty thread and set as current thread. highest priority ptr point to current_thread-> next. current thread -> next = null.
	//CBOS_threadStatus.current_thread = CBOS_threadStatus.priorityArray[1];
	//CBOS_threadStatus.priorityArray[1] = CBOS_threadStatus.priorityArray[1]->next;
	__set_PSP(CBOS_threadStatus.current_thread->stackPtr_address + 12*4); //cannot guarantee there is a thread here... how else do we decide?
	
	//setup systick timer
	SysTick_Config(SystemCoreClock/200);
	
	//Start the first switch
	trigger_pendsv();
}

void set_PSP_new_stackPtr(){
	printf("Switching Context!\n");
	/*-Saving the current (about to switch out of) thread PSP
		-we do not have to save an offset bec by saving it here
			it has the offset included
	*/
	CBOS_threadStatus.current_thread->stackPtr_address =__get_PSP(); 
	
	//call next_thread() function to find next thread based on priority and round-robin scheduling
	CBOS_next_thread();
	
	//Finally, set the PSP for the new thread
	__set_PSP((uint32_t)CBOS_threadStatus.current_thread->stackPtr_address);
}

void CBOS_next_thread(void)
{
	//push current thread onto the appropriate priority stack
	CBOS_add_priority_queue(CBOS_threadStatus.current_thread);
	//decide which thread to run next 
	//find highest priorty thread and set as current thread. highest priority ptr point to current_thread-> next. current thread -> next = null.
	//CBOS_threadInfo_t * next_thread = CBOS_threadStatus.priorityArray[1];
	//CBOS_threadStatus.priorityArray[1] = CBOS_threadStatus.current_thread;
	//CBOS_threadStatus.current_thread = next_thread;
	//CBOS_threadStatus.current_thread->next = NULL;
}
void SysTick_Handler(void){
	trigger_pendsv();
}
