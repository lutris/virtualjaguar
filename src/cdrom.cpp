//
// CD handler
//
// by cal2
// GCC/SDL port by Niels Wagenaar (Linux/WIN32) and Caz (BeOS)
// Cleanups by James L. Hammons
//

#include "cdrom.h"

//#define CDROM_LOG

static uint8 cdrom_ram[0x100];
static uint16 cdrom_cmd = 0;


void cdrom_init(void)
{
}

void cdrom_reset(void)
{
	memset(cdrom_ram, 0x00, 0x100);
	cdrom_cmd = 0;
}

void cdrom_done(void)
{
}

void cdrom_byte_write(uint32 offset, uint8 data)
{
	offset &= 0xFF;
	cdrom_ram[offset] = data;

#ifdef CDROM_LOG
	fprintf(log_get(),"cdrom: writing byte 0x%.2x at 0x%.8x\n",data,offset);
#endif
}

void cdrom_word_write(uint32 offset, uint16 data)
{
	offset &= 0xFF;
	cdrom_ram[offset+0] = (data >> 8) & 0xFF;
	cdrom_ram[offset+1] = data & 0xFF;
		
	// command register
/*
	if (offset==0x0A)
	{
		cdrom_cmd=data;
		if ((data&0xff00)==0x1500)
		{
			fprintf(log_get(),"cdrom: setting mode 0x%.2x\n",data&0xff);
			return;
		}
		if (data==0x7001)
		{
			uint32 offset=cdrom_ram[0x00];
			offset<<=8;
			offset|=cdrom_ram[0x01];
			offset<<=8;
			offset|=cdrom_ram[0x02];
			offset<<=8;
			offset|=cdrom_ram[0x03];

			uint32 size=cdrom_ram[0x04];
			offset<<=8;
			offset|=cdrom_ram[0x05];
			
			fprintf(log_get(),"cdrom: READ(0x%.8x, 0x%.4x) [68k pc=0x%.8x]\n",offset,size,s68000readPC());
			return;
		}
		else
			fprintf(log_get(),"cdrom: unknown command 0x%.4x\n",data);
	}
*/
#ifdef CDROM_LOG
	fprintf(log_get(),"cdrom: writing word 0x%.4x at 0x%.8x\n",data,offset);
#endif
}

uint8 cdrom_byte_read(uint32 offset)
{
#ifdef CDROM_LOG
	fprintf(log_get(),"cdrom: reading byte from 0x%.8x\n",offset);
#endif
	return cdrom_ram[offset & 0xFF];
}

uint16 cdrom_word_read(uint32 offset)
{
	offset &= 0xFF;

	uint16 data = 0x0000;
	
	if (offset == 0x00) 
		data = 0x0000;
	else if (offset == 0x02) 
		data = 0x2000;
	else if (offset == 0x0A) 
	{
		if (cdrom_cmd == 0x7001)
			data = cdrom_cmd;
		else
			data = 0x0400;
	}
	else
		data = (cdrom_ram[offset+0] << 8) | cdrom_ram[offset+1];

#ifdef CDROM_LOG
	fprintf(log_get(),"cdrom: reading word 0x%.4x from 0x%.8x [68k pc=0x%.8x]\n",data,offset,s68000readPC());
#endif
	return data;
}
