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
		trigger_pendsv();
	}
}

void thread2()
{
	while(1)
	{
		printf("Thread 2, the cool thread is running!\n");
		trigger_pendsv();
	}
}


int main(void) {
	SystemInit();//set the LEDs to be outputs. You may or may not care about this
	printf("\n\n\nSystem initialized!\n");
	/* set the PendSV interrupt priority to the lowest level 0xFF,
		 which makes it very negative. In the ARM Cortex, negative priorities are 
		 high. It's frustrating. Think of them as "strong" priorities or something
	
		 The constant 0xE000ED20 is the location of the priority table for PendSV
	*/
	*(uint32_t volatile *)0xE000ED20 |= (0xFFU << 16);
		
	//These next three lines demonstrate how to find and set various stack pointers
	uint32_t* MSR_Original = (uint32_t*)0;
	uint32_t msrAddr = *MSR_Original;
	
	CBOS_threadStatus.initial_MSP_addr = msrAddr;
	
	//printf("MSR Originally at: %d\n",msrAddr);
	
	/*
		These next lines show how to switch from using MSP to using PSP. You need to switch the control
		register's bit 1 to 1 to indicate that we are using PSP, then immediately write a valid value to PSP
		before you try to use the stack
	*/
	//__set_CONTROL(1<<1);
	//__set_PSP((uint32_t)threadPSP);
	
	CBOS_kernel_initialize();
	
	CBOS_create_thread(thread1);
	CBOS_create_thread(thread2);
	CBOS_kernel_start();
	
	//The purpose of exercise 1 is to trigger PendSV and successfully return
	trigger_pendsv();
	
	printf("If you see this you successfully got out of PendSV\n");
	
	//Then do nothing
	while(1);
}
