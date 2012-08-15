//
// Stuff that's neither inline or generated by gencpu.c, but depended upon by
// cpuemu.c.
//
// Originally part of UAE by Bernd Schmidt
// and released under the GPL v2 or later
//

#include "cpuextra.h"
#include "cpudefs.h"
#include "inlines.h"


uint16_t last_op_for_exception_3;		// Opcode of faulting instruction
uint32_t last_addr_for_exception_3;		// PC at fault time
uint32_t last_fault_for_exception_3;	// Address that generated the exception

int OpcodeFamily;						// Used by cpuemu.c...
int BusCyclePenalty = 0;				// Used by cpuemu.c...
int CurrentInstrCycles;

struct regstruct regs;

//
// Make displacement effective address for 68000
//
uint32_t get_disp_ea_000(uint32_t base, uint32_t dp)
{
	int reg = (dp >> 12) & 0x0F;
	int32_t regd = regs.regs[reg];

#if 1
	if ((dp & 0x800) == 0)
		regd = (int32_t)(int16_t)regd;

	return base + (int8_t)dp + regd;
#else
	/* Branch-free code... benchmark this again now that
	 * things are no longer inline.
	 */
	int32_t regd16;
	uint32_t mask;
	mask = ((dp & 0x800) >> 11) - 1;
	regd16 = (int32_t)(int16_t)regd;
	regd16 &= mask;
	mask = ~mask;
	base += (int8_t)dp;
	regd &= mask;
	regd |= regd16;
	return base + regd;
#endif
}

//
// Create the Status Register from the flags
//
void MakeSR(void)
{
	regs.sr = ((regs.s << 13) | (regs.intmask << 8) | (GET_XFLG << 4)
		| (GET_NFLG << 3) | (GET_ZFLG << 2) | (GET_VFLG << 1) | GET_CFLG);
}

//
// Set up the flags from Status Register
//
void MakeFromSR(void)
{
	int olds = regs.s;

	regs.s = (regs.sr >> 13) & 1;
	regs.intmask = (regs.sr >> 8) & 7;
	SET_XFLG((regs.sr >> 4) & 1);
	SET_NFLG((regs.sr >> 3) & 1);
	SET_ZFLG((regs.sr >> 2) & 1);
	SET_VFLG((regs.sr >> 1) & 1);
	SET_CFLG(regs.sr & 1);

	if (olds != regs.s)
	{
		if (olds)
		{
			regs.isp = m68k_areg(regs, 7);
			m68k_areg(regs, 7) = regs.usp;
		}
		else
		{
			regs.usp = m68k_areg(regs, 7);
			m68k_areg(regs, 7) = regs.isp;
		}
	}

	/* Pending interrupts can occur again after a write to the SR: */
//JLH: is this needed?
//	set_special(SPCFLAG_DOINT);
}

//
// Rudimentary exception handling. This is really stripped down from what
// was in Hatari.
/*
NB: Seems that when an address exception occurs, it doesn't get handled properly
    as per test1.cof. Need to figure out why it keeps going when it should wedge. :-P
*/
//
// Handle exceptions. We need a special case to handle MFP exceptions
// on Atari ST, because it's possible to change the MFP's vector base
// and get a conflict with 'normal' cpu exceptions.
//
void Exception(int nr, uint32_t oldpc, int ExceptionSource)
{
char excNames[33][64] = {
	"???", "???", "Bus Error", "Address Error",
	"Illegal Instruction", "Zero Divide", "CHK", "TrapV",
	"Privilege Violation", "Trace", "Line A", "Line F",
	"???", "???", "Format Error", "Uninitialized Interrupt",
	"???", "???", "???", "???",
	"???", "???", "???", "???",
	"Spurious/Autovector", "???", "???", "???",
	"???", "???", "???", "???",
	"Trap #"
};

printf("Exception #%i occurred! (%s)\n", nr, (nr < 32 ? excNames[nr] : (nr < 48 ? "Trap #" : "????")));
printf("Vector @ #%i = %08X\n", nr, m68k_read_memory_32(nr * 4));
//abort();
	uint32_t currpc = m68k_getpc(), newpc;
printf("PC = $%08X\n", currpc);
printf("A0 = $%08X A1 = $%08X A2 = $%08X A3 = $%08X\n", m68k_areg(regs, 0), m68k_areg(regs, 1), m68k_areg(regs, 2), m68k_areg(regs, 3));
printf("A4 = $%08X A5 = $%08X A6 = $%08X A7 = $%08X\n", m68k_areg(regs, 4), m68k_areg(regs, 5), m68k_areg(regs, 6), m68k_areg(regs, 7));
printf("D0 = $%08X D1 = $%08X D2 = $%08X D3 = $%08X\n", m68k_dreg(regs, 0), m68k_dreg(regs, 1), m68k_dreg(regs, 2), m68k_dreg(regs, 3));
printf("D4 = $%08X D5 = $%08X D6 = $%08X D7 = $%08X\n", m68k_dreg(regs, 4), m68k_dreg(regs, 5), m68k_dreg(regs, 6), m68k_dreg(regs, 7));
printf("\n");

uint32_t disPC = currpc - 10;
char buffer[128];

do
{
	uint32_t oldpc = disPC;
	disPC += m68k_disassemble(buffer, disPC, 0);
	printf("%s%08X: %s\n", (oldpc == currpc ? ">" : " "), oldpc, buffer);
}
while (disPC < (currpc + 10));

/*if( nr>=2 && nr<10 )  fprintf(stderr,"Exception (-> %i bombs)!\n",nr);*/

	MakeSR();

	// Change to supervisor mode if necessary
	if (!regs.s)
	{
		regs.usp = m68k_areg(regs, 7);
		m68k_areg(regs, 7) = regs.isp;
		regs.s = 1;
	}

	// Create 68000 style stack frame
	m68k_areg(regs, 7) -= 4;				// Push PC on stack
	m68k_write_memory_32(m68k_areg(regs, 7), currpc);
	m68k_areg(regs, 7) -= 2;				// Push SR on stack
	m68k_write_memory_16(m68k_areg(regs, 7), regs.sr);

//	LOG_TRACE(TRACE_CPU_EXCEPTION, "cpu exception %d currpc %x buspc %x newpc %x fault_e3 %x op_e3 %hx addr_e3 %x\n",
//	nr, currpc, BusErrorPC, get_long(4 * nr), last_fault_for_exception_3, last_op_for_exception_3, last_addr_for_exception_3);

#if 0
	/* 68000 bus/address errors: */
	if ((nr == 2 || nr == 3) && ExceptionSource == M68000_EXC_SRC_CPU)
	{
		uint16_t specialstatus = 1;

		/* Special status word emulation isn't perfect yet... :-( */
		if (regs.sr & 0x2000)
			specialstatus |= 0x4;

		m68k_areg(regs, 7) -= 8;

		if (nr == 3)     /* Address error */
		{
			specialstatus |= (last_op_for_exception_3 & (~0x1F));	/* [NP] unused bits of specialstatus are those of the last opcode ! */
			put_word(m68k_areg(regs, 7), specialstatus);
			put_long(m68k_areg(regs, 7) + 2, last_fault_for_exception_3);
			put_word(m68k_areg(regs, 7) + 6, last_op_for_exception_3);
			put_long(m68k_areg(regs, 7) + 10, last_addr_for_exception_3);

//JLH: Not now...
#if 0
			if (bExceptionDebugging)
			{
				fprintf(stderr,"Address Error at address $%x, PC=$%x\n", last_fault_for_exception_3, currpc);
				DebugUI();
			}
#endif
		}
		else     /* Bus error */
		{
			specialstatus |= (get_word(BusErrorPC) & (~0x1F));	/* [NP] unused bits of special status are those of the last opcode ! */

			if (bBusErrorReadWrite)
				specialstatus |= 0x10;

			put_word(m68k_areg(regs, 7), specialstatus);
			put_long(m68k_areg(regs, 7) + 2, BusErrorAddress);
			put_word(m68k_areg(regs, 7) + 6, get_word(BusErrorPC));	/* Opcode */

			/* [NP] PC stored in the stack frame is not necessarily pointing to the next instruction ! */
			/* FIXME : we should have a proper model for this, in the meantime we handle specific cases */
			if (get_word(BusErrorPC) == 0x21F8)			/* move.l $0.w,$24.w (Transbeauce 2 loader) */
				put_long(m68k_areg(regs, 7) + 10, currpc - 2);		/* correct PC is 2 bytes less than usual value */

			/* Check for double bus errors: */
			if (regs.spcflags & SPCFLAG_BUSERROR)
			{
				fprintf(stderr, "Detected double bus error at address $%x, PC=$%lx => CPU halted!\n",
				BusErrorAddress, (long)currpc);
				unset_special(SPCFLAG_BUSERROR);

				if (bExceptionDebugging)
					DebugUI();
				else
					DlgAlert_Notice("Detected double bus error => CPU halted!\nEmulation needs to be reset.\n");

				regs.intmask = 7;
				m68k_setstopped(true);
				return;
			}

			if (bExceptionDebugging && BusErrorAddress != 0xFF8A00)
			{
				fprintf(stderr,"Bus Error at address $%x, PC=$%lx\n", BusErrorAddress, (long)currpc);
				DebugUI();
			}
		}
	}

//Not now...
#if 0
	/* Set PC and flags */
	if (bExceptionDebugging && get_long(4 * nr) == 0)
	{
		write_log("Uninitialized exception handler #%i!\n", nr);
		DebugUI();
	}
#endif

	newpc = get_long(4 * nr);

	if (newpc & 1)				/* check new pc is odd */
	{
		if (nr == 2 || nr == 3)			/* address error during bus/address error -> stop emulation */
		{
			fprintf(stderr,"Address Error during exception 2/3, aborting new PC=$%x\n", newpc);
			DebugUI();
		}
		else
		{
			fprintf(stderr,"Address Error during exception, new PC=$%x\n", newpc);
			Exception(3, m68k_getpc(), M68000_EXC_SRC_CPU);
		}

		return;
	}
#endif

	m68k_setpc(m68k_read_memory_32(4 * nr));
	fill_prefetch_0();
	/* Handle trace flags depending on current state */
//JLH:no	exception_trace(nr);

#if 0
	/* Handle exception cycles (special case for MFP) */
//	if (ExceptionSource == M68000_EXC_SRC_INT_MFP)
//	{
//		M68000_AddCycles(44 + 12);			/* MFP interrupt, 'nr' can be in a different range depending on $fffa17 */
//	}
//	else
	if (nr >= 24 && nr <= 31)
	{
#if 0
		if (nr == 26)				/* HBL */
		{
			/* store current cycle pos when then interrupt was received (see video.c) */
			LastCycleHblException = Cycles_GetCounter(CYCLES_COUNTER_VIDEO);
			M68000_AddCycles(44 + 12);		/* Video Interrupt */
		}
		else if (nr == 28) 			/* VBL */
			M68000_AddCycles(44 + 12);		/* Video Interrupt */
		else
#endif
			M68000_AddCycles(44 + 4);			/* Other Interrupts */
	}
	else if (nr >= 32 && nr <= 47)
	{
		M68000_AddCycles(34 - 4);			/* Trap (total is 34, but cpuemu.c already adds 4) */
	}
	else switch(nr)
	{
		case 2: M68000_AddCycles(50); break;	/* Bus error */
		case 3: M68000_AddCycles(50); break;	/* Address error */
		case 4: M68000_AddCycles(34); break;	/* Illegal instruction */
		case 5: M68000_AddCycles(38); break;	/* Div by zero */
		case 6: M68000_AddCycles(40); break;	/* CHK */
		case 7: M68000_AddCycles(34); break;	/* TRAPV */
		case 8: M68000_AddCycles(34); break;	/* Privilege violation */
		case 9: M68000_AddCycles(34); break;	/* Trace */
		case 10: M68000_AddCycles(34); break;	/* Line-A - probably wrong */
		case 11: M68000_AddCycles(34); break;	/* Line-F - probably wrong */
		default:
		/* FIXME: Add right cycles value for MFP interrupts and copro exceptions ... */
		if (nr < 64)
			M68000_AddCycles(4);			/* Coprocessor and unassigned exceptions (???) */
		else
			M68000_AddCycles(44 + 12);		/* Must be a MFP or DSP interrupt */

		break;
	}
#endif
}

/*
 The routines below take dividend and divisor as parameters.
 They return 0 if division by zero, or exact number of cycles otherwise.

 The number of cycles returned assumes a register operand.
 Effective address time must be added if memory operand.

 For 68000 only (not 68010, 68012, 68020, etc).
 Probably valid for 68008 after adding the extra prefetch cycle.


 Best and worst cases are for register operand:
 (Note the difference with the documented range.)


 DIVU:

 Overflow (always): 10 cycles.
 Worst case: 136 cycles.
 Best case: 76 cycles.


 DIVS:

 Absolute overflow: 16-18 cycles.
 Signed overflow is not detected prematurely.

 Worst case: 156 cycles.
 Best case without signed overflow: 122 cycles.
 Best case with signed overflow: 120 cycles
 */

//
// DIVU
// Unsigned division
//
STATIC_INLINE int getDivu68kCycles_2 (uint32_t dividend, uint16_t divisor)
{
	int mcycles;
	uint32_t hdivisor;
	int i;

	if (divisor == 0)
		return 0;

	// Overflow
	if ((dividend >> 16) >= divisor)
		return (mcycles = 5) * 2;

	mcycles = 38;
	hdivisor = divisor << 16;

	for(i=0; i<15; i++)
	{
		uint32_t temp;
		temp = dividend;

		dividend <<= 1;

		// If carry from shift
		if ((int32_t)temp < 0)
			dividend -= hdivisor;
		else
		{
			mcycles += 2;

			if (dividend >= hdivisor)
			{
				dividend -= hdivisor;
				mcycles--;
			}
		}
	}

	return mcycles * 2;
}

// This is called by cpuemu.c
int getDivu68kCycles(uint32_t dividend, uint16_t divisor)
{
	int v = getDivu68kCycles_2(dividend, divisor) - 4;
//	write_log ("U%d ", v);
	return v;
}

//
// DIVS
// Signed division
//

STATIC_INLINE int getDivs68kCycles_2(int32_t dividend, int16_t divisor)
{
	int mcycles;
	uint32_t aquot;
	int i;

	if (divisor == 0)
		return 0;

	mcycles = 6;

	if (dividend < 0)
		mcycles++;

	// Check for absolute overflow
	if (((uint32_t)abs(dividend) >> 16) >= (uint16_t)abs(divisor))
		return (mcycles + 2) * 2;

	// Absolute quotient
	aquot = (uint32_t)abs(dividend) / (uint16_t)abs(divisor);

	mcycles += 55;

	if (divisor >= 0)
	{
		if (dividend >= 0)
			mcycles--;
		else
			mcycles++;
	}

	// Count 15 msbits in absolute of quotient

	for(i=0; i<15; i++)
	{
		if ((int16_t)aquot >= 0)
			mcycles++;

		aquot <<= 1;
	}

	return mcycles * 2;
}

// This is called by cpuemu.c
int getDivs68kCycles(int32_t dividend, int16_t divisor)
{
	int v = getDivs68kCycles_2(dividend, divisor) - 4;
//	write_log ("S%d ", v);
	return v;
}
