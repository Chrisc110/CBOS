/*
 * Default main.c for rtos lab.
 * @author Andrew Morton, 2018
 */
#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "CBOS_functions.h"

#define CASE 2

CBOS_mutex_id_t mutex; 
uint32_t count = 0;
CBOS_semaphore_id_t semaphore;
	
void case1_thread1()
{
	while(1)
	{
		CBOS_mutex_aquire(mutex);
		count++;
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
		printf("Thread 2 is running: %d\n", count);
		CBOS_mutex_release(mutex);
	}
}

uint8_t n = 3; 
uint8_t c2_count = 0;
CBOS_mutex_id_t c2_mutex;
CBOS_semaphore_id_t c2_turnstile1; //start at 1
CBOS_semaphore_id_t c2_turnstile2; //start at 2

void synchronize(uint8_t thread_num)
{
	//taken directly from the lecture notes

	CBOS_mutex_aquire(c2_mutex);
	c2_count++;
	if (c2_count == n)
	{
		CBOS_semaphore_aquire(c2_turnstile2);
		CBOS_semaphore_release(c2_turnstile1);
	}
	CBOS_mutex_release(c2_mutex);

	CBOS_semaphore_aquire(c2_turnstile1);
	CBOS_semaphore_release(c2_turnstile1);

	printf("Thread %d part 1\n", thread_num);

	CBOS_mutex_aquire(c2_mutex);
	c2_count--;
	if (c2_count == 0)
	{
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
		synchronize(1);	
	}
}

void case2_thread2()
{
	while(1)
	{
		synchronize(2);	
	}
}

void case2_thread3()
{
	while(1)
	{
		synchronize(3);	
	}
}

void case3_thread1()
{
	while(1)
	{
		for (uint8_t i = 0; i < 200; i++)
			printf("Thread 1 is running\n");
		CBOS_delay(2000);
	}
}

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
	
	//Creating threads and starting "Kernel" 
	if (CASE == 1)
	{
		mutex = CBOS_create_mutex();
		CBOS_create_thread(case1_thread1, 1);
		CBOS_create_thread(case1_thread2, 1);
	}

	else if (CASE == 2)
	{
		CBOS_mutex_id_t c2_mutex;
		CBOS_semaphore_id_t c2_turnstile1; //start at 1
		CBOS_semaphore_id_t c2_turnstile2;

		c2_mutex = CBOS_create_mutex();
		c2_turnstile1 = CBOS_create_semaphore(6, 0);
		c2_turnstile2 = CBOS_create_semaphore(6, 1);
		
		printf("Turn 1: %d\n", c2_turnstile1.semaphoreId);
		printf("Turn 2: %d\n", c2_turnstile2.semaphoreId);


		CBOS_create_thread(case2_thread1, 1);
		CBOS_create_thread(case2_thread2, 1);
		CBOS_create_thread(case2_thread3, 1);
	}

	else if (CASE == 3)
	{
		CBOS_create_thread(case3_thread1, 1);
		CBOS_create_thread(case3_thread2, 2);
		//CBOS_create_thread(case3_thread3, 2);
	}

	CBOS_kernel_start();
	
	while(1);
	
	//Should never get here
	return 0;
}
