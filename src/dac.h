//
// DAC.H: Header file
//

#ifndef __DAC_H__
#define __DAC_H__

//this is here, because we have to compensate in more than just dac.cpp...
#define NEW_DAC_CODE							// New code paths!

//#include "types.h"
#include "memory.h"

void DACInit(void);
void DACReset(void);
void DACDone(void);
int GetCalculatedFrequency(void);
void DACSetNewFrequency(int);

// DAC memory access

void DACWriteByte(uint32 offset, uint8 data, uint32 who = UNKNOWN);
void DACWriteWord(uint32 offset, uint16 data, uint32 who = UNKNOWN);
uint8 DACReadByte(uint32 offset, uint32 who = UNKNOWN);
uint16 DACReadWord(uint32 offset, uint32 who = UNKNOWN);

// Global variables

//extern uint16 lrxd, rrxd;							// I2S ports (into Jaguar)

#endif	// __DAC_H__
