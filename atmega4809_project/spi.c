
#define F_CPU	5000000UL		// Max System Clock Frequency at 1.8V ~ 5.5V VDD

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdio.h>

#include "spi.h"

void IOX_Send2LCD_Async( uint8_t dat, bool rs );

extern struct {
	bool	FP, CP;
	bool	P1, P2;
	uint8_t	DIE, SUR;
	uint8_t TR1, TR2;
	
	bool	ROL, HOLD, NEW_GAME;	// Input SW	
	bool	WIN;
} PigData;

extern enum pigmode { PigNONE, Pig1Player, Pig2Player } Pig;
	
extern volatile uint8_t  gkswScanCode;
extern volatile bool gkswFlag;
extern bool	stop;

uint8_t	KeySwCol;

SPI_DESC_t SPI_Desc = { 0 };
uint8_t	spi_buff[32];

void InitializeSPI( void ) {
	SPI_Desc.Status = SPI_FREE;
	
	PORTA.DIRSET = PIN4_bm | PIN6_bm | PIN7_bm;			//PA4(MO), PA5(MI):INPUT, PA6(SCK), PA7(SS)
	PORTA.OUTSET = PIN7_bm;								// SS : Normal High
	
	SPI0.CTRLA = SPI_MASTER_bm | SPI_ENABLE_bm;			// Master Mode, 5/4MHz, SPI Enable, Mode 0
	SPI0.INTCTRL = SPI_IE_bm;
	
	SPI_Desc.Status = SPI_IDLE;
}

spi_operation_t SPI_Stop_CB( void * p ) {
	return spi_stop;
}

void SPI_SetDataXferCompleteCallBack( spi_callback_t cb, void * p ) {
	if ( cb ) {
		SPI_Desc.callbackDataXferComplete	= cb;
		SPI_Desc.callbackParameter			= p;
	} else {
		SPI_Desc.callbackDataXferComplete	= SPI_Stop_CB;
		SPI_Desc.callbackParameter			= NULL;
	}
}

spi_operation_t SPI_InitIOX_CB( void *p ) {
	SPI_Desc.pBlockData = p;
	SPI_Desc.Size = 3;
	SPI_SetDataXferCompleteCallBack( SPI_Stop_CB, NULL );	
	return spi_readwrite;
}

void InitializeIOX( void ) {
	SPI_Desc.Status = SPI_IDLE;	
	spi_buff[0] = IOX_ADR_WRITE;						// Slave Address/Wr
	spi_buff[1] = IOX_IOCON;							// IOCON
	spi_buff[2] = 0x30;									// SEQOP Disable, DISSLW Disable
	
	spi_buff[3] = IOX_ADR_WRITE;						// Slave Address/Wr
	spi_buff[4] = IOX_IODIRB;							// IODIR B
	spi_buff[5] = 0x0f;									// GPB0-3 : Input, GPB4-7 : Output
	
	SPI_SetDataXferCompleteCallBack( SPI_InitIOX_CB, &spi_buff[3] );
	SPI_Block_ReadWriteStart( spi_buff, 3 );
	while ( SPI_Desc.Status == SPI_BUSY ) ;

	_delay_ms(1);
	
	SPI_Desc.Status = SPI_IDLE;
	spi_buff[0] = IOX_ADR_WRITE;						// Slave Address/Wr
	spi_buff[1] = IOX_IODIRA;							// IODIR A
	spi_buff[2] = 0x01;									// GPA0 : Input(1), GPA1-7 : Output(0)
														
	spi_buff[3] = IOX_ADR_WRITE;						// Slave Address/Wr
	spi_buff[4] = IOX_GPIOA;							// GPIOA
	spi_buff[5] = 0x00;									// 00
	
	
	SPI_SetDataXferCompleteCallBack( SPI_InitIOX_CB, &spi_buff[3] );
	SPI_Block_ReadWriteStart( spi_buff, 3 );
	while ( SPI_Desc.Status == SPI_BUSY ) ;
}

spi_operation_t SPI_ReadStop_CB( void * p ) {
	return spi_readstop;
}

ISR( SPI0_INT_vect ) {
	SPI0.INTFLAGS |= SPI_IF_bm;
	
	*SPI_Desc.pBlockData++ = SPI0.DATA;
	if ( --SPI_Desc.Size > 0 ) {
		SPI0.DATA = *SPI_Desc.pBlockData;
	} else {
		SS_HIGH;
		switch ( SPI_Desc.callbackDataXferComplete(SPI_Desc.callbackParameter)) {
		case spi_repeatBlock :
		case spi_readwrite :
			SS_LOW;
			SPI0.DATA = *SPI_Desc.pBlockData;
			break;
		case spi_readstop :
			KeySwCol = SPI0.DATA;
		case spi_stop :
			SPI_Desc.Status = SPI_DONE;
			break;
		}
	}
}

void SPI_Block_ReadWriteStart( uint8_t *block, uint8_t size ) {
	SPI_Desc.pBlockData = block;
	SPI_Desc.Size = size;
	SPI_Desc.Status = SPI_BUSY;
	
	SS_LOW;
	SPI0.DATA = *block;
}

spi_operation_t SPI_getKeySW_CB( void *p ) {
	SPI_Desc.pBlockData = p;
	SPI_Desc.Size = 3;
	SPI_SetDataXferCompleteCallBack( SPI_ReadStop_CB, NULL );
	return spi_readwrite;
}

void getKeySW( uint8_t row ) {
	SPI_Desc.Status = SPI_IDLE;
	spi_buff[0] = IOX_ADR_WRITE;					// Slave Address/Wr
	spi_buff[1] = IOX_GPIOB;						// GPIOB B Out
	spi_buff[2] = row;								// Out Data to High Nibble
	
	spi_buff[3] = IOX_ADR_READ;						// Slave Address/Wr
	spi_buff[4] = IOX_GPIOB;						// GPIOB B In form Low Nibble
	spi_buff[5] = 0x00;								// In Data
	
	SPI_SetDataXferCompleteCallBack( SPI_getKeySW_CB, &spi_buff[3] );	
	SPI_Block_ReadWriteStart( spi_buff, 3 );
}

typedef enum { kswILDE0, kswILDE1, kswPRESSING, kswPRESSED0, kswPRESSED1, kswPRESSED2, kswPRESSED3, kswPRESSED,kswRELEASING } KeySW_State_t;

void ScanKeySwISR( void ) {		// in ISR
	static KeySW_State_t SwitchState = kswILDE0;
	uint8_t	i, col;

	switch ( SwitchState ) {
	case kswILDE0 :
		getKeySW(0x00);  SwitchState = kswILDE1;
		break;
	case kswILDE1 :
		if ( Pig ) {
			PigData.ROL = PigData.HOLD = PigData.NEW_GAME = false;
		}
		if ( (KeySwCol & 0xf) != 0xf ) SwitchState = kswPRESSING;
		getKeySW(0x00);
		break;
	case kswPRESSING :
		if ( (KeySwCol & 0xf) != 0xf ) {
			SwitchState = kswPRESSED0;
			getKeySW(~PIN4_bm);
		} else {
			SwitchState = kswILDE1;
			getKeySW(0x00);
		}
		break;
	case kswPRESSED0 :
		col = KeySwCol & 0xf;
		if ( col == 0xf ) {
			SwitchState = kswPRESSED1;
			getKeySW(~PIN5_bm);
		} else {
			for ( i = 0 ; i < 4 ; i++ ) {
				if ( (col & ( 1 << i )) == 0 ) {
					gkswScanCode = i;  gkswFlag = true;
					SwitchState = kswPRESSED;
					getKeySW(0x00);
					break; 
				}
			}
		}
		break;
	case kswPRESSED1 :
		col = KeySwCol & 0xf;
		if ( col == 0xf ) {
			SwitchState = kswPRESSED2;
			getKeySW(~PIN6_bm);
		} else {
			for ( i = 0 ; i < 4 ; i++ ) {
				if ( (col & ( 1 << i )) == 0 ) {
					gkswScanCode = i + 4;  gkswFlag = true;
					SwitchState = kswPRESSED;
					getKeySW(0x00);
					break;
				}
			}
		}
		break;
	case kswPRESSED2 :
		col = KeySwCol & 0xf;
		if ( col == 0xf ) {
			SwitchState = kswPRESSED3;
			getKeySW((uint8_t)~PIN7_bm);
		} else {
			for ( i = 0 ; i < 4 ; i++ ) {
				if ( (col & ( 1 << i )) == 0 ) {
					gkswScanCode = i + 8;  gkswFlag = true;
					SwitchState = kswPRESSED;
					getKeySW(0x00);
					break;
				}
			}
		}
		break;
	case kswPRESSED3 :
		col = KeySwCol & 0xf;
		if ( col != 0xf ) {
			for ( i = 0 ; i < 4 ; i++ ) {
				if ( (col & ( 1 << i )) == 0 ) {
					gkswScanCode = i + 12;  gkswFlag = true;
					break;
				}
			}
		} else {
			gkswScanCode = 0xff;  gkswFlag = true;
		}
		SwitchState = kswPRESSED;
		getKeySW(0x00);
		break;
	case kswPRESSED :
		if ( Pig ) {
			if ( gkswScanCode == 12 ) PigData.NEW_GAME = true;	// '*'
			if ( gkswScanCode == 13 ) PigData.ROL = true;		// '0'
			if ( gkswScanCode == 14 ) PigData.HOLD = true;		// '#'
		}
		if ( (KeySwCol & 0xf) == 0xf ) SwitchState = kswRELEASING;
		getKeySW(0x00);
		break;
	case kswRELEASING :
		SwitchState = ( (KeySwCol & 0xf) != 0xf )? kswPRESSED : kswILDE1;
		
		getKeySW(0x00);
		break;
	default:
		SwitchState = kswILDE0;
		break;
	}
}

//
//
typedef enum { op_idle, op_ready, op_busy, op_done } clcd_op_t;
	
struct clcd_buff {
	unsigned char Buffer[32];
	volatile unsigned char Size;
	volatile unsigned char Count;
	volatile bool RS;
	volatile clcd_op_t Status;
} ClcdData;

typedef enum { Clcd_WaitData, Clcd_SendSpi } clcd_state_t;
	
void SendLCDbySpiISR( void ) {
	static clcd_state_t clcd_state = Clcd_WaitData;
	
	switch( clcd_state ) {
		case Clcd_WaitData :
			if ( ClcdData.Status == op_ready ) {
				ClcdData.Count = 0;
				ClcdData.Status = op_busy;
				clcd_state = Clcd_SendSpi;
			}
			break;
		case Clcd_SendSpi :
			if ( ClcdData.Size-- > 0 ) 
				IOX_Send2LCD_Async( ClcdData.Buffer[ClcdData.Count++], ClcdData.RS );
			else {
				ClcdData.Status = op_done;
				clcd_state = Clcd_WaitData;
			}
			break;
		default:
			clcd_state = Clcd_WaitData;
			break;
	}
}

spi_operation_t SPI_Send2LCD_CB( void *p ) {
	SPI_Desc.Size = 3;
	if ( --SPI_Desc.blockNo == 0 ) 
		SPI_SetDataXferCompleteCallBack( SPI_Stop_CB, NULL );
	return spi_repeatBlock;
}

void IOX_Send2LCD_Async( uint8_t dat, bool rs ) {							// RS true : data, RS false : inst
	uint8_t	*pBuff;
	uint8_t ctl;
	
	SPI_Desc.Status = SPI_IDLE;
	
	pBuff = spi_buff;
	
	*pBuff++ = IOX_ADR_WRITE;											// Slave Address/Wr
	*pBuff++ = IOX_GPIOA;												// GPIOA Out
	*pBuff++ = ctl = (rs)? ((dat & 0xf0) | RS_On) : (dat & 0xf0);		// Out Data to High Nibble
	
	*pBuff++ = IOX_ADR_WRITE;						
	*pBuff++ = IOX_GPIOA;							
	*pBuff++ = ctl + E_On;												// 
	
	*pBuff++ = IOX_ADR_WRITE;						
	*pBuff++ = IOX_GPIOA;							
	*pBuff++ = ctl & E_Off;												// 
	
	*pBuff++ = IOX_ADR_WRITE;						
	*pBuff++ = IOX_GPIOA;							
	*pBuff++ = ctl = (rs)? ((dat << 4) | RS_On) : (dat << 4);			// 
	
	*pBuff++ = IOX_ADR_WRITE;						
	*pBuff++ = IOX_GPIOA;							
	*pBuff++ = ctl + E_On;												// 
	
	*pBuff++ = IOX_ADR_WRITE;						
	*pBuff++ = IOX_GPIOA;							
	*pBuff++ = ctl & E_Off;												// 
	
	SPI_Desc.blockNo = 5;	
	SPI_SetDataXferCompleteCallBack( SPI_Send2LCD_CB, &spi_buff[3] );
	SPI_Block_ReadWriteStart( spi_buff, 3 );
}

void IOX_CLCD_SendInst( uint8_t dat ) {
	ClcdData.Buffer[0] = dat;
	ClcdData.Size = 1;
	ClcdData.RS	= false;
	ClcdData.Status = op_ready;
	while ( ClcdData.Status != op_done ) ;
}

void IOX_CLCD_SendData( uint8_t dat ) {
	ClcdData.Buffer[0] = dat;
	ClcdData.Size = 1;
	ClcdData.RS	= true;
	ClcdData.Status = op_ready;
	while ( ClcdData.Status != op_done ) ;
}

void InitializeIOX_CLCD( void ) {
	_delay_ms(30);
	IOX_CLCD_SendInst(0x20);
	_delay_us(50);
	IOX_CLCD_SendInst(0x28);
	_delay_us(50);
	IOX_CLCD_SendInst(0x0c);
	_delay_us(50);
	IOX_CLCD_SendInst(0x01);
	_delay_ms(2);
	IOX_CLCD_SendInst(0x06);
	_delay_us(50);
	
	ClcdData.Status = op_idle;
}

void IOX_CLCD_ClearDisplay( void ) {
	IOX_CLCD_SendInst( 0x01 );
	_delay_ms(2);
}

void IOX_CLCD_GotoRC( uint8_t row, uint8_t col )
{
	col %= 16;										//열 16칸, 0-14
	row %= 2;										//행 2칸, 0-1
	
	uint8_t address = (0x40 * row) + col;			//0행: 0x00, 1행: 0x40부터 주소 시작
	IOX_CLCD_SendInst( 0x80 + address );
}

void IOX_CLCD_DisplayString( uint8_t r, uint8_t c, char *str ) {
	uint8_t		i, t;
	
	if ( !stop ) return;
	
	IOX_CLCD_GotoRC( r, c );
	for ( i = 0 ; ; i++ ) {
		if ( (t = *str++) ) {
			ClcdData.Buffer[i] = (unsigned char)t;
		} else break;
	}
	ClcdData.Size = i;
	ClcdData.RS	= true;
	ClcdData.Status = op_ready;	
	while ( ClcdData.Status != op_done ) ;
}
	
void IOX_CLCD_makeFont( uint8_t addr, uint8_t *user_font )
{
	IOX_CLCD_SendInst( 0x40 + addr * 8 );
	for( uint8_t i = 0; i < 8; i++ ) {
		IOX_CLCD_SendData( user_font[i] );
	}
}
