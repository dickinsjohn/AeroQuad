/*
  Copyright (c) 2012 kha.  All rights reserved.

  STM32 PPM receiver by kha based on
  STM32 receiver class by ala42 using time input capture
  for use with AeroQuad software and Maple library
  V 1.0 Jun 14 2012

  Define the pin numbers used for the receiver in receiverPinPPM

  Timer and timer channels are accessed using the Maple PIN_MAP array.
  Make sure libmaple and this receiver class are compiled using the
  same structure alignment mode. When in doubt, change the stm32_pin_info
  declaration in wirish_types.h to align the size to a multiple of 4 byte
  by adding a filler byte at the end of the structure declaration.
*/

#ifndef _AEROQUAD_RECEIVER_STM32PPM_H_
#define _AEROQUAD_RECEIVER_STM32PPM_H_

#if defined(AeroQuadSTM32)

#include "Receiver.h"
#include "wirish.h"
#include "Receiver_PPM_common.h"

//#define STM32_TIMER_DEBUG // enable debug messages

static byte ReceiverChannelMap[PPM_CHANNELS] = {SERIAL_SUM_PPM};

uint16 rawChannelValue[PPM_CHANNELS] =  {1500,1500,1500,1500,1500,1500,1500,1500,1500,1500};
byte   currentChannel;


///////////////////////////////////////////////////////////////////////////////
// implementation part starts here.

typedef struct {
  timer_dev   *TimerDev;
  timer_gen_reg_map *TimerRegs;
  __io uint32	*Timer_ccr;
  uint16		RiseTime;
  int			TimerChannel;
  int			PolarityMask;
} tFrqData;

volatile tFrqData FrqData;

void FrqInit(int aDefault, timer_dev *aTimer, int aTimerChannel)
{
  aTimerChannel--;  // transform timer channel numbering from 1-4 to 0-3

  FrqData.TimerDev     = aTimer;
  timer_gen_reg_map *timer = aTimer->regs.gen;
  FrqData.TimerRegs    = timer;

  FrqData.Timer_ccr    = &timer->CCR1 + aTimerChannel;
  FrqData.TimerChannel = aTimerChannel;

  int TimerEnable = (1 << (4*aTimerChannel));
  FrqData.PolarityMask = TimerEnable << 1;

  uint32 clock_speed = rcc_dev_timer_clk_speed(FrqData.TimerDev->clk_id);
  timer->PSC	= (clock_speed/1000000)-1;
  timer->ARR	= 0xffff;
  timer->CR1	= 0;
  timer->DIER &= ~(1);

  timer->CCER &= ~TimerEnable; // Disable timer
  timer->CCER &= ~(FrqData.PolarityMask);

#ifdef STM32_TIMER_DEBUG
  Serial.print("  clk ");
  Serial.print(clock_speed/1000000, 10);
  Serial.print("MHz ");

  Serial.print(" CCMR0 ");
  Serial.print(timer->CCMR1, 16);
#endif

  volatile uint32 *mr;
  if(aTimerChannel < 2) {
    mr = &(timer->CCMR1);
  } else {
    mr = &(timer->CCMR2);
  }
  *mr &= ~(0xFF << (8*(aTimerChannel&1)));	// prescaler 1
  *mr |= 0x61 << (8*(aTimerChannel&1));		// 0x61 -> 6=filter, 1=inputs 1,2,3,4

  timer->CCER |= TimerEnable; // Enable
  timer->CR1 = 1;

#ifdef STM32_TIMER_DEBUG
  Serial.print(" CCER ");
  Serial.print(timer->CCER, 16);
  Serial.print(" CCMR1 ");
  Serial.print(timer->CCMR1, 16);
  Serial.println();
#endif
}

void FrqChange()
{
  timer_gen_reg_map *timer = FrqData.TimerRegs;
  uint16_t c = *(FrqData.Timer_ccr);
  bool rising = (timer->CCER & FrqData.PolarityMask) == 0;

  if(rising) {
    uint16_t diffTime = c - FrqData.RiseTime;
    if ((diffTime > 900) && (diffTime < 2100)) {
      if (currentChannel < PPM_CHANNELS) {
        rawChannelValue[currentChannel] = diffTime;
        currentChannel++;
      }
    } else if (diffTime > 2500) {
      currentChannel = 0;
    } else {
      // glitch; stop and wait next round
      currentChannel = PPM_CHANNELS;
    }
    FrqData.RiseTime = c;
  }
//  FrqData.TimerRegs->CCER ^= FrqData.PolarityMask; // invert polarity
}

void InitFrqMeasurement()
{

#ifdef STM32_TIMER_DEBUG
  Serial.println("InitFrqMeasurement");
#endif
  int pin = receiverPinPPM;
  timer_dev *timer_num = PIN_MAP[pin].timer_device;

  currentChannel=8;
  if(timer_num == NULL) {
#ifdef STM32_TIMER_DEBUG
    Serial.print("InitFrqMeasurement: invalid PWM input ");
    Serial.print(pin);
    Serial.println();
#endif
  } else {
#ifdef STM32F2
    gpio_set_mode(PIN_MAP[pin].gpio_device, PIN_MAP[pin].gpio_bit, GPIO_AF_INPUT_PD);
#else
    pinMode(pin, INPUT_PULLDOWN);
#endif

#ifdef STM32_TIMER_DEBUG
    timer_gen_reg_map *timer = PIN_MAP[pin].timer_device->regs.gen;
    Serial.print("pin ");
    Serial.print(pin);
    Serial.print(" timerbase ");
    Serial.print((int32)timer,16);
    Serial.println();
#endif
    FrqInit(1500, timer_num, PIN_MAP[pin].timer_channel);

    timer_attach_interrupt(timer_num, PIN_MAP[pin].timer_channel, FrqChange);
  }

#ifdef STM32_TIMER_DEBUG
  Serial.println("InitFrqMeasurement done");
#endif
}



///////////////////////////////////////////////////////////////////////////////
// interface part starts here

void initializeReceiver(int nbChannel = 8) {
  initializeReceiverParam(nbChannel);
  InitFrqMeasurement();
}


int getRawChannelValue(const byte channel) {
  return rawChannelValue[ReceiverChannelMap[channel]];
}


void setChannelValue(byte channel,int value) {
}

#endif

#endif
