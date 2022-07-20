#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "CBOS_functions.h"

#define CASE 3 //Change this to run the different test cases 1-3

/****************************** TEST CASE 1 *****************************/
CBOS_mutex_id_t mutex; 
uint32_t count = 0;
	
void case1_thread1()
{
	while(1)
	{
		CBOS_mutex_aquire(mutex);
		count++;
		CBOS_delay(100);
		printf("Thread 1 is running: %d\n", count);
		CBOS_mutex_release(mutex);
	}
}

void case1_thread2()
{
	while(1)
	{
		CBOS_mutex_aquire(mutex);
		count++;
		CBOS_delay(100);
		printf("Thread 2 is running: %d\n", count);
		CBOS_mutex_release(mutex);
	}
}

/****************************** TEST CASE 2 *****************************/
uint8_t n = 3; 
uint8_t c2_count = 0;
CBOS_mutex_id_t c2_mutex;
CBOS_semaphore_id_t c2_turnstile1; //initial value at 0
CBOS_semaphore_id_t c2_turnstile2; //initial value at 1

void synchronize(uint8_t thread_num)
{
	// This code is taken directly from the lecture notes and implements a reusable synchronizing barrier
	CBOS_mutex_aquire(c2_mutex);
	CBOS_delay(thread_num*100);
	c2_count++;
	if (c2_count == n)
	{
		CBOS_semaphore_aquire(c2_turnstile2);
		CBOS_semaphore_release(c2_turnstile1);
	}
	CBOS_mutex_release(c2_mutex);
	
	printf("Thread %d waiting\n", thread_num);
	
	CBOS_semaphore_aquire(c2_turnstile1);
	CBOS_semaphore_release(c2_turnstile1);

	printf("Thread %d part 1\n", thread_num);

	CBOS_mutex_aquire(c2_mutex);
	c2_count--;
	if (c2_count == 0)
	{
		CBOS_delay(thread_num*100);
		CBOS_semaphore_aquire(c2_turnstile1);
		CBOS_semaphore_release(c2_turnstile2);
	}
	CBOS_mutex_release(c2_mutex);

	CBOS_semaphore_aquire(c2_turnstile2);
	CBOS_semaphore_release(c2_turnstile2);

	printf("Thread %d part 2\n", thread_num);
}

void case2_thread1()
{
	while(1)
	{
		//implement the synchronizing barrier
		synchronize(1);	
	}
}

void case2_thread2()
{
	while(1)
	{
		//implement the synchronizing barrier
		synchronize(2);	
	}
}

void case2_thread3()
{
	while(1)
	{
		//implement the synchronizing barrier
		synchronize(3);	
	}
}

void case3_thread1()
{
	while(1)
	{
		//to ensure we see this thread running on the serial console
		for (uint8_t i = 0; i < 200; i++)
			printf("Thread 1 is running\n");

		CBOS_delay(200); //delay to allow other threads to run
	}
}

//this thread is run co-operatively with thread 3
void case3_thread2()
{
	while(1)
	{
		printf("Thread 2 is running\n");
		CBOS_yield();
	}
}

void case3_thread3()
{
	while(1)
	{
		printf("Thread 3 is running\n");
		CBOS_yield();
	}
}


int main(void) {
	SystemInit();//set the LEDs to be outputs. You may or may not care about this
	CBOS_kernel_initialize(); 
	
	printf("\n\n\nSystem initialized!\n");
	
	//creating the test case threads
	if (CASE == 1)
	{
		mutex = CBOS_create_mutex();
		CBOS_create_thread(case1_thread1, 1);
		CBOS_create_thread(case1_thread2, 1);
	}

	else if (CASE == 2)
	{
		CBOS_mutex_id_t c2_mutex;

		c2_mutex = CBOS_create_mutex();
		c2_turnstile1 = CBOS_create_semaphore(6, 0);
		c2_turnstile2 = CBOS_create_semaphore(6, 1);

		CBOS_create_thread(case2_thread1, 1);
		CBOS_create_thread(case2_thread2, 1);
		CBOS_create_thread(case2_thread3, 1);
	}

	else if (CASE == 3)
	{
		CBOS_create_thread(case3_thread1, 1);
		CBOS_create_thread(case3_thread2, 2);
		CBOS_create_thread(case3_thread3, 2);
	}

	CBOS_kernel_start();
	
	while(1);
	
	//Should never get here
	return 0;
}
