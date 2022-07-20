#ifndef CBOS_FUNCTIONS_H
#define CBOS_FUNCTIONS_H

#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define THREAD_STACK_SIZE 0x200 //512 byte thread stack. This is a lot.
#define SLEEPING_THREAD_MIN_DELAY 1 //ms
#define TIMESLICE 1000 //ms

//struct for individual thread info
typedef struct CBOS_threadInfo_t{
	void (*funct_ptr)();
	uint32_t stackPtr_address;
	struct CBOS_threadInfo_t * next;
	uint8_t priority;
	uint32_t delay;
}CBOS_threadInfo_t;

//struct for mutex info
typedef struct CBOS_mutex_t{
	uint8_t count;
	CBOS_threadInfo_t * owner;
	struct CBOS_mutex_t * next;
	CBOS_threadInfo_t * blocked_head;
}CBOS_mutex_t;

//struct for semaphore info
typedef struct CBOS_semaphore_t{
	uint8_t count;
	uint8_t max_count;
	struct CBOS_semaphore_t * next;
	CBOS_threadInfo_t * blocked_head;
}CBOS_semaphore_t;

//struct for mutex ID
typedef struct{
	uint8_t mutexId;
}CBOS_mutex_id_t;

//struct for semaphore ID
typedef struct{
	uint8_t semaphoreId;
}CBOS_semaphore_id_t;

//struct of overall RTOS information
typedef struct {
	uint32_t system_count;
	uint8_t thread_count;
	CBOS_threadInfo_t * current_thread;
	CBOS_threadInfo_t * next_thread;
	uint32_t initial_MSP_addr;
	CBOS_threadInfo_t * priorityArray[10];
	CBOS_threadInfo_t * sleepingHead;
	CBOS_mutex_t * mutex_head;
	CBOS_semaphore_t * semaphore_head;
}CBOS_status_t;

/**
 * @brief adds the given thread to the priority queue
 * 
 * @param thread is the thread to add
 */
void CBOS_add_priority_queue(CBOS_threadInfo_t *thread);

/**
 * @brief  creates a mutex for the user
 * 
 * @return CBOS_mutex_id_t structure containing the id of the mutex
 * this id is used later as it corresponds to the mutex's index in the 
 * linked list of mutexes
 */
CBOS_mutex_id_t CBOS_create_mutex(void);

/**
 * @brief aquires a mutex if it is available.  If not, the current thread is pushed onto the mutexe's
 * respective blocked queue and poped later once the mutex is released by it's owner.
 * Once the thread is popped, when it is eventually run by the scheduler the thread 
 * will assume it has already aquired the mutex (taken care of in release)
 * 
 * @param calledMutex passes in the mutex id so CBOS knows which mutex is being aquired
 */
void CBOS_mutex_aquire(CBOS_mutex_id_t calledMutex);

/**
 * @brief mutex release first checks if the current thread owns the mutex. The thread will remove 
 * itself as owner and set count to 1 if there are no threads in the blocked queue.  If there are 
 * threads in the queue, it will set the owner to be the waiting thread and pop that thread to the 
 * priority queue so it will be the next to run and only thread to aquire the mutex next (leaves count at 0)
 * 
 * @param calledMutex passes in the mutex id so CBOS knows which mutex is being aquired
 */
void CBOS_mutex_release(CBOS_mutex_id_t calledMutex);

/**
 * @brief creates a semaphore for the user
 * 
 * @param max_count sets maximum value the count can increment to
 * @param starting_count sets the initial count the semaphore will have at run time
 * @return CBOS_semaphore_id_t passes in the semaphore id so CBOS knows which mutex is being aquired
 */
CBOS_semaphore_id_t CBOS_create_semaphore(uint8_t max_count, uint8_t starting_count);

/**
 * @brief aquires a semaphore if it is available.  If not, the current thread is pushed onto the semaphore's
 * respective blocked queue and poped later once the semaphore is released by another thread.
 * Once the thread is popped, when it is eventually run by the scheduler the thread 
 * will assume it has already aquired the semaphore (taken care of in release)
 * 
 * @param calledSemaphore passes in the semaphore id so CBOS knows which mutex is being aquired
 */
void CBOS_semaphore_aquire(CBOS_semaphore_id_t calledSemaphore);

/**
 * @brief will increment count if there are no threads in the blocked queue.  If there are 
 * threads in the queue, it will pop the next thread to the priority queue so it will be the 
 * next to run and only thread to aquire the semaphore next (leaves count at 0)
 * 
 * @param calledSemaphore passes in the semaphore id so CBOS knows which mutex is being aquired
 */
void CBOS_semaphore_release(CBOS_semaphore_id_t calledSemaphore);

/**
 * @brief creates a thread for the user
 * 
 * @param funct_ptr stores the function pointer to the thread's function
 * @param priority sets the priority of the thread fro scheduling
 */
void CBOS_create_thread(void (*funct_ptr)(), uint8_t priority);

/**
 * @brief triggers PendSV interrupt to switch between threads
 * 
 */
void context_switch(void);

/**
 * @brief priority 9 thread to only be run if no user-created thread is available. 
 * Constantly calls yield to allow other threads the oppertunity to run as soon as possible
 * 
 */
void idle_thread(void);

/**
 * @brief initializes idle thread and saves MSR for create thread function
 * 
 */
void CBOS_kernel_initialize(void);

/**
 * @brief initializes the systick interrrupt and sets current thread and next thread.
 * The psp is set to the current thread and a context switch is triggered from the current
 * thread to the next thread 
 * 
 */
void CBOS_kernel_start(void);

/**
 * @brief calls the scheduler and enables a context switch
 * 
 */
void CBOS_run_kernel(void);

/**
 * @brief iterates through the priority queue and finds the highest priority non-null thread in 
 * the 'next_thread' variable as well as pops this thread from the priority queue
 * 
 */
void CBOS_run_scheduler(void);

extern CBOS_status_t CBOS_threadStatus;

/**
 * @brief Set the PSP new stackPtr object is called in the PendSV to save the current PSP as well
 * as switch the current thread to the next thread previously decided by our scheduler
 * 
 */
void set_PSP_new_stackPtr(void);

/**
 * @brief delays the current function
 * 
 * @param ticks  - number of sysTick interrupts before the thread will be unblocked again
 */
void CBOS_delay(uint32_t ticks);

/**
 * @brief allows the current thread to yield to a thread of equal or higher priority
 * 
 */
void CBOS_yield(void);

/**
 * @brief updates all delayed threads once every systick interrupt
 * 
 */
void CBOS_update_sleeping_queue(void);

#endif
