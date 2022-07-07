	AREA	handle_pend,CODE,READONLY
	GLOBAL PendSV_Handler
		EXTERN set_PSP_new_stackPtr
	PRESERVE8
PendSV_Handler
		
		;setting r0 to the address of PSP
		MRS r0, PSP
		
		;5. Put R4 through R11 onto the stack, "saving its context"
		stmdb r0!,{r4-r11}
		
		;since we are saving the PSP in set_PSP_new_stackPtr, 
		;we are making sure that it is up to date with r0
		MSR PSP,r0
		
		;calling the c function which does the following:
		;1. Save the PSP for next time we want to enter the thread
		;2. Increment the "current thread" index to the new thread we want to run
		;3. Set the PSP to whatever it was before when we left the new thread
		BL set_PSP_new_stackPtr 
		
		;setting LR which does the important following things:
		;1. Go back into thread mode
		;2. Return to the PSP
		mov LR, #0xFFFFFFFD
		
		;setting r0 to the address of the new thread's PSP
		MRS r0, PSP
		
		
		;8. Pop R11 through R4, "restoring its context"
		ldmia r0!,{r4-r11}
		
		;9. Return from the PendSV handler. The microcontroller will pop the rest of the registers out for you
		MSR PSP, r0

		BX LR
		
		END