#ifndef CBOS_FUNCTIONS_H
#define CBOS_FUNCTIONS_H

#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define THREAD_STACK_SIZE 0x200 //512 byte thread stack. This is a lot.
#define TIME_BETWEEN_SYSTICK_MS 1
typedef struct CBOS_threadInfo_t{
	void (*funct_ptr)(); // do we use this?
	uint32_t stackPtr_address;
	struct CBOS_threadInfo_t * next;
	uint8_t priority;
	uint32_t delay;
}CBOS_threadInfo_t;

typedef struct CBOS_mutex_t{
	uint8_t count;
	CBOS_threadInfo_t * owner;
	struct CBOS_mutex_t * next;
	CBOS_threadInfo_t * blocked_head; //linked list of all threads waiting on this mutex
}CBOS_mutex_t;

typedef struct{
	uint8_t mutexId;
}CBOS_mutex_id_t;


typedef struct {
	uint8_t thread_count; //eventually remove, we will just update initial_MSP_addr with the "next" thread location
	CBOS_threadInfo_t * current_thread;
	CBOS_threadInfo_t * next_thread;
	bool isCurrentThreadYield;
	uint32_t initial_MSP_addr;
	CBOS_threadInfo_t * priorityArray[10];
	CBOS_threadInfo_t * sleepingHead;
	CBOS_mutex_t * mutex_head;
	//array or linked list to store mutexes/semaphores?
}CBOS_status_t;

typedef struct{
	uint8_t count; 
	CBOS_threadInfo_t blocked_threads; //linked list of all threads waiting on this semaphore
}CBOS_semaphore_t;
void CBOS_add_priority_queue(CBOS_threadInfo_t *thread);

void CBOS_find_next_thread(void);

CBOS_mutex_id_t CBOS_create_mutex(void);

void CBOS_mutex_aquire(CBOS_mutex_id_t calledMutex);

void CBOS_mutex_release(CBOS_mutex_id_t calledMutex);

void CBOS_create_semaphore(void);

void CBOS_semaphore_aquire(void);

void CBOS_semaphore_release(void);

void CBOS_create_thread(void (*funct_ptr)(), uint8_t priority);

void contex_switch(void);

void idle_thread(void);

void CBOS_kernel_initialize(void);

void CBOS_kernel_start(void);

void CBOS_run_kernel(void);

void CBOS_run_scheduler(void);

extern CBOS_status_t CBOS_threadStatus;

void set_PSP_new_stackPtr(void);

void CBOS_delay(uint32_t ms);

void CBOS_yield(void);

void CBOS_update_sleeping_queue(void);

#endif
