#include "CBOS_functions.h"
#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

//Initializing the thread status structure
CBOS_status_t CBOS_threadStatus = {
	0,
	NULL,
	0,
	{NULL}
};

void CBOS_context_switch(void)
{
	//Trigger the PendSV interrupt 
	volatile uint32_t *icsr = (void*)0xE000ED04; //Interrupt Control/Status Vector
	*icsr = 0x1<<28; //tell the chip to do the pendsv by writing 1 to the PDNSVSET bit

	//flush things
		__ASM("isb"); //This just prevents other things in the pipeline from running before we return
	printf("\nswitching da thready\n");
	return;
}

void CBOS_add_priority_queue(CBOS_threadInfo_t * thread)
{
	if (CBOS_threadStatus.priorityArray[thread->priority] == NULL)
		CBOS_threadStatus.priorityArray[thread->priority] = thread;
	else
	{
		CBOS_threadInfo_t * temp = CBOS_threadStatus.priorityArray[thread->priority];
		while (!(temp->next == NULL))
			temp = temp->next;
		temp->next = thread;
	}
}

void CBOS_create_thread(void (*funct_ptr)(), uint8_t priority)
{
	
	//increment thread count
	CBOS_threadStatus.thread_count++;
	
	//update structure with function pointer and priority
	CBOS_threadInfo_t * new_thread = (CBOS_threadInfo_t*)malloc(sizeof(CBOS_threadInfo_t));
	new_thread->funct_ptr = funct_ptr;
	new_thread->priority = priority;	
	
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
	
	//update priority queue with thread
	CBOS_add_priority_queue(new_thread);
}

void idle_thread()
{
	while(1)
	{
		printf("Idle Thread\n");
	}
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
	
	//Create the idle thread and set it to the lowest priority
	CBOS_create_thread(idle_thread, 9);
}

void CBOS_kernel_start(void){
	
	/*Setting PSP to first thread address + 14 * 4 (this offset changed to 12*4 for
		when we tailchained trigger_pendsv() with the systick_handler().
		When the thread was created, it was initially ready to be run, but
		now, by setting its PSP then forcing a trigger_pendsv() we are pushing
		instead of popping, so we are setting an offset to ensure we are back 
		where we should be
	*/
	CBOS_scheduler();
	CBOS_threadStatus.current_thread = CBOS_threadStatus.next_thread;
	CBOS_set_next_thread();
	CBOS_scheduler();
	__set_CONTROL(1<<1);
	__set_PSP(CBOS_threadStatus.current_thread->stackPtr_address + 12*4); //cannot guarantee there is a thread here... how else do we decide?
	
	//setup systick timer
	SysTick_Config(SystemCoreClock/200);
	
	//Call the scheduler and start the first switch
	CBOS_context_switch();
}

void set_PSP_new_stackPtr(){
	//printf("Switching Context!\n");
	/*-Saving the current (about to switch out of) thread PSP
		-we do not have to save an offset bec by saving it here
			it has the offset included
	*/
	CBOS_threadStatus.current_thread->stackPtr_address =__get_PSP(); 
	
	//call next_thread() function to find next thread based on priority and round-robin scheduling
	CBOS_set_next_thread();
	
	//Finally, set the PSP for the new thread
	__set_PSP((uint32_t)CBOS_threadStatus.current_thread->stackPtr_address);
}

void CBOS_scheduler(void)
{
	uint8_t index = 0;
	CBOS_threadInfo_t * temp = CBOS_threadStatus.priorityArray[index];
	
	//find the next highest occupied priority array index
	while(temp == NULL)
	{
		index++;
		temp = CBOS_threadStatus.priorityArray[index];
	}
	
	//MAKE SURE YOU DEFINE AN IDLE THREAD SO WE ALWAYS REACH A THREAD
	
	//set the next thread to be the "longest waiting" highest priority ready thread
	CBOS_threadStatus.next_thread= temp;
}

void CBOS_set_next_thread(void)
{	
	//pop the selected thread from the ready array (queue)
	CBOS_threadInfo_t * temp = CBOS_threadStatus.next_thread;
	CBOS_threadStatus.priorityArray[temp->priority] = temp->next;
	
	//make sure it connects to nothing
	temp->next = NULL;
}

void CBOS_delay(uint32_t ms)
{
	//the delay will be in terms of ms bec our systick handler will fire every 1ms
	
	CBOS_threadInfo_t * temp = CBOS_threadStatus.sleepingHead;
	if (temp == NULL)
		temp = CBOS_threadStatus.current_thread;
	else 
		while (temp->next != NULL)
			temp = temp->next;
		temp = CBOS_threadStatus.current_thread;
	temp->delay = ms + 1;
	CBOS_scheduler();
	CBOS_context_switch();
}



void SysTick_Handler(void)
{
	//CBOS_threadStatus.sysCount = (CBOS_threadStatus.sysCount + 1)%TICKDELAY;
	CBOS_update_sleeping_queue();
	
	//run the scheduler
	CBOS_scheduler();
	//check if suggested next thread has higher priority than current thread.. 
	//if not, let thread yield know it should not perform a context switch by setting next_thread to NULL
	if (CBOS_threadStatus.next_thread->priority >= CBOS_threadStatus.current_thread->priority)
	{
		CBOS_add_priority_queue(CBOS_threadStatus.current_thread);
		CBOS_context_switch();
	}
	/*
	if (CBOS_threadStatus.sysCount == TICKDELAY-1)
	{
		//update delay queue function
		CBOS_update_delay_count();
		CBOS_kernel_thread();
	}
	*/
}


void CBOS_update_sleeping_queue(void)
{
	CBOS_threadInfo_t * prev = NULL;
	CBOS_threadInfo_t * current = CBOS_threadStatus.sleepingHead;
	CBOS_threadInfo_t * delete_node = NULL;
	
	while (current != NULL)
	{
		current->delay -= TIME_BETWEEN_SYSTICK_MS;
		
		if(current->delay <=0)
		{
			if(current == CBOS_threadStatus.sleepingHead)
			{
				CBOS_threadStatus.sleepingHead = current->next;
			}
			
			delete_node = current;
			current = current->next;
			
			delete_node->next = NULL;
			CBOS_add_priority_queue(delete_node);
			if (prev != NULL)
				prev->next = current;
		}
		
		else
		{
			prev = current;
			current = current->next;
		}
	}
}

void CBOS_thread_yield(void)
{
	CBOS_scheduler();
	CBOS_add_priority_queue(CBOS_threadStatus.current_thread);
	CBOS_context_switch();
}

