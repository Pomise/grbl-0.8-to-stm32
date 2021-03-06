/*
  serial.c - Low level functions for sending and recieving bytes via the serial port
  Part of Grbl

  The MIT License (MIT)

  GRBL(tm) - Embedded CNC g-code interpreter and motion-controller
  Copyright (c) 2009-2011 Simen Svale Skogsrud
  Copyright (c) 2011-2012 Sungeun K. Jeon

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

//#include <avr/interrupt.h>
//#include "serial.h"
//#include "config.h"
//#include "motion_control.h"
//#include "protocol.h"

#include "include.h"

uint8_t rx_buffer[RX_BUFFER_SIZE];
uint8_t rx_buffer_head = 0;
volatile uint8_t rx_buffer_tail = 0;

uint8_t tx_buffer[TX_BUFFER_SIZE];
uint8_t tx_buffer_head = 0;
volatile uint8_t tx_buffer_tail = 0;

#ifdef ENABLE_XONXOFF
  volatile uint8_t flow_ctrl = XON_SENT; // Flow control state variable
  
  // Returns the number of bytes in the RX buffer. This replaces a typical byte counter to prevent
  // the interrupt and main programs from writing to the counter at the same time.
	/*返回RX缓冲区中的字节数。这将替换典型的字节计数器来防止
中断和主程序同时写入到计数器。*/

static uint8_t get_rx_buffer_count()
  {
    if (rx_buffer_head == rx_buffer_tail) { return(0); }
    if (rx_buffer_head < rx_buffer_tail) { return(rx_buffer_tail-rx_buffer_head); }
    return (RX_BUFFER_SIZE - (rx_buffer_head-rx_buffer_tail));
  }
#endif

void serial_init()
{
//==========================================
//  // Set baud rate
//  #if BAUD_RATE < 57600
//    uint16_t UBRR0_value = ((F_CPU / (8L * BAUD_RATE)) - 1)/2 ;
//    UCSR0A &= ~(1 << U2X0); // baud doubler off  - Only needed on Uno XXX
//  #else
//    uint16_t UBRR0_value = ((F_CPU / (4L * BAUD_RATE)) - 1)/2;
//    UCSR0A |= (1 << U2X0);  // baud doubler on for high baud rates, i.e. 115200
//  #endif
//  UBRR0H = UBRR0_value >> 8;					 //设置波特率
//  UBRR0L = UBRR0_value;
//            
//  // enable rx and tx
//  UCSR0B |= 1<<RXEN0;							 //使能IO作为收发
//  UCSR0B |= 1<<TXEN0;
//	
//  // enable interrupt on complete reception of a byte
//  UCSR0B |= 1<<RXCIE0;						 //接收结束中断使能

/*--------已在串口初始化函数中定义---------*/
//==========================================
	  
  // defaults to 8-bit, no parity, 1 stop bit
}

void serial_write(uint8_t data) {
  // Calculate next head 计算下一个开始处
  uint8_t next_head = tx_buffer_head + 1;
  if (next_head == TX_BUFFER_SIZE) { next_head = 0; }

	/******************************************可以会陷入无限循环************************************/
  // Wait until there is space in the buffer等到缓冲区中有空格
  while (next_head == tx_buffer_tail) { 
    if (sys.execute & EXEC_RESET) { return; } // Only check for abort to avoid an endless loop.
																						  //只检查中止以避免无休止的循环。
  }

  // Store data and advance head存储数据和将head值前进
  tx_buffer[tx_buffer_head] = data;
  tx_buffer_head = next_head;
 //========================================== 
  // Enable Data Register Empty Interrupt to make sure tx-streaming is running
//  UCSR0B |=  (1 << UDRIE0);		   //使能串口发送完成中断后会立刻进入发送完成中断!!!!!
	USART_ITConfig(USART1,USART_IT_TXE, ENABLE);//开启发送中断并立刻进入发送中断
  //========================================== 
}



uint8_t serial_read()
{
  uint8_t tail = rx_buffer_tail; // Temporary(暂时） rx_buffer_tail (to optimize for volatile为挥发性优化)
  uint8_t data ;
  if (rx_buffer_head == tail) {
    return SERIAL_NO_DATA;
  } else {
    data = rx_buffer[tail];
    tail++;
    if (tail == RX_BUFFER_SIZE) { tail = 0; }
    rx_buffer_tail = tail;
    
    #ifdef ENABLE_XONXOFF
      if ((get_rx_buffer_count() < RX_BUFFER_LOW) && flow_ctrl == XOFF_SENT) { 
        flow_ctrl = SEND_XON;
        UCSR0B |=  (1 << UDRIE0); // Force TX
      }
    #endif
    
    return data;
  }
}


void serial_reset_read_buffer() 
{
  rx_buffer_tail = rx_buffer_head;

  #ifdef ENABLE_XONXOFF
    flow_ctrl = XON_SENT;
  #endif
}


//==========================================
void USART1_IRQHandler(void)
{
	if(USART_GetFlagStatus(USART1 , USART_IT_RXNE)!=RESET)//接收到数据
	{
		//-----------------【接收中断】
		//==========================================
		uint8_t data=USART1->DR;				   //接收
		uint8_t next_head;
		//==========================================
//ISR(SERIAL_RX)
//{
//  
//  	uint8_t data = UDR0;
//  	uint8_t next_head;
  // Pick off runtime command characters directly from the serial stream. These characters are
  // not passed into the buffer, but these set system state flag bits for runtime execution.
	/*从串行流中直接选择运行时命令字符。这些字符是不传递到缓冲区，但是这些设置系统的状态标记位用于运行时执行。*/
		
  switch (data) {
    case CMD_STATUS_REPORT: sys.execute |= EXEC_STATUS_REPORT; break; // Set as true
    case CMD_CYCLE_START:   sys.execute |= EXEC_CYCLE_START; break; // Set as true
    case CMD_FEED_HOLD:     sys.execute |= EXEC_FEED_HOLD; break; // Set as true
    case CMD_RESET:         mc_reset(); break; // Call motion control reset routine.
    default: // Write character to buffer    
      next_head = rx_buffer_head + 1;
      if (next_head == RX_BUFFER_SIZE) { next_head = 0; }
    
      // Write data to buffer unless it is full.
      if (next_head != rx_buffer_tail) {
        rx_buffer[rx_buffer_head] = data;
        rx_buffer_head = next_head;    
        
//        #ifdef ENABLE_XONXOFF
//          if ((get_rx_buffer_count() >= RX_BUFFER_FULL) && flow_ctrl == XON_SENT) {
//            flow_ctrl = SEND_XOFF;
//            UCSR0B |=  (1 << UDRIE0); // Force TX
//          } 
//        #endif

      }
  }
//}
//--------------------------
	}
	if (USART_GetITStatus(USART1, USART_IT_TXE) != RESET) 	//写数据寄存器空，可以写数据
	{
		//-----------------【BUFF空中断】
//		// Data Register Empty Interrupt handler
//		ISR(SERIAL_UDRE)
//		{
		  // Temporary tx_buffer_tail (to optimize for volatile)
		  uint8_t tail = tx_buffer_tail;
		  
		  #ifdef ENABLE_XONXOFF
		    if (flow_ctrl == SEND_XOFF) { 
		      UDR0 = XOFF_CHAR; 
		      flow_ctrl = XOFF_SENT; 
		    } else if (flow_ctrl == SEND_XON) { 
		      UDR0 = XON_CHAR; 
		      flow_ctrl = XON_SENT; 
		    } else
		  #endif
		  { 
		    // Send a byte from the buffer
		   //==========================================

		    USART1->DR= tx_buffer[tail];  //发送
		   //==========================================
		    // Update tail position
		    tail++;
		    if (tail == TX_BUFFER_SIZE) { tail = 0; }
		  
		    tx_buffer_tail = tail;
		  }
		  
		  // Turn off Data Register Empty Interrupt to stop tx-streaming if this concludes the transfer
		  if (tail == tx_buffer_head) {
		  //==========================================
//		   UCSR0B &= ~(1 << UDRIE0);						//如果输出到对首，关闭TXE中断
		   USART_ITConfig(USART1, USART_IT_TXE, DISABLE);
		   //==========================================
		  }
	}
}


//==========================================

