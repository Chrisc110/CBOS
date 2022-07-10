#ifndef CBOS_FUNCTIONS_H
#define CBOS_FUNCTIONS_H

#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define THREAD_STACK_SIZE 0x200 //512 byte thread stack. This is a lot.

typedef struct{
	void (*funct_ptr)(); // do we use this?
	uint32_t stackPtr_address;
	CBOS_threadInfo_t * next;
	uint8_t priority;
}CBOS_threadInfo_t;

typedef struct{
	uint8_t count; 
	CBOS_threadInfo_t blocked_threads; //linked list of all threads waiting on this mutex
}CBOS_mutex_t;

typedef struct{
	uint8_t count; 
	CBOS_threadInfo_t blocked_threads; //linked list of all threads waiting on this semaphore
}CBOS_semaphore_t;

typedef struct {
	uint8_t thread_count;
	CBOS_threadInfo_t current_thread;
	uint32_t initial_MSP_addr;
	CBOS_threadInfo_t * threadInfo[10];
	//array or linked list to store mutexes/semaphores?
}CBOS_status_t;

void CBOS_add_priority_queue(uint8_t priority, CBOS_threadInfo_t thread);

void CBOS_next_thread(void);

void CBOS_create_mutex(void);

void CBOS_mutex_aquire(void);

void CBOS_mutex_release(void);

void CBOS_create_semaphore(void);

void CBOS_semaphore_aquire(void);

void CBOS_semaphore_release(void);

void CBOS_create_thread(void (*funct_ptr)(), uint8_t priority);

void trigger_pendsv(void);

void idle_thread(void);

void CBOS_kernel_initialize(void);

void CBOS_kernel_start(void);

extern CBOS_status_t CBOS_threadStatus;

void set_PSP_new_stackPtr(void);

#endif
