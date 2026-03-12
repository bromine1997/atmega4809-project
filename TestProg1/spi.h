/*
 * spi.h
 *
 * Created: 2021-11-08 오후 5:37:15
 *  Author: Tae Min Shin
 */ 


#ifndef SPI_H_
#define SPI_H_

#define E_On   ( 0b00001000 )
#define E_Off  ( 0b11110111 )
#define RS_On  ( 0b00000010 )
#define RS_Off ( 0b11111101 )
#define RW_On  ( 0b00000100 )
#define RW_Off ( 0b11111011 )

//
#define	SS_HIGH		( PORTA.OUTSET = PIN7_bm )
#define SS_LOW		( PORTA.OUTCLR = PIN7_bm )

typedef enum {
	SPI_FREE,
	SPI_IDLE,
	SPI_BUSY,
	SPI_DONE	
} SPI_XferStaus_t;

typedef enum {
	spi_readwrite,
	spi_repeatBlock,
	spi_readstop,
	spi_stop
} spi_operation_t;

typedef spi_operation_t (*spi_callback_t)( void * p );

typedef struct spi_desciption {
	uint8_t *pBlockData;
	uint8_t  Size;
	volatile SPI_XferStaus_t Status;
	volatile uint8_t	 blockNo;
	
	spi_callback_t	callbackDataXferComplete;
	void *			callbackParameter;
} SPI_DESC_t;

// MCP23S17 IO EXPANDER REGISTERS
#define		IOX_ADR_WRITE	0x40
#define		IOX_ADR_READ	0x41
#define		IOX_IODIRA		0x00
#define		IOX_IODIRB		0x01
#define		IOX_IOCON		0x0a
#define		IOX_GPIOA		0x12
#define		IOX_GPIOB		0x13

void InitializeSPI( void );
void InitializeIOX( void );
void SPI_Block_ReadWriteStart( uint8_t *block, uint8_t size );
void ScanKeySwISR( void );

void SendLCDbySpiISR( void );

void InitializeIOX_CLCD( void );
void IOX_CLCD_ClearDisplay( void );
void IOX_CLCD_GotoRC( uint8_t row, uint8_t col );
void IOX_CLCD_DisplayString( uint8_t r, uint8_t c, char *str );
void IOX_CLCD_makeFont( uint8_t addr, uint8_t *user_font );

#endif /* SPI_H_ */