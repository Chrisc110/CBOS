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

void contex_switch(void)
{
	//Determine whether a switch is even necessary � if there is only one task running of that priority, why bother?
	//if there are no other threads in the same priority queue as the current thread, no context switch is needed 
	//in the case that a higher priority thread has been unblocked, this will be taken care of when the systick handler is called
	//if (CBOS_threadStatus.priorityArray[CBOS_threadStatus.current_thread->priority] == NULL)//->thread info?
		//return;
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
	__disable_irq();
	if (CBOS_threadStatus.priorityArray[thread->priority] == NULL)
		CBOS_threadStatus.priorityArray[thread->priority] = thread;
	else
	{
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
		CBOS_yield();
		//printf("Idle Thread\n");
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
	CBOS_threadStatus.current_thread = CBOS_threadStatus.next_thread;
	__set_CONTROL(1<<1);
	__set_PSP(CBOS_threadStatus.current_thread->stackPtr_address + 12*4); //cannot guarantee there is a thread here... how else do we decide?
	
	//setup systick timer
	SysTick_Config(SystemCoreClock/200);
	
	//Start the first switch
	contex_switch();
}

void CBOS_run_kernel(void)
{
	__disable_irq();	
	CBOS_run_scheduler();

	if (CBOS_threadStatus.next_thread != CBOS_threadStatus.current_thread)
	{
		contex_switch();
	}
	__enable_irq();
	//else do nothing
}

//not used so far
void CBOS_update_mutex_threads(void)
{
	CBOS_mutex_t * current = CBOS_threadStatus.mutex_head;
	
	while(current != NULL)
	{
		if (current->count == 1)
		{
			//pop the next blocked thread waiting on that mutex
			if (current->blocked_head != NULL)
			{
				//get the first blcoked thread
				CBOS_threadInfo_t * temp = current->blocked_head;
				//make the new blocked head the next one waiting
				current->blocked_head = current->blocked_head->next;
					
				//cut the old head off
				temp->next = NULL;
					
				//add it to the ready queue
				CBOS_add_priority_queue(temp);
			}
		}
		current = current->next;
	}
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
	
	//MAKE SURE YOU DEFINE AN IDLE THREAD SO WE ALWAYS REACH A THREAD
	
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
	
	CBOS_threadStatus.current_thread = CBOS_threadStatus.next_thread;
	
	CBOS_threadStatus.next_thread = NULL;
}

void SysTick_Handler(void){
	CBOS_threadStatus.system_count++;
	//check delayed threads and put them back to ready queue if done
	CBOS_update_sleeping_queue();
	if (CBOS_threadStatus.system_count % TIMESLICE == 0)
	{
		//check delayed threads and put them back to ready queue if done
		//put current thread back in ready queue
		if (CBOS_threadStatus.next_thread == NULL)
		{
			CBOS_add_priority_queue(CBOS_threadStatus.current_thread);
			//run kernel
			CBOS_run_kernel();
		}
	}

}

void CBOS_update_sleeping_queue(void)
{
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

void CBOS_delay(uint32_t ms)
{
	//the delay will be in terms of ms bec our systick handler will fire every 1ms
	
	CBOS_threadInfo_t * temp = CBOS_threadStatus.sleepingHead;
	if (temp == NULL)
	{
		temp = CBOS_threadStatus.current_thread;
		CBOS_threadStatus.sleepingHead = CBOS_threadStatus.current_thread;
	}
	
	else 
	{
		while (temp->next != NULL)
			temp = temp->next;

		temp->next = CBOS_threadStatus.current_thread;
	}
	temp->delay = ms + 1;

	CBOS_run_kernel();
}

void CBOS_yield(void)
{
	CBOS_add_priority_queue(CBOS_threadStatus.current_thread);
	CBOS_run_kernel();
}

CBOS_mutex_id_t CBOS_create_mutex(void)
{
	__disable_irq();
	CBOS_mutex_t * mutex = (CBOS_mutex_t*)malloc(sizeof(CBOS_mutex_t));
	mutex->count = 1;
	mutex->owner = NULL;
	mutex->next = NULL;
	mutex->blocked_head = NULL;
	
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
	__enable_irq();
	id.mutexId = count;
	return id;
}

void CBOS_mutex_aquire(CBOS_mutex_id_t calledMutex)
{
	__disable_irq();
	
	CBOS_mutex_t * mutex = CBOS_threadStatus.mutex_head;
	for (uint8_t i = 0; i < calledMutex.mutexId; i++)
		mutex = mutex->next;
	
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
		
		__enable_irq();
		
		//blocked
		CBOS_run_kernel();
	}
	
	__disable_irq();
	mutex->count = 0;
	mutex->owner = CBOS_threadStatus.current_thread;
	__enable_irq();
	
}

void CBOS_mutex_release(CBOS_mutex_id_t calledMutex)
{
	__disable_irq();
	CBOS_mutex_t * mutex = CBOS_threadStatus.mutex_head;
	for (uint8_t i = 0; i < calledMutex.mutexId; i++)
		mutex = mutex->next;
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
	
	uint8_t count = 0;
	CBOS_semaphore_id_t id;
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
	for (uint8_t i = 0; i < calledSemaphore.semaphoreId; i++)
		semaphore = semaphore->next;
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
		
		//__enable_irq();
		
		//blocked
		CBOS_run_kernel();
	}
	else 
	{
		semaphore->count --;
	}
	__enable_irq();
}

void CBOS_semaphore_release(CBOS_semaphore_id_t calledSemaphore)
{
	__disable_irq();
	CBOS_semaphore_t * semaphore = CBOS_threadStatus.semaphore_head;
	for (uint8_t i = 0; i < calledSemaphore.semaphoreId; i++)
		semaphore = semaphore->next;
	if (semaphore->count < semaphore->max_count)
	{
		CBOS_threadInfo_t * temp = semaphore->blocked_head;
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