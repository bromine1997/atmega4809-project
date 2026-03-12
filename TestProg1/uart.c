/*
 * uart.c
 *
 * Created: 2021-10-04 오후 5:07:37
 *  Author: Tae Min Shin
 */ 
#define F_CPU	5000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdio.h>

#include "uart.h"

static FILE OUTPUT_device = FDEV_SETUP_STREAM( StdIO_Put, NULL, _FDEV_SETUP_WRITE);
static FILE INPUT_device  = FDEV_SETUP_STREAM( NULL, StdIO_Get, _FDEV_SETUP_READ);

RingBuffer_t	RxBuffer, TxBuffer;

void InitializeUsart0( long baud ) {
	PORTA.DIRSET = PIN0_bm;				// TxD output mode
	PORTA.OUTSET = PIN0_bm;				// TxD <= '1' (RS-232C Txd : Normal High)
	
	USART0.BAUD = (uint16_t)USART0_BAUD_RATE(baud);
	USART0.CTRLC |= USART_SBMODE_bm;	// 115200, n, 8, 2
	USART0.CTRLB |= USART_TXEN_bm | USART_RXEN_bm;
	USART0.CTRLA |= USART_RXCIE_bm;		// RxCIE
	
	RxBuffer.HeadIndex = RxBuffer.TailIndex = 0;
	TxBuffer.HeadIndex = TxBuffer.TailIndex = 0;
	RxBuffer.NoElement = TxBuffer.NoElement = 0;
	
	while ( USART0.STATUS & USART_RXCIF_bm ) USART0.RXDATAL;
	
	stdout = &OUTPUT_device;
	stdin  = &INPUT_device;
}

ISR(USART0_RXC_vect) {
	uint8_t	rxdat;
	
	rxdat = USART0.RXDATAL;
	if ( RxBuffer.NoElement < USART0_BUFFER_SIZE ) {
		RxBuffer.RingBuffer[RxBuffer.HeadIndex++] = rxdat;
		RxBuffer.HeadIndex &= USART0_BUFFER_MASK;			// Wrap around
		RxBuffer.NoElement++;
	} else {
		;
	}
}

bool USART0_CheckRxData( void ) {
	return ( RxBuffer.NoElement )? true : false;
}

// for STDIO Get
int StdIO_Get( FILE *stream ) {
	return (int)USART0_GetChar();
}

uint8_t USART0_GetChar( void ) {
	uint8_t	rxdat;
	
	while ( RxBuffer.NoElement == 0 ) ;
	rxdat = RxBuffer.RingBuffer[RxBuffer.TailIndex++];
	RxBuffer.TailIndex &= USART0_BUFFER_MASK;
	cli();  RxBuffer.NoElement--;  sei();
		
	return rxdat;
}

ISR( USART0_DRE_vect) {
	if ( TxBuffer.NoElement > 0 ) {
		USART0.TXDATAL = TxBuffer.RingBuffer[TxBuffer.TailIndex++];
		TxBuffer.TailIndex &= USART0_BUFFER_MASK;
		TxBuffer.NoElement--;
	} else {
		USART0.CTRLA &= ~USART_DREIE_bm;
	}
}

// for STDIO Put
int StdIO_Put( char d, FILE *stream ) {
	USART0_PutChar( (uint8_t)d );
	return 0;
}

uint8_t USART0_PutChar( uint8_t dat ) {
	while ( TxBuffer.NoElement >= USART0_BUFFER_SIZE ) ;
	
	TxBuffer.RingBuffer[TxBuffer.HeadIndex++] = dat;
	TxBuffer.HeadIndex &= USART0_BUFFER_MASK;
	cli();  TxBuffer.NoElement++;  sei();
	
	USART0.CTRLA |= USART_DREIE_bm;				// Tx Interrupt Enable
	
	return	dat;
}

