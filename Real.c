#include <stdio.h>
#include <cdefBF706.h>
#include <defBF706.h>
#include "stdfix.h"
#include <sys/platform.h>
#include "adi_initialize.h"
#include <services/int/adi_int.h>

#define BUFFER_SIZE 2                                     // Size of buffer to transmit
#define FILTER_LENGTH 50
long fract XIN[BUFFER_SIZE];                              // Input buffer
long fract YOUT[BUFFER_SIZE];                             // Output buffer
long fract h[FILTER_LENGTH]= {0.0};
long fract x[FILTER_LENGTH] = {0.0};
long fract y=0;
long fract e;                                             // Error signal
int LEARNING_RATE = 600;                                           // Delta fraction. This is large due to 24-bit
int k=0;
int n=0;

void TWI_write(uint16_t, uint8_t);
void codec_configure(void);
void sport_configure(void);
void init_SPORT_DMA(void);
void SPORT0_RX_interrupt_handler(uint32_t iid, void *handlerArg);

// Subroutine DMA_init initialises the SPORT0 DMA0 and DMA1 in auto-buffer mode, p19¨C39 and p19¨C49, BHRM.

void init_SPORT_DMA()
{
 *pREG_DMA0_ADDRSTART = YOUT;                             // points to start of SPORT0_A buffer
 *pREG_DMA0_XCNT = BUFFER_SIZE;                           // no. of words to transmit
 *pREG_DMA0_XMOD = 4;                                     // Word length, increment to find next word in the buffer
 *pREG_DMA1_ADDRSTART = XIN;                              // points to start of SPORT0_B buffer
 *pREG_DMA1_XCNT = BUFFER_SIZE;                           // no. of words to receive
 *pREG_DMA1_XMOD = 4;                                     // Word length, increment to find the next word in buffer
 *pREG_DMA0_CFG = 0x00001221;                             // SPORT0 TX, FLOW = autobuffer, MSIZE = PSIZE = 4 bytes
 *pREG_DMA1_CFG = 0x00101223;                             // SPORT0 RX, DMA interrupt when x count expires
}

// This is the LMS FIR adaptive filter routine. The first stage is the FIR filter;
// The second stage is the coefficient update routine
// XIN[1] = noise
// XIN[0] = signal + noise

void SPORT0_RX_interrupt_handler(uint32_t iid, void *handlerArg)
{
  *pREG_DMA1_STAT = 0x1;                                  // Clear interrupt
  y=0;
  x[n] = XIN[1];

      for (int k = 0; k < FILTER_LENGTH-1; k++)
      {
    	  if (n>=FILTER_LENGTH) {
    	      n=0; } //circular index
          y+=h[k]*x[n];
          n++;
          }

      //e[n] = d[n] - y[n]
       e = XIN[0] - y;

      //update h
      int j=n;
      for (int k = 0; k < FILTER_LENGTH-1; k++)
      {
    	  if (j>=FILTER_LENGTH) {
    	      j=0;}
          h[k]+=LEARNING_RATE*e*x[j];
          j++;
          }

      //support circular buffer
      n--;
      if(n<0){
     	 n=FILTER_LENGTH-1;
       }
  YOUT[0]=e;//left channel                                             // Channel 0, pure signal
  //YOUT[1]=y;  //right channel
  //the output from the board can not be separated totally!crosstalk
}

// Function sport_configure initialises the SPORT0. Refer to pages 26-59, 26-67,
// 26-75 and 26-76 of the ADSP-BF70x Blackfin+ Processor Hardware Reference manual.

void sport_configure()
{
 *pREG_PORTC_FER=0x0003F0;                                // Set up Port C in peripheral mode
 *pREG_PORTC_FER_SET=0x0003F0;                            // Set up Port C in peripheral mode
 *pREG_SPORT0_CTL_A=0x2001973;                            // Set up SPORT0 (A) as TX to codec, 24 bits
 *pREG_SPORT0_DIV_A=0x400001;                             // 64 bits per frame, clock divisor of 1
 *pREG_SPORT0_CTL_B=0x0001973;                            // Set up SPORT0 (B) as RX from codec, 24 bits
 *pREG_SPORT0_DIV_B=0x400001;                             // 64 bits per frame, clock divisor of 1
}

// Function TWI_write is a simple driver for the TWI. Refer to page 24-15 onwards
// of the ADSP-BF70x Blackfin+ Processor Hardware Reference manual.

void TWI_write(uint16_t reg_add, uint8_t reg_data)
{
  int n;
  reg_add=(reg_add<<8)|(reg_add>>8);                      // Reverse low order and high order bytes
  *pREG_TWI0_CLKDIV=0x3232;                               // Set duty cycle
  *pREG_TWI0_CTL=0x8c;                                    // Set prescale and enable TWI
  *pREG_TWI0_MSTRADDR=0x38;                               // Address of codec
  *pREG_TWI0_TXDATA16=reg_add;                            // Address of register to set, LSB then MSB
  *pREG_TWI0_MSTRCTL=0xc1;                                // Command to send three bytes and enable transmit
  for(n=0;n<8000;n++){}                                   // Delay since codec must respond
  *pREG_TWI0_TXDATA8=reg_data;                            // Data to write
  for(n=0;n<10000;n++){}                                  // Delay
  *pREG_TWI0_ISTAT=0x050;                                 // Clear TXERV interrupt
  for(n=0;n<10000;n++){}                                  // Delay
  *pREG_TWI0_ISTAT=0x010;                                 // Clear MCOMP interrupt
}

// Function codec_configure initialises the ADAU1761 codec. Refer to the control register
// descriptions, page 51 onwards of the ADAU1761 data sheet.

void codec_configure()
{
  TWI_write(0x4000, 0x01);                                // Enable master clock, disable PLL
  TWI_write(0x40F9, 0x7f);                                // Enable all clocks
  TWI_write(0x40Fa, 0x03);                                // Enable all clocks
  TWI_write(0x4015, 0x01);                                // Set serial port master mode
  TWI_write(0x4019, 0x13);                                // Set ADC to on, both channels
  TWI_write(0x401c, 0x21);                                // Enable left channel mixer
  TWI_write(0x401e, 0x41);                                // Enable right channel mixer
  TWI_write(0x4029, 0x03);                                // Turn on power, both channels
  TWI_write(0x402A, 0x03);                                // Set both DACs on
  TWI_write(0x40f2, 0x01);                                // DAC gets L, R input from serial port
  TWI_write(0x40f3, 0x01);                                // ADC sends L, R input to serial port
  TWI_write(0x400a, 0x0b);                                // Set left line-in gain to 0 dB
  TWI_write(0x400c, 0x0b);                                // Set right line-in gain to 0 dB
  TWI_write(0x4023, 0xe7);	                               // Set left headphone volume to 0 dB
  TWI_write(0x4024, 0xe7);                                // Set right headphone volume to 0 dB
  TWI_write(0x4017, 0x00);                                // Set codec default sample rate, 48 kHz
}

int main(void)
{
 bool my_audio = true;
 codec_configure();                                       // Enable codec, sport and DMA
 sport_configure();
 init_SPORT_DMA();
 adi_int_InstallHandler(INTR_SPORT0_B_DMA, SPORT0_RX_interrupt_handler, 0, true);
 *pREG_SEC0_GCTL  = 1;                                    // Enable the System Event Controller (SEC)
 *pREG_SEC0_CCTL0 = 1;                                    // Enable SEC Core Interface (SCI)
 while(my_audio){};
}

