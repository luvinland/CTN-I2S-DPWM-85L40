/******************************************************************************
 * @file     main.c
 * @version  V0.10
 * $Revision: 1 $
 * $Date: 17/07/28 10:04a $
 * @brief	This sample uses 85L40 mics as audio input and useing I2S to receive 
 *			audio data. User can process audio data before output. This sample
 *			sues 83P20 speaker and using DPWM to drive the speaker. 
 * @note
 * Copyright (C) 2017 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "Platform.h"
#include "BUFCTRL.h"
#include "ConfigSysClk.h"

/*---------------------------------------------------------------------------------------------------------*/
/* Define global constants                                                                   			   */
/*---------------------------------------------------------------------------------------------------------*/
#define SAMPLE_RATE              (48000)

/*---------------------------------------------------------------------------------------------------------*/
/* Define functions prototype                                                                              */
/*---------------------------------------------------------------------------------------------------------*/   
void SPK_Init(S_BUFCTRL* psOutBufCtrl);
void SPK_Start(void);
void SPK_Stop(void);
void MIC_Init(S_BUFCTRL* psInBufCtrl);
void MIC_Start(void);
void MIC_Stop(void);

/*---------------------------------------------------------------------------------------------------------*/
/* Define global variables and constants                                                                   */
/*---------------------------------------------------------------------------------------------------------*/
volatile S_BUFCTRL sInBufCtrl,sOutBufCtrl;// Buffer control handler.
int32_t    ai32InBuf[256];  	  // Buffer array: store audio data receiced from I2S(85L40)
int32_t    ai32OutBuf[128]; 	  // Buffer array: store audio data ready to send to DPWM

/*---------------------------------------------------------------------------------------------------------*/
/*  Main Function                                                                                          */
/*---------------------------------------------------------------------------------------------------------*/
int32_t main(void) 
{
	int32_t i32Data1, i32Data2;
	
	// Initiate system clock(Configure in ConfigSysClk.h)
	SYSCLK_INITIATE();
		
	// These defines are from  BUFCTRL.h for buffer control in this samle. 
	// Buffer control handler configuration. 
	BUFCTRL_CFG((&sInBufCtrl),ai32InBuf,sizeof(ai32InBuf)/sizeof(uint32_t));
	BUFCTRL_CFG((&sOutBufCtrl),ai32OutBuf,sizeof(ai32OutBuf)/sizeof(uint32_t));
	// full empty data into output buffer.
	sOutBufCtrl.u16DataCount = sOutBufCtrl.u16BufCount;  
	
	// Initiate speaker.
	SPK_Init((S_BUFCTRL*)&sOutBufCtrl);
	// Initiate microphone.
	MIC_Init((S_BUFCTRL*)&sInBufCtrl);

	// Start microphone.
	MIC_Start();
	// Start speaker.
	SPK_Start();
	
	// while loop for processing.
	while(1) 
	{
		while( BUFCTRL_GET_COUNT((&sInBufCtrl))>= 2 && !BUFCTRL_IS_FULL((&sOutBufCtrl)) )
		{
			/* 4 channel mixer to 2 channel. */
			BUFCTRL_READ((&sInBufCtrl),&i32Data1);
			BUFCTRL_READ((&sInBufCtrl),&i32Data2);
			
			i32Data1 >>= 1;
            i32Data2 >>= 1;
			i32Data1 += i32Data2;
			
			BUFCTRL_WRITE((&sOutBufCtrl),i32Data1);
		}
	};
}

// Speaker(DPWM) = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
S_BUFCTRL* psSPK_BufCtrl = NULL;            // Provide Speaker to output data.

void SPK_Init(S_BUFCTRL* psOutBufCtrl)
{
	// (1) Config DPWM to be a speaker output.
	{
		// Enable DPWM clock. 
		CLK_EnableModuleClock(DPWM_MODULE);
		// Select DPWM CLK source HIRC. 
		CLK_SetModuleClock(DPWM_MODULE, CLK_CLKSEL2_DPWMSEL_HIRC, MODULE_NoMsk);

		// DPWM IPReset. 
		SYS_ResetModule(DPWM_RST);
		// HIRC=49.152MHz,Fs=24.576MHz/(128x4)=48kHz. 
		DPWM_SET_CLKSET(DPWM, DPWM_CLKSET_512FS);
		DPWM_SetSampleRate(SAMPLE_RATE);
		
		// Enable threshold int and Data width 24Bits. 
		DPWM_SET_FIFODATAWIDTH(DPWM, DPWM_FIFO_DATAWIDTH_MSB24BITS);
		
		DPWM_ENABLE_FIFOTHRESHOLDINT(DPWM,8);
		// Enable NVIC.
		NVIC_EnableIRQ(DPWM_IRQn);
		// GPIO multi-function.
		SYS->GPC_MFPH = (SYS->GPC_MFPH & ~(SYS_GPC_MFPH_PC13MFP_Msk|SYS_GPC_MFPH_PC12MFP_Msk|SYS_GPC_MFPH_PC11MFP_Msk|SYS_GPC_MFPH_PC10MFP_Msk))|(SYS_GPC_MFPH_PC13MFP_DPWM_LP|SYS_GPC_MFPH_PC12MFP_DPWM_LN|SYS_GPC_MFPH_PC11MFP_DPWM_RP|SYS_GPC_MFPH_PC10MFP_DPWM_RN);	

	}
	// (2) Config DPWM(Speaker) buffer control 
	{
		psSPK_BufCtrl = psOutBufCtrl;
	}
}
void SPK_Start(void)
{
	if( psSPK_BufCtrl != NULL )
	{
		DPWM_ENABLE_DRIVER(DPWM);
		DPWM_START_PLAY(DPWM);
	}
}
void SPK_Stop(void)
{
	DPWM_STOP_PLAY(DPWM);
	DPWM_DISABLE_DRIVER(DPWM);	
}
void DPWM_IRQHandler(void) 
{
	uint32_t u32Tmp, i;
	
	if( BUFCTRL_IS_EMPTY(psSPK_BufCtrl) ) 
	{
		if( DPWM_IS_FIFOEMPTY(DPWM) )
		{
			for( i=0; i<8; i++ )
				DPWM_WRITE_INDATA(DPWM,0);
		}
	} 
	else 
	{
		for(i=0; i<4; i++)
		{
			if(!DPWM_IS_FIFOFULL(DPWM)) 
			{
				if( !BUFCTRL_IS_EMPTY(psSPK_BufCtrl) )
				{
					BUFCTRL_READ(psSPK_BufCtrl,&u32Tmp);
					DPWM_WRITE_INDATA(DPWM,u32Tmp);
				}
			}
			else
				break;
		}
	}
}

// Microphone(85L40) = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
typedef struct {
	uint8_t  u8DeviceAddr;
	uint16_t u16Counter;
	uint16_t u16MaxCount;
	uint8_t* pau8Cmd;
} S_MIC_I2CCTRL;

typedef struct {
	uint8_t  u8Reg[2];
	uint8_t  u8Value[2];
} S_MIC_I2CCMD;

S_BUFCTRL* psMIC_BufCtrl = NULL;           // Provide microphone input buffer control.
volatile S_MIC_I2CCTRL s_MIC_I2CCtrl;      // Provide microphone send command to 85L40S_BUFCTRL* psSPK_BufCtrl = NULL;
// Command for 85L40(transfer via I2C1)
S_MIC_I2CCMD const asMIC_Cmd_85L40[] = {
//-------
{	0x00	,	0x00	,	0x00	,	0x01	}	,
//-------
{	0x00	,	0x03	,	0x00	,	0x40	}	,
{	0x00	,	0x04	,	0x00	,	0x01	}	,
{	0x00	,	0x05	,	0x31	,	0x26	}	,
{	0x00	,	0x06	,	0x00	,	0x08	}	,
{	0x00	,	0x07	,	0x00	,	0x10	}	,
{	0x00	,	0x08	,	0xC0	,	0x00	}	,
{	0x00	,	0x09	,	0xE0	,	0x00	}	,
{	0x00	,	0x0A	,	0xF1	,	0x3C	}	,
{	0x00	,	0x10	,	0x00	,	0x4F	}	,  //PCMB, 32Bit
{	0x00	,	0x11	,	0x00	,	0x00	}	,
{	0x00	,	0x12	,	0x00	,	0x00	}	,
{	0x00	,	0x13	,	0x00	,	0x00	}	,
{	0x00	,	0x14	,	0xC0	,	0x0F	}	,
{	0x00	,	0x20	,	0x00	,	0x00	}	,
{	0x00	,	0x21	,	0x70	,	0x0B	}	,
{	0x00	,	0x22	,	0x00	,	0x22	}	,
{	0x00	,	0x23	,	0x10	,	0x10	}	,
{	0x00	,	0x24	,	0x10	,	0x10	}	,
{	0x00	,	0x2D	,	0x10	,	0x10	}	,
{	0x00	,	0x2E	,	0x10	,	0x10	}	,
{	0x00	,	0x2F	,	0x00	,	0x00	}	,
{	0x00	,	0x30	,	0x00	,	0x00	}	,
{	0x00	,	0x31	,	0x00	,	0x00	}	,
{	0x00	,	0x32	,	0x00	,	0x00	}	,
{	0x00	,	0x33	,	0x00	,	0x00	}	,
{	0x00	,	0x34	,	0x00	,	0x00	}	,
{	0x00	,	0x35	,	0x00	,	0x00	}	,
{	0x00	,	0x36	,	0x00	,	0x00	}	,
{	0x00	,	0x37	,	0x00	,	0x00	}	,
{	0x00	,	0x38	,	0x00	,	0x00	}	,
{	0x00	,	0x39	,	0x00	,	0x00	}	,
{	0x00	,	0x3A	,	0x40	,	0x02	}	,
{	0x00	,	0x40	,	0x04	,	0x00	}	,  //DGAIN = 0dB
{	0x00	,	0x41	,	0x04	,	0x00	}	,  //DGAIN = 0dB
{	0x00	,	0x42	,	0x04	,	0x00	}	,  //DGAIN = 0dB
{	0x00	,	0x43	,	0x04	,	0x00	}	,  //DGAIN = 0dB
{	0x00	,	0x44	,	0x00	,	0xE4	}	,
{	0x00	,	0x48	,	0x00	,	0x00	}	,
{	0x00	,	0x49	,	0x00	,	0x00	}	,
{	0x00	,	0x4A	,	0x00	,	0x00	}	,
{	0x00	,	0x4B	,	0x00	,	0x00	}	,
{	0x00	,	0x4C	,	0x00	,	0x00	}	,
{	0x00	,	0x4D	,	0x00	,	0x00	}	,
{	0x00	,	0x4E	,	0x00	,	0x00	}	,
{	0x00	,	0x4F	,	0x00	,	0x00	}	,
{	0x00	,	0x50	,	0x00	,	0x00	}	,
{	0x00	,	0x51	,	0x00	,	0x00	}	,
{	0x00	,	0x52	,	0xEF	,	0xFF	}	,
{	0x00	,	0x57	,	0x00	,	0x00	}	,
{	0x00	,	0x58	,	0x1C	,	0xF0	}	,
{	0x00	,	0x59	,	0x00	,	0x08	}	,
{	0x00	,	0x60	,	0x00	,	0x60	}	,
{	0x00	,	0x61	,	0x00	,	0x00	}	,
{	0x00	,	0x62	,	0x00	,	0x00	}	,
{	0x00	,	0x63	,	0x00	,	0x00	}	,
{	0x00	,	0x64	,	0x00	,	0x11	}	,
{	0x00	,	0x65	,	0x02	,	0x20	}	,
{	0x00	,	0x66	,	0x00	,	0x0F	}	,
{	0x00	,	0x67	,	0x0D	,	0x04	}	,
{	0x00	,	0x68	,	0x70	,	0x00	}	,
{	0x00	,	0x69	,	0x00	,	0x00	}	,
{	0x00	,	0x6A	,	0x00	,	0x00	}	,
{	0x00	,	0x6B	,	0x1B	,	0x1B	}	,  //AGAIN = 26dB
{	0x00	,	0x6C	,	0x1B	,	0x1B	}	,  //AGAIN = 26dB
{	0x00	,	0x6D	,	0xF0	,	0x00	}	,
{	0x00	,	0x01	,	0x00	,	0x0F	}	,
{	0x00	,	0x02	,	0x80	,	0x03	}	
};

void MIC_Init(S_BUFCTRL* psInBufCtrl)
{
	// (1) Config I2C1 for sending command to 85L40
	// (1-1) Initiate I2C1
	{
		// Reset module. 
		SYS_ResetModule(I2C1_RST);
		// Enable I2C0 module clock. 
		CLK_EnableModuleClock(I2C1_MODULE);
		// Open I2C module and set bus clock. 
		I2C_Open(I2C1, 100000);
		// Enable I2C interrupt. 
		I2C_EnableInt(I2C1);
		NVIC_EnableIRQ(I2C1_IRQn);	
		// GPIO multi-function.(GPD14:I2C1_SCL,GPD15:I2C1_SDA). 
		SYS->GPD_MFPH = (SYS->GPD_MFPH & ~(SYS_GPD_MFPH_PD14MFP_Msk|SYS_GPD_MFPH_PD15MFP_Msk))|(SYS_GPD_MFPH_PD14MFP_I2C1_SCL|SYS_GPD_MFPH_PD15MFP_I2C1_SDA);
	}		
	// (1-2) Send command to 85L40 via I2C1
	{
		uint16_t u16i;
		I2C_SetBusClockFreq(I2C1,100000);
		s_MIC_I2CCtrl.u8DeviceAddr = 0x1C;
		for(u16i=0;u16i<sizeof(asMIC_Cmd_85L40)/sizeof(S_MIC_I2CCMD);u16i++) 
		{
			s_MIC_I2CCtrl.pau8Cmd = (uint8_t*)&asMIC_Cmd_85L40[u16i];
			s_MIC_I2CCtrl.u16Counter = 0;
			s_MIC_I2CCtrl.u16MaxCount = sizeof(S_MIC_I2CCMD);
			I2C_START(I2C1);
			/* Wait for I2C transmit completed. */
			while(s_MIC_I2CCtrl.u16MaxCount>0);
		}		
	}
	
	// (2) Config I2S to ge voice data from 85L40
	{
		//---
		uint32_t u32BitRate, u32Divider;
		
		/* GPIO multi-function.(GPA7:SPI2I2S_DI,GPA8:SPI2I2S_DO). */
		SYS->GPD_MFPL = (SYS->GPD_MFPL & ~(SYS_GPD_MFPL_PD2MFP_Msk|SYS_GPD_MFPL_PD3MFP_Msk|SYS_GPD_MFPL_PD4MFP_Msk|SYS_GPD_MFPL_PD5MFP_Msk|SYS_GPD_MFPL_PD6MFP_Msk)) | (SYS_GPD_MFPL_PD2MFP_I2S0_MCLK|SYS_GPD_MFPL_PD3MFP_I2S0_LRCK|SYS_GPD_MFPL_PD4MFP_I2S0_DI|SYS_GPD_MFPL_PD5MFP_I2S0_DO|SYS_GPD_MFPL_PD6MFP_I2S0_BCLK);
		
		/* Enable I2S clock. */
		CLK_EnableModuleClock(I2S0_MODULE);

		/* Select I2S clock. */
		CLK_SetModuleClock(I2S0_MODULE, CLK_CLKSEL3_I2S0SEL_HIRC, NULL);
		
		/* I2S IPReset. */
		SYS_ResetModule(I2S0_RST);
		
		/* Open I2S and enable master clock. */
		I2S_Open(I2S0, I2S_MASTER, SAMPLE_RATE, I2S_DATABIT_24, I2S_TDMCHNUM_4CH, I2S_STEREO, I2S_FORMAT_PCMMSB);
		I2S_EnableMCLK(I2S0, SAMPLE_RATE * 256);
		
		/* I2S Configuration. */
		I2S_SET_PCMSYNC(I2S0, I2S_PCMSYNC_BCLK);
		I2S_SET_MONO_RX_CHANNEL(I2S0, I2S_MONO_RX_RIGHT);
		I2S_SET_STEREOORDER(I2S0, I2S_ORDER_EVENLOW);
		
		/* Set channel width. */
		I2S_SET_CHWIDTH(I2S0, I2S_CHWIDTH_32);
		/* Set FIFO threshold. */
		I2S_SET_TXTH(I2S0, I2S_FIFO_TX_LEVEL_WORD_8);
		I2S_SET_RXTH(I2S0, I2S_FIFO_RX_LEVEL_WORD_9);
		/* Enable interrupt. */
		I2S_ENABLE_INT(I2S0, I2S_TXTH_INT_MASK|I2S_RXTH_INT_MASK|I2S_TXOV_INT_MASK|I2S_RXOV_INT_MASK|I2S_TXUF_INT_MASK|I2S_RXUF_INT_MASK);
		/* Clear TX, RX FIFO buffer */
		I2S_CLR_TX_FIFO(I2S0);
		I2S_CLR_RX_FIFO(I2S0);
		/* Enable I2S's NVIC. */
		NVIC_EnableIRQ(I2S0_IRQn);
	}
	// (3) Config I2S0(for 85L40) buffer control
	{
		psMIC_BufCtrl = psInBufCtrl;
	}
}
void MIC_Start(void)
{
	if( psMIC_BufCtrl != NULL )
	{
		I2S_ENABLE(I2S0);
		I2S_ENABLE_RX(I2S0);	
	}
}
void MIC_Stop(void)
{
	I2S_DISABLE_RX(I2S0);
	I2S_DISABLE(I2S0);
}
void I2S0_IRQHandler() 
{
	uint32_t u32Tmp, i; 
	
	for(i=0; i<8; i++)
    {
		if(!I2S_GET_RX_IS_EMPTY(I2S0))
		{
			// Read the data from I2S RXFIFO, but not write to bufer. 
			u32Tmp = I2S_READ_RX_FIFO(I2S0);
			
			// Write the data from I2S RXFIFO to buffer.
			if(!BUFCTRL_IS_FULL(psMIC_BufCtrl) && psSPK_BufCtrl != NULL)
				BUFCTRL_WRITE(psMIC_BufCtrl,u32Tmp);
		}
		else
			break;
	}
	
	I2S_CLR_INT_FLAG(I2S0, I2S_RXUD_INT_FLAG);
}
void I2C1_IRQHandler() 
{
    if(I2C_GET_TIMEOUT_FLAG(I2C1)) 
        I2C_ClearTimeoutFlag(I2C1); 
	else 
	{
		switch(I2C_GET_STATUS(I2C1)) {
			/* START has been transmitted and Write SLA+W to Register I2CDAT. */
			case 0x08:
				I2C_SET_DATA(I2C1, s_MIC_I2CCtrl.u8DeviceAddr << 1);    
				I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI);			
				break;
			/* SLA+W has been transmitted and ACK has been received. */
			case 0x18:
				I2C_SET_DATA(I2C1, s_MIC_I2CCtrl.pau8Cmd[s_MIC_I2CCtrl.u16Counter++]);
				I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI);		
				break;
			/* SLA+W has been transmitted and NACK has been received. */
			case 0x20:
				I2C_STOP(I2C1);
				I2C_START(I2C1);	
				s_MIC_I2CCtrl.u16MaxCount = 0;
				break;
			/* DATA has been transmitted and ACK has been received. */
			case 0x28:
				if(s_MIC_I2CCtrl.u16Counter < s_MIC_I2CCtrl.u16MaxCount) {
					I2C_SET_DATA(I2C1, s_MIC_I2CCtrl.pau8Cmd[s_MIC_I2CCtrl.u16Counter++]);
					I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI);
				} else {
					I2C_SET_CONTROL_REG(I2C1, I2C_CTL_STO_SI);
					s_MIC_I2CCtrl.u16MaxCount = 0;
				}
				break;
		}
	}
}
/*** (C) COPYRIGHT 2017 Nuvoton Technology Corp. ***/
