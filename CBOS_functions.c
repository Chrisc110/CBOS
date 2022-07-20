#include "CBOS_functions.h"
#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

//Initializing the thread status structure
CBOS_status_t CBOS_threadStatus = {
	0,
	0,
	NULL,
	NULL,
	0,
	{NULL},
	NULL
};

void context_switch(void)
{
	//Determine whether a switch is even necessary if there is only one task running of that priority, why bother?
	//if there are no other threads in the same priority queue as the current thread, no context switch is needed 
	//in the case that a higher priority thread has been unblocked, this will be taken care of when the systick handler is called
	
	__enable_irq();
	//Trigger the PendSV interrupt 
	volatile uint32_t *icsr = (void*)0xE000ED04; //Interrupt Control/Status Vector
	*icsr = 0x1<<28; //tell the chip to do the pendsv by writing 1 to the PDNSVSET bit

	//flush things
		__ASM("isb"); //This just prevents other things in the pipeline from running before we return
	
	return;
}

void CBOS_add_priority_queue(CBOS_threadInfo_t * thread)
{
	__disable_irq(); //disable interrupts to ensure we are not interrupted in this important process

	//first check if the element of the array is empty
	if (CBOS_threadStatus.priorityArray[thread->priority] == NULL)
		CBOS_threadStatus.priorityArray[thread->priority] = thread;
	
	else
	{	//find the end of the linked list and populate it
		CBOS_threadInfo_t * temp = CBOS_threadStatus.priorityArray[thread->priority];
		while (!(temp->next == NULL))
			temp = temp->next;
		temp->next = thread;
	}
	__enable_irq();
}

void CBOS_create_thread(void (*funct_ptr)(), uint8_t priority)
{
	//increment thread count
	CBOS_threadStatus.thread_count++;
	
	//update structure with function pointer and priority
	CBOS_threadInfo_t * new_thread = (CBOS_threadInfo_t*)malloc(sizeof(CBOS_threadInfo_t));
	new_thread->funct_ptr = funct_ptr;
	new_thread->priority = priority;
	new_thread->next = NULL;
	
	
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
		//do nothing and yield to check if any threads have become available before the systick
		CBOS_yield();
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

	CBOS_run_scheduler();

	//Set the current thread to run the thread that the scheduler found
	CBOS_threadStatus.current_thread = CBOS_threadStatus.next_thread;

	__set_CONTROL(1<<1);
	__set_PSP(CBOS_threadStatus.current_thread->stackPtr_address + 12*4); //cannot guarantee there is a thread here... how else do we decide?
	
	//setup systick timer
	SysTick_Config(SystemCoreClock/1000);
	
	//Start the first switch
	context_switch();
}

void CBOS_run_kernel(void)
{
	__disable_irq();

	//first run the scheduler to determine the next thread to run
	CBOS_run_scheduler();

	//only context switch if the thread the scheduler choose is different than the current thread running
	if (CBOS_threadStatus.next_thread != CBOS_threadStatus.current_thread)
	{
		context_switch();
	}
	__enable_irq();
}

void CBOS_run_scheduler(void)
{
	uint8_t index = 0;
	CBOS_threadInfo_t * temp = CBOS_threadStatus.priorityArray[index];
	
	//find the next highest occupied priority array index
	while(temp == NULL)
	{
		index++;
		temp = CBOS_threadStatus.priorityArray[index];
	}
	
	//there is an idle thread so if there are no other available threads, it will be chosen
	
	//set the current thread to be the "longest waiting" highest priority ready thread
	CBOS_threadStatus.next_thread = temp;
	
	//pop the selected thread from the ready array (queue)
	CBOS_threadStatus.priorityArray[index] = temp->next;
	
	//make sure it connects to nothing
	temp->next = NULL;
}

void set_PSP_new_stackPtr(){
	//printf("Switching Context!\n");
	/*-Saving the current (about to switch out of) thread PSP
		-we do not have to save an offset bec by saving it here
			it has the offset included
	*/
	CBOS_threadStatus.current_thread->stackPtr_address =__get_PSP(); 
	
	//set the PSP for the new thread
	__set_PSP((uint32_t)CBOS_threadStatus.next_thread->stackPtr_address);

	
	//actually set the current thread to the thread the scheduler chose
	CBOS_threadStatus.current_thread = CBOS_threadStatus.next_thread;
	
	//set the "next_thread" to NULL to signify it has been used
	CBOS_threadStatus.next_thread = NULL;
}

void SysTick_Handler(void){
	//increment the system count which is used to get different timeslices
	CBOS_threadStatus.system_count++;

	//check delayed/sleeping threads and put them back to ready queue if done
	CBOS_update_sleeping_queue();

	//only if the timeslice is up, run the kernel
	if (CBOS_threadStatus.system_count % TIMESLICE == 0)
	{
		//this check is necessary to ensure no other "context switching" event already choose a next thread to run
		if (CBOS_threadStatus.next_thread == NULL)
		{
			//add the current thread to the priority queue
			CBOS_add_priority_queue(CBOS_threadStatus.current_thread);

			//run kernel
			CBOS_run_kernel();
		}
	}

}

void CBOS_update_sleeping_queue(void)
{
	//only if the smallest resolution (or tick) has passed should we update the sleeping queues
	if (CBOS_threadStatus.system_count % SLEEPING_THREAD_MIN_DELAY == 0)
	{
		CBOS_threadInfo_t * prev = NULL;
		CBOS_threadInfo_t * current = CBOS_threadStatus.sleepingHead;
		CBOS_threadInfo_t * delete_node = NULL;
		
		while (current != NULL)
		{
			//checking underflow
			if (current->delay - SLEEPING_THREAD_MIN_DELAY > current->delay)
			{
				current->delay = 0;
			}

			else
				current->delay -= SLEEPING_THREAD_MIN_DELAY;
			
			//if the delay is over, pop the thread from the sleeping queue and add it to the priority queue
			if(current->delay == 0)
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
}

void CBOS_delay(uint32_t ticks)
{	
	CBOS_threadInfo_t * temp = CBOS_threadStatus.sleepingHead;

	//check if there are currently no threads in the sleeping queue
	if (temp == NULL)
	{
		temp = CBOS_threadStatus.current_thread;
		CBOS_threadStatus.sleepingHead = CBOS_threadStatus.current_thread;
	}
	
	else 
	{
		//find the end of the sleeping queue and add the current thread that called delay
		while (temp->next != NULL)
			temp = temp->next;

		temp->next = CBOS_threadStatus.current_thread;
	}
	
	//set the delay value and add 1 because we are in the middle of a timeslice
	temp->delay = ticks + 1;

	CBOS_run_kernel();
}

void CBOS_yield(void)
{
	//first add the thread back into the priority queue then run the kernel
	CBOS_add_priority_queue(CBOS_threadStatus.current_thread);
	CBOS_run_kernel();
}

CBOS_mutex_id_t CBOS_create_mutex(void)
{
	//set the characteristics of the mutex
	CBOS_mutex_t * mutex = (CBOS_mutex_t*)malloc(sizeof(CBOS_mutex_t));
	mutex->count = 1;
	mutex->owner = NULL;
	mutex->next = NULL;
	mutex->blocked_head = NULL;
	
	//add the mutex to the end of the mutex linked list
	CBOS_mutex_t * temp = CBOS_threadStatus.mutex_head;
	
	uint8_t count = 0;
	CBOS_mutex_id_t id;
	if (temp == NULL)
		CBOS_threadStatus.mutex_head = mutex;
	else
	{
		while(temp->next!=NULL)
		{
			count++;
			temp = temp->next;
		}
		temp->next = mutex;
	}
	id.mutexId = count;
	return id;
}

void CBOS_mutex_aquire(CBOS_mutex_id_t calledMutex)
{
	__disable_irq();
	
	//find the muxtex that was called based on its ID
	CBOS_mutex_t * mutex = CBOS_threadStatus.mutex_head;
	for (uint8_t i = 0; i < calledMutex.mutexId; i++)
		mutex = mutex->next;
	
	//check if the count is 0, which means it is locked
	if (mutex->count == 0)
	{
		CBOS_threadInfo_t * temp = mutex->blocked_head;
		if (temp == NULL)
		{
			mutex->blocked_head = CBOS_threadStatus.current_thread;
		}
		
		else 
		{
			while (temp->next != NULL)
				temp = temp->next;

			temp->next = CBOS_threadStatus.current_thread;
		}
				
		//blocked
		CBOS_run_kernel();
	}
	
	//if the mutex is not locked, OR it was just released for this thread, lock it and set the new owners
	__disable_irq();
	mutex->count = 0;
	mutex->owner = CBOS_threadStatus.current_thread;
	__enable_irq();
}

void CBOS_mutex_release(CBOS_mutex_id_t calledMutex)
{
	__disable_irq();
	CBOS_mutex_t * mutex = CBOS_threadStatus.mutex_head;

	//find the mutex based on its ID
	for (uint8_t i = 0; i < calledMutex.mutexId; i++)
		mutex = mutex->next;

	//only release if the thread that called it is the owner
	if (mutex->owner == CBOS_threadStatus.current_thread)
	{
		CBOS_threadInfo_t * temp = mutex->blocked_head;
		if(mutex->blocked_head != NULL)
		{
			mutex->blocked_head = mutex->blocked_head->next;
			temp->next = NULL;
			CBOS_add_priority_queue(temp);
			mutex->owner = temp;
		}
		else
			mutex->count = 1;
			mutex->owner = NULL;
	}
	__enable_irq();
}

CBOS_semaphore_id_t CBOS_create_semaphore(uint8_t max_count, uint8_t starting_count)
{
	__disable_irq();
	CBOS_semaphore_t * semaphore = (CBOS_semaphore_t*)malloc(sizeof(CBOS_semaphore_t));
	semaphore->count = starting_count;
	semaphore->max_count = max_count;
	semaphore->next = NULL;
	semaphore->blocked_head = NULL;
	
	CBOS_semaphore_t * temp = CBOS_threadStatus.semaphore_head;
	
	//this will be used for the ID
	uint8_t count = 0;

	//create a semaphore ID to return
	CBOS_semaphore_id_t id;

	//find the end of the semaphore linked list
	if (temp == NULL)
		CBOS_threadStatus.semaphore_head = semaphore;
	else
	{
		while(temp->next!=NULL)
		{
			count++;
			temp = temp->next;
		}
		count++;
		temp->next = semaphore;
	}
	__enable_irq();
	
	id.semaphoreId = count;

	return id;
}

void CBOS_semaphore_aquire(CBOS_semaphore_id_t calledSemaphore)
{
	__disable_irq();
	
	CBOS_semaphore_t * semaphore = CBOS_threadStatus.semaphore_head;

	//find the semaphore based on its ID
	for (uint8_t i = 0; i < calledSemaphore.semaphoreId; i++)
		semaphore = semaphore->next;

	//check if the count is 0, which means there are no more semaphores to aquire
	if (semaphore->count == 0)
	{
		CBOS_threadInfo_t * temp = semaphore->blocked_head;
		if (temp == NULL)
		{
			semaphore->blocked_head = CBOS_threadStatus.current_thread;
		} 
		
		else 
		{
			while (temp->next != NULL)
				temp = temp->next;

			temp->next = CBOS_threadStatus.current_thread;
		}
				
		//blocked
		CBOS_run_kernel();
	}

	else 
	{
		//only if there was a semaphore available, without having to wait should we decrement
		semaphore->count --;
	}
	__enable_irq();
}

void CBOS_semaphore_release(CBOS_semaphore_id_t calledSemaphore)
{
	__disable_irq();

	CBOS_semaphore_t * semaphore = CBOS_threadStatus.semaphore_head;

	//find the semaphore based on its ID
	for (uint8_t i = 0; i < calledSemaphore.semaphoreId; i++)
		semaphore = semaphore->next;
	
	//only increment if we are below the max count for the semaphore
	if (semaphore->count < semaphore->max_count)
	{
		CBOS_threadInfo_t * temp = semaphore->blocked_head;

		//only increment if there are no blocked threads waiting for this semaphore
		if(semaphore->blocked_head != NULL)
		{
			semaphore->blocked_head = semaphore->blocked_head->next;
			temp->next = NULL;
			CBOS_add_priority_queue(temp);
		}
		else
		{
			semaphore->count ++;
		}
	}
	__enable_irq();
}