	AREA	handle_pend,CODE,READONLY
	GLOBAL PendSV_Handler
		EXTERN set_PSP_new_stackPtr
	PRESERVE8
PendSV_Handler
		
		;write the magic value of 0xFFFFFFFD (seven F's then a D) to LR
		mov LR, #0xFFFFFFFD
		MRS r0, PSP
		;5. Put R4 through R11 onto the stack
		stmdb r0!,{r4-r11}
		;6. Save the thread’s stack pointer-- done in #3 in main.c function
		
		;7. Switch PSP so that it points to the new thread’s stack pointer 
			;call function to switch r0 to new function stack pointer
		BL set_PSP_new_stackPtr 
		mov LR, #0xFFFFFFFD
		
		MRS r0, PSP
				
		;8. Pop R11 through R4 (notice the reverse order)set r0 to new psp in order to access
		ldmia r0!,{r4-r11}
				
		;9. Return from the PendSV handler. The microcontroller will pop the rest of the registers out for you
		MSR PSP, r0

		
	
		;return - Note that, since LR has the magic value, the microcontroller reloads it from the 
		;stack frame it pushed on at the start of this call. Finding that value and aligning your stack
		;will 100% cause you grief while you try to do a context switch, but how to do that depends
		;so much on your design that we can't tell you how!
		BX LR
		
		END