/* -------------------------------- Arctic Core ------------------------------
 * Arctic Core - the open source AUTOSAR platform http://arccore.com
 *
 * Copyright (C) 2009  ArcCore AB <contact@arccore.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * -------------------------------- Arctic Core ------------------------------*/

/* ----------------------------[includes]------------------------------------*/
/* ----------------------------[private define]------------------------------*/
/* ----------------------------[private macro]-------------------------------*/
/* ----------------------------[private typedef]-----------------------------*/
/* ----------------------------[private function prototypes]-----------------*/
/* ----------------------------[private variables]---------------------------*/
/* ----------------------------[private functions]---------------------------*/
/* ----------------------------[public functions]----------------------------*/


#include "internal.h"
#include "irq_types.h"
#include "mpc55xx.h"


#include "pcb.h"
#include "sys.h"
#include "internal.h"
#include "task_i.h"
#include "hooks.h"


#include "debug.h"

#include "irq.h"

typedef void (*f_t)( uint32_t *);
extern void exception_tbl(void);


extern void * Irq_VectorTable[NUMBER_OF_INTERRUPTS_AND_EXCEPTIONS];
//extern uint8 Irq_IsrTypeTable[NUMBER_OF_INTERRUPTS_AND_EXCEPTIONS];

// write 0 to pop INTC stack
void Irq_Init( void ) {
	  // Check alignment for the exception table
	  assert(((uint32)exception_tbl & 0xfff)==0);
	  set_spr(SPR_IVPR,(uint32)exception_tbl);

	  ramlog_str("Test\n");
	  ramlog_hex(0x10);
	  ramlog_dec(20);

	  // TODO: The 5516 simulator still thinks it's a 5554 so setup the rest
#if (defined(CFG_SIMULATOR) && defined(CFG_MPC5516)) || defined(CFG_MPC5567) || defined(CFG_MPC5554)
	    set_spr(SPR_IVOR0,((uint32_t)&exception_tbl+0x0) );
	    set_spr(SPR_IVOR1,((uint32_t)&exception_tbl+0x10) );
	    set_spr(SPR_IVOR2,((uint32_t)&exception_tbl+0x20) );
	    set_spr(SPR_IVOR3,((uint32_t)&exception_tbl+0x30) );
	    set_spr(SPR_IVOR4,((uint32_t)&exception_tbl+0x40) );
	    set_spr(SPR_IVOR5,((uint32_t)&exception_tbl+0x50) );
	    set_spr(SPR_IVOR6,((uint32_t)&exception_tbl+0x60) );
	    set_spr(SPR_IVOR7,((uint32_t)&exception_tbl+0x70) );
	    set_spr(SPR_IVOR8,((uint32_t)&exception_tbl+0x80) );
	    set_spr(SPR_IVOR9,((uint32_t)&exception_tbl+0x90) );
	    set_spr(SPR_IVOR10,((uint32_t)&exception_tbl+0xa0) );
	    set_spr(SPR_IVOR11,((uint32_t)&exception_tbl+0xb0) );
	    set_spr(SPR_IVOR12,((uint32_t)&exception_tbl+0xc0) );
	    set_spr(SPR_IVOR13,((uint32_t)&exception_tbl+0xd0) );
	    set_spr(SPR_IVOR14,((uint32_t)&exception_tbl+0xe0) );
#if defined(CFG_SPE)
	    // SPE exceptions...map to dummy
	    set_spr(SPR_IVOR32,((uint32_t)&exception_tbl+0xf0) );
	    set_spr(SPR_IVOR33,((uint32_t)&exception_tbl+0xf0) );
	    set_spr(SPR_IVOR34,((uint32_t)&exception_tbl+0xf0) );
#endif
#endif

	  //
	  // Setup INTC
	  //
	  // according to manual
	  //
	  // 1. configure VTES_PRC0,VTES_PRC1,HVEN_PRC0 and HVEN_PRC1 in INTC_MCR
	  // 2. configure VTBA_PRCx in INTC_IACKR_PRCx
	  // 3. raise the PRIx fields and set the PRC_SELx fields to the desired processor in INTC_PSRx_x
	  // 4. set the enable bits or clear the mask bits for the peripheral interrupt requests
	  // 5. lower PRI in INTC_CPR_PRCx to zero
	  // 6. enable processor(s) recognition of interrupts

	  // Z1 init

	#if defined(CFG_MPC5516)
	  INTC.MCR.B.HVEN_PRC0 = 0; // Soft vector mode
	  INTC.MCR.B.VTES_PRC0 = 0; // 4 byte offset between entries
	#elif defined(CFG_MPC5554) || defined(CFG_MPC5567) || defined(CFG_MPC5606S)
	  INTC.MCR.B.HVEN = 0; // Soft vector mode
	  INTC.MCR.B.VTES = 0; // 4 byte offset between entries
	#endif



#if 1
	  // Check alignment requirements for the INTC table
	  assert( (((uint32_t)&Irq_VectorTable[0]) & 0x7ff) == 0 );
	#if defined(CFG_MPC5516)
	  INTC.IACKR_PRC0.R = (uint32_t) & Irq_VectorTable[0]; // Set INTC ISR vector table
	#elif defined(CFG_MPC5606S)
	  INTC.IACKR.R = (uint32_t) & Irq_VectorTable[0];
	#elif defined(CFG_MPC5554) || defined(CFG_MPC5567)
	  INTC.IACKR.R = (uint32_t) & Irq_VectorTable[0]; // Set INTC ISR vector table
	#endif
#endif
	  // Pop the FIFO queue
	  for (int i = 0; i < 15; i++)
	  {
	#if defined(CFG_MPC5516)
	    INTC.EOIR_PRC0.R = 0;
	#elif defined(CFG_MPC5554) || defined(CFG_MPC5567) || defined(CFG_MPC5606S)
	    INTC.EOIR.R = 0;
	#endif
	  }

	  // Accept interrupts
	#if defined(CFG_MPC5516)
	  INTC.CPR_PRC0.B.PRI = 0;
	#elif defined(CFG_MPC5554) || defined(CFG_MPC5567) || defined(CFG_MPC5606S)
	  INTC.CPR.B.PRI = 0;
	#endif

}

void Irq_EOI( void ) {
#if defined(CFG_MPC5516)
	volatile struct INTC_tag *intc = &INTC;
	intc->EOIR_PRC0.R = 0;
#elif defined(CFG_MPC5554)||defined(CFG_MPC5567) || defined(CFG_MPC5606S)
	volatile struct INTC_tag *intc = &INTC;
	#if defined(CFG_MPC5606S)
	RTC.RTCC.B.CNTEN = 0;
	RTC.RTCC.B.CNTEN = 1;
	RTC.RTCS.B.RTCF = 1;
	#endif
	intc->EOIR.R = 0;
#endif
}

#if 0
/**
 *
 * @param stack_p Ptr to the current stack.
 *
 * The stack holds C, NVGPR, VGPR and the EXC frame.
 *
 */
void *Irq_Entry( void *stack_p )
{
	uint32_t vector;
	volatile struct INTC_tag *intc = &INTC;

#if defined(CFG_MPC5516)
	vector = (intc->IACKR_PRC0.B.INTVEC_PRC0);
#elif defined(CFG_MPC5554)||defined(CFG_MPC5567) || defined(CFG_MPC5606S)
	vector = (intc->IACKR.B.INTVEC);
#endif
	// Clear software int if that was it.
	if(vector<=INTC_SSCIR0_CLR7)
	{
		intc->SSCIR[vector].B.CLR = 1;
		asm("mbar 0");
	}

	if( Irq_GetIsrType(vector) == ISR_TYPE_1 ) {
		// It's a function, just call it.
		((func_t)Irq_VectorTable[vector])();
		return stack_p;
	} else {
		// It's a PCB
		// Let the kernel handle the rest,
		return Os_Isr(stack_p, (void *)Irq_VectorTable[vector]);
	}
}

#endif




static inline int osPrioToCpuPio( uint8_t prio ) {
	assert(prio<32);
	return prio>>1;		// Os have 32 -> 16
}

void Irq_SetPriority( Cpu_t cpu,  IrqType vector, uint8_t prio ) {
#if defined(CFG_MPC5516)
	INTC.PSR[vector].B.PRC_SEL = cpu;
#endif
	INTC.PSR[vector].B.PRI = prio;
}



/**
 * Attach an ISR type 1 to the interrupt controller.
 *
 * @param entry
 * @param int_ctrl
 * @param vector
 * @param prio
 */
void Irq_AttachIsr1( void (*entry)(void), void *int_ctrl, uint32_t vector,uint8_t prio) {
	Irq_VectorTable[vector] = (void *)entry;
	Irq_SetIsrType(vector, ISR_TYPE_1);

	if (vector < INTC_NUMBER_OF_INTERRUPTS) {
		Irq_SetPriority(CPU_CORE0,vector + IRQ_INTERRUPT_OFFSET, osPrioToCpuPio(prio));
	} else if ((vector >= CRITICAL_INPUT_EXCEPTION) && (vector
			<= DEBUG_EXCEPTION)) {
	} else {
		/* Invalid vector! */
		assert(0);
	}

}

/**
 * Attach a ISR type 2 to the interrupt controller.
 *
 * @param tid
 * @param int_ctrl
 * @param vector
 */
void Irq_AttachIsr2(TaskType tid,void *int_ctrl,IrqType vector ) {
	OsPcbType *pcb;

	pcb = os_find_task(tid);
	Irq_VectorTable[vector] = pcb;
	Irq_IsrTypeTable[vector] = PROC_ISR2;

	if (vector < INTC_NUMBER_OF_INTERRUPTS) {
		Irq_SetPriority(CPU_CORE0,vector + IRQ_INTERRUPT_OFFSET, osPrioToCpuPio(pcb->prio));
	} else if ((vector >= CRITICAL_INPUT_EXCEPTION) && (vector
			<= DEBUG_EXCEPTION)) {
	} else {
		/* Invalid vector! */
		assert(0);
	}
}


/**
 * Generates a soft interrupt
 * @param vector
 */
void Irq_GenerateSoftInt( IrqType vector ) {
	if( vector > INTC_SSCIR0_CLR7 ) {
		assert(0);
	}

	INTC.SSCIR[vector].B.SET = 1;
}

/**
 * Get the current priority from the interrupt controller.
 * @param cpu
 * @return
 */
uint8_t Irq_GetCurrentPriority( Cpu_t cpu) {

	uint8_t prio = 0;

#if defined(CFG_MPC5516)
	if( cpu == CPU_Z1 ) {
		prio = INTC.CPR_PRC0.B.PRI;
	} else if ( cpu == CPU_Z0 ) {
		prio = INTC.CPR_PRC1.B.PRI;
	}
#elif defined(CFG_MPC5554)||defined(CFG_MPC5567) || defined(CFG_MPC5606S)
	prio = INTC.CPR.B.PRI;
#endif

	return prio;
}


