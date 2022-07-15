/*
 * Default main.c for rtos lab.
 * @author Andrew Morton, 2018
 */
#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "CBOS_functions.h"

void thread1()
{
	while(1)
	{
		printf("Thread 1 is running\n");
		CBOS_delay(4);
		printf("Thread 1 part 2 is running\n");
	}
}

void thread2()
{
	while(1)
	{
		printf("Thread 2, the cool thread is running!\n");
		//trigger_pendsv();
		printf("Thread 2, the cool thread part 2 is running!\n");
	}
}


int main(void) {
	SystemInit();//set the LEDs to be outputs. You may or may not care about this
	CBOS_kernel_initialize();
	
	printf("\n\n\nSystem initialized!\n");
	
	//Creating threads and starting "Kernel" 
	CBOS_create_thread(thread1, 1);
	CBOS_create_thread(thread2, 1);

	CBOS_kernel_start();
	
	while(1);
	
	//Should never get here
	return 0;
}
