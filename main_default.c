/*
 * Default main.c for rtos lab.
 * @author Andrew Morton, 2018
 */
#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "CBOS_functions.h"


CBOS_mutex_id_t mutex; 
uint8_t count = 0;

CBOS_semaphore_id_t semaphore;
	
void thread1()
{
	while(1)
	{
		CBOS_semaphore_aquire(semaphore);
		count++;
		printf("Thread 1 is running %d\n", count);
		CBOS_semaphore_release(semaphore);
		//CBOS_yield();
	}
}

void thread2()
{
	while(1)
	{
		CBOS_semaphore_aquire(semaphore);
		count++;
		printf("Thread 2 is running %d\n", count);
		CBOS_semaphore_release(semaphore);
		//CBOS_yield();
	}
}

int main(void) {
	SystemInit();//set the LEDs to be outputs. You may or may not care about this
	CBOS_kernel_initialize();
	
	printf("\n\n\nSystem initialized!\n");
	
	//Creating threads and starting "Kernel" 
	mutex = CBOS_create_mutex();
	semaphore = CBOS_create_semaphore(1);
	CBOS_create_thread(thread1, 1);
	CBOS_create_thread(thread2, 1);

	CBOS_kernel_start();
	
	while(1);
	
	//Should never get here
	return 0;
}
