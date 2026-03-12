/*
 * uart.h
 *
 * Created: 2021-10-04 오후 5:07:57
 *  Author: Tae Min Shin
 */ 


#ifndef UART_H_
#define UART_H_

#define USART0_BAUD_RATE(BAUD_RATE)		(64.0 * F_CPU / ( 16.0 * baud ) + 0.5)
#define USART0_BUFFER_SIZE				32		// 2^n < 256
#define USART0_BUFFER_MASK				(USART0_BUFFER_SIZE - 1)

typedef struct ring_buffer {
	uint8_t	RingBuffer[USART0_BUFFER_SIZE];
	volatile uint8_t HeadIndex, TailIndex;
	volatile uint8_t NoElement;
} RingBuffer_t;


void InitializeUsart0( long baud );
bool USART0_CheckRxData( void );
uint8_t USART0_GetChar( void );
uint8_t USART0_PutChar( uint8_t dat );

int StdIO_Get( FILE *stream );
int StdIO_Put( char d, FILE *stream );

#endif /* UART_H_ */