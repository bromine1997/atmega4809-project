/*
 * twi.c
 *
 * Created: 2021-10-25 오후 7:02:16
 *  Author: Tae Min Shin
 */ 
#define F_CPU	5000000UL		// Max System Clock Frequency at 1.8V ~ 5.5V VDD

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdio.h>

#include "twi.h"

i2c_info_t	i2c_status;

void InitializeTWI( void ) {
	PORTA.DIRSET = PIN2_bm | PIN3_bm;		// PA2:SDA, PA3:SCL Out Mode
	PORTA.OUTSET = PIN2_bm | PIN3_bm;		// PA2:SDA, PA3:SCL <- '1'
	
	TWI0.MBAUD = 20;						// Fscl = 100KHz, Trise = 0
	TWI0.MCTRLA |= TWI_RIEN_bm | TWI_WIEN_bm | TWI_ENABLE_bm;
}

//
//
i2c_fsm_states_t FSM_HANDLER_IDLE( void ) {
	i2c_status.busy = false;
	i2c_status.error = I2C_NOERR;
	return I2C_IDLE;
}

i2c_fsm_states_t FSM_HANDLER_STOP( void ) {
	TWI0.MCTRLB |= TWI_MCMD_STOP_gc;
	return FSM_HANDLER_IDLE();
}

i2c_fsm_states_t FSM_HANDLER_NAK_STOP( void ) {
	TWI0.MCTRLB |= TWI_ACKACT_bm;
	TWI0.MCTRLB |= TWI_MCMD_STOP_gc;
	return FSM_HANDLER_IDLE();
}

i2c_fsm_states_t FSM_HANDLER_SEND_ADR_READ( void ) {
	i2c_status.addressNAKCheck = true;
	TWI0.MADDR = i2c_status.address << 1 | 1;
	return I2C_RX;
}

i2c_fsm_states_t FSM_HANDLER_SEND_ADR_WRITE( void ) {
	i2c_status.addressNAKCheck = true;
	TWI0.MADDR = i2c_status.address << 1;
	return I2C_TX;
}

i2c_fsm_states_t FSM_HANDLER_TX( void ) {
	if ( TWI0.MSTATUS & TWI_RXACK_bm ) {
		return FSM_HANDLER_STOP();
	} else {
		i2c_status.addressNAKCheck = false;
		TWI0.MDATA = *i2c_status.data_ptr++;
		return ( --i2c_status.data_length )? I2C_TX : I2C_TX_COMPLETE;
	}
}

i2c_fsm_states_t FSM_HANDLER_NAK_RESTART_READ( void ) {
	TWI0.MCTRLB |= TWI_ACKACT_bm;
	TWI0.MCTRLB |= TWI_MCMD_REPSTART_gc;
	return FSM_HANDLER_SEND_ADR_READ();
}

i2c_fsm_states_t FSM_HANDLER_NAK_RESTART_WRITE( void ) {
	TWI0.MCTRLB |= TWI_ACKACT_bm;
	TWI0.MCTRLB |= TWI_MCMD_REPSTART_gc;
	return FSM_HANDLER_SEND_ADR_WRITE();
}

i2c_fsm_states_t FSM_HANDLER_RESET( void ) {
	i2c_status.busy = false;
	// error : fail processing
	return I2C_RESET;
}
i2c_fsm_states_t FSM_HANDLER_ADDRESS_NAK( void ) {
	TWI0.MCTRLB |= TWI_MCMD_STOP_gc;
	i2c_status.error = I2C_FAIL;
	return FSM_HANDLER_RESET();
}

i2c_fsm_states_t FSM_HANDLER_BUS_COLLISION( void ) {
	TWI0.MSTATUS |= TWI_ARBLOST_bm;
	i2c_status.error = I2C_FAIL;
	return FSM_HANDLER_RESET();
}

i2c_fsm_states_t FSM_HANDLER_BUS_ERROR( void ) {
	TWI0.MSTATUS |= TWI_BUSERR_bm;
	TWI0.MSTATUS |= TWI_BUSSTATE_IDLE_gc;
	TWI0.MCTRLB |= TWI_FLUSH_bm;
	
	i2c_status.error = I2C_FAIL;
	return FSM_HANDLER_RESET();
}

i2c_fsm_states_t FSM_HANDLER_RX( void ) {
	i2c_status.addressNAKCheck = false;
	
	*i2c_status.data_ptr++ = TWI0.MDATA;

	if ( --i2c_status.data_length ) {
		TWI0.MCTRLB &= ~TWI_ACKACT_bm;
		TWI0.MCTRLB |= TWI_MCMD_RECVTRANS_gc;
		return I2C_RX;
	} else {
		i2c_status.bufferFree = true;
		switch ( i2c_status.callbackDataXferComplete(i2c_status.callbackParameter)) {
			case i2c_restart_read :
				return FSM_HANDLER_NAK_RESTART_READ();
			case i2c_restart_write :
				return FSM_HANDLER_NAK_RESTART_WRITE();
			case i2c_contine :
			case i2c_stop :
			default :
				return FSM_HANDLER_NAK_STOP();
		}
	}
}

i2c_fsm_states_t FSM_HANDLER_TX_COMPLETE( void ) {
	if ( TWI0.MSTATUS & TWI_RXACK_bm ) {
		return FSM_HANDLER_STOP();
	} else {
		i2c_status.bufferFree = true;
		switch ( i2c_status.callbackDataXferComplete(i2c_status.callbackParameter)) {
			case i2c_restart_read :
				return FSM_HANDLER_NAK_RESTART_READ();
			case i2c_restart_write :
				return FSM_HANDLER_NAK_RESTART_WRITE();
			case i2c_contine :
				return FSM_HANDLER_TX();
			case i2c_stop :
			default :
				return FSM_HANDLER_STOP();
		}
	}
}

stateHandler_t I2C_fsm_Handler_table[] = {
	FSM_HANDLER_IDLE,
	FSM_HANDLER_SEND_ADR_READ,
	FSM_HANDLER_SEND_ADR_WRITE,
	FSM_HANDLER_TX,
	FSM_HANDLER_RX,
	FSM_HANDLER_TX_COMPLETE,
	FSM_HANDLER_STOP,
	FSM_HANDLER_NAK_STOP,
	FSM_HANDLER_NAK_RESTART_READ,
	FSM_HANDLER_NAK_RESTART_WRITE,
	FSM_HANDLER_ADDRESS_NAK,
	FSM_HANDLER_BUS_COLLISION,
	FSM_HANDLER_BUS_ERROR,
	FSM_HANDLER_RESET		
};

ISR( TWI0_TWIM_vect ) {
	TWI0.MSTATUS |= TWI_RIF_bm | TWI_WIF_bm;
	
	if ( i2c_status.addressNAKCheck && ( TWI0.MSTATUS & TWI_RXACK_bm ) )
		i2c_status.state = I2C_ADDRESS_NAK;
	
	if ( TWI0.MSTATUS &  TWI_ARBLOST_bm )
		i2c_status.state = I2C_BUS_COLLISION;
		
	if ( TWI0.MSTATUS &  TWI_BUSERR_bm )
		i2c_status.state = I2C_BUS_ERROR;
	
	i2c_status.state = I2C_fsm_Handler_table[i2c_status.state]();
}

typedef enum { M_READ, M_WRITE } master_op_t;
	
i2c_error_t I2C_master_read_write( master_op_t mrw ) {
	i2c_error_t ret = I2C_BUSY;
	
	if ( !i2c_status.busy ) {
		i2c_status.busy = true;
		ret = I2C_NOERR;
		
		i2c_status.state = ( mrw == M_READ )? I2C_SEND_ADR_READ : I2C_SEND_ADR_WRITE;
		i2c_status.state = I2C_fsm_Handler_table[i2c_status.state]();
	}
	
	return ret;
}

i2c_operation_t I2C_Stop_CB( void * p ) {	
	return i2c_stop;
}

//
//
i2c_error_t I2C_open( i2c_address_t address ) {
	i2c_error_t ret = I2C_BUSY;
	
	if ( !i2c_status.inUse ) {
		i2c_status.address					= address;
		
		i2c_status.busy						= false;
		i2c_status.inUse					= true;
		i2c_status.addressNAKCheck			= false;
		i2c_status.bufferFree				= true;
		
		i2c_status.state					= I2C_IDLE;
		
		i2c_status.callbackDataXferComplete = I2C_Stop_CB;
		i2c_status.callbackParameter		= NULL;
		
		TWI0.MCTRLB		|= TWI_FLUSH_bm;
		TWI0.MSTATUS	|= TWI_BUSSTATE_IDLE_gc;
		TWI0.MSTATUS	|= TWI_RIF_bm | TWI_WIF_bm;
		TWI0.MCTRLB		|= TWI_WIEN_bm | TWI_RIEN_bm;
		
		ret = I2C_NOERR;
	}
	return ret;
}

i2c_error_t I2C_close( void ) {
	i2c_error_t ret = I2C_BUSY;
	
	if ( TWI0.MSTATUS & TWI_BUSERR_bm ) {
		i2c_status.error = I2C_FAIL;
		i2c_status.busy = false;
	}
	if ( !i2c_status.busy ) {
		i2c_status.inUse = false;
		i2c_status.address = 0xff;
		TWI0.MSTATUS &= ~( TWI_WIEN_bm | TWI_RIEN_bm );
		ret = i2c_status.error;
	}
	
	return ret;
}

void I2C_SetDataXferCompleteCallBack( i2c_callback_t cb, void * p ) {
	if ( cb ) {
		i2c_status.callbackDataXferComplete = cb;
		i2c_status.callbackParameter		= p;
	} else {
		i2c_status.callbackDataXferComplete = I2C_Stop_CB;
		i2c_status.callbackParameter		= NULL;	
	}
}

void I2C_SetDataBuffer( void *buffer, uint8_t size ) {
	if ( i2c_status.bufferFree ) {
		i2c_status.data_ptr		= buffer;
		i2c_status.data_length	= size;
		i2c_status.bufferFree	= false;
	}
}

//
//

void I2C_writeCommand( i2c_address_t addr, uint8_t cmd ) {
	while ( I2C_open( addr ) == I2C_BUSY ) ;
	I2C_SetDataBuffer( &cmd, sizeof(cmd) );
	I2C_master_read_write( M_WRITE );
	while ( I2C_close() == I2C_BUSY ) ;
}

i2c_operation_t I2C_writeCmd_Byte_CB( void *p ) {
	I2C_SetDataBuffer( p, 1 );
	I2C_SetDataXferCompleteCallBack( I2C_Stop_CB, NULL );
	return i2c_contine;
}
//
void I2C_writeCmd_Byte( i2c_address_t addr, uint8_t cmd, uint8_t dat ) {
	while ( I2C_open( addr ) == I2C_BUSY ) ;
	I2C_SetDataXferCompleteCallBack( I2C_writeCmd_Byte_CB, &dat );
	I2C_SetDataBuffer( &cmd, sizeof(cmd) );
	I2C_master_read_write( M_WRITE );
	while ( I2C_close() == I2C_BUSY ) ;
}

i2c_operation_t I2C_writeCmd_Uint16_CB( void *p ) {
	I2C_SetDataBuffer( p, 2 );
	I2C_SetDataXferCompleteCallBack( I2C_Stop_CB, NULL );
	return i2c_contine;
}
//
void I2C_writeCmd_Uint16( i2c_address_t addr, uint8_t cmd, uint16_t dat ) {
	while ( I2C_open( addr ) == I2C_BUSY ) ;
	I2C_SetDataXferCompleteCallBack( I2C_writeCmd_Uint16_CB, &dat );
	I2C_SetDataBuffer( &cmd, sizeof(cmd) );
	I2C_master_read_write( M_WRITE );
	while ( I2C_close() == I2C_BUSY ) ;
}
//
void I2C_writeBlock( i2c_address_t addr, uint8_t *data, uint8_t len ) {
	while ( I2C_open( addr ) == I2C_BUSY ) ;
	I2C_SetDataBuffer( data, len );
	I2C_master_read_write( M_WRITE );
	while ( I2C_close() == I2C_BUSY ) ;
}

i2c_operation_t I2C_readCmd_Byte_CB( void *p ) {
	I2C_SetDataBuffer( p, 1 );
	I2C_SetDataXferCompleteCallBack( I2C_Stop_CB, NULL );
	return i2c_restart_read;
}
//
uint8_t I2C_readCmd_Byte( i2c_address_t addr, uint8_t cmd ) {
	uint8_t		dat;
	
	while ( I2C_open( addr ) == I2C_BUSY ) ;
	I2C_SetDataXferCompleteCallBack( I2C_readCmd_Byte_CB, &dat );
	I2C_SetDataBuffer( &cmd, sizeof(cmd) );
	I2C_master_read_write( M_WRITE );
	while ( I2C_close() == I2C_BUSY ) ;
	
	return dat;
}

i2c_operation_t I2C_readCmd_Uint16_CB( void *p ) {
	I2C_SetDataBuffer( p, 2 );
	I2C_SetDataXferCompleteCallBack( I2C_Stop_CB, NULL );
	return i2c_restart_read;
}
//
uint16_t I2C_readCmd_Uint16( i2c_address_t addr, uint8_t cmd ) {
	uint16_t		dat;
	
	while ( I2C_open( addr ) == I2C_BUSY ) ;
	I2C_SetDataXferCompleteCallBack( I2C_readCmd_Uint16_CB, &dat );
	I2C_SetDataBuffer( &cmd, sizeof(cmd) );
	I2C_master_read_write( M_WRITE );
	while ( I2C_close() == I2C_BUSY ) ;
	
	return ( dat << 8 | dat >> 8 );		// Byte swapping
}

typedef struct {
	uint8_t *buff;
	uint8_t	len;
} i2c_block_t;

i2c_operation_t I2C_readCmd_Block_CB( void *p ) {
	i2c_block_t *pp = (i2c_block_t *)p;
	
	I2C_SetDataBuffer( pp->buff, pp->len );
	I2C_SetDataXferCompleteCallBack( I2C_Stop_CB, NULL );
	
	return i2c_restart_read;
}

void I2C_readCmd_Block( i2c_address_t addr, uint8_t cmd, uint8_t *buffer, uint8_t len ) {
	i2c_block_t block;
	
	block.buff = buffer;
	block.len = len;
	
	while ( I2C_open( addr ) == I2C_BUSY ) ;
	I2C_SetDataXferCompleteCallBack( I2C_readCmd_Block_CB, &block );
	I2C_SetDataBuffer( &cmd, sizeof(cmd) );
	I2C_master_read_write( M_WRITE );
	while ( I2C_close() == I2C_BUSY ) ;
}

//
//	for 24FC512 Serial EEPROM

i2c_operation_t I2C_writeAddress_Byte_CB( void *p ) {
	I2C_SetDataBuffer( p, 1 );
	I2C_SetDataXferCompleteCallBack( I2C_Stop_CB, NULL );
	return i2c_contine;
}
//
void I2C_writeAddress_Byte( i2c_address_t addr, uint16_t address, uint8_t dat ) {
	while ( I2C_open( addr ) == I2C_BUSY ) ;
	I2C_SetDataXferCompleteCallBack( I2C_writeAddress_Byte_CB, &dat );
	address = address << 8 | address >> 8;
	I2C_SetDataBuffer( &address, sizeof(address) );
	I2C_master_read_write( M_WRITE );
	while ( I2C_close() == I2C_BUSY ) ;
}

i2c_operation_t I2C_writeAddress_Uint16_CB( void *p ) {
	I2C_SetDataBuffer( p, 2 );
	I2C_SetDataXferCompleteCallBack( I2C_Stop_CB, NULL );
	return i2c_contine;
}
//
void I2C_writeAddress_Uint16( i2c_address_t addr, uint16_t address, uint16_t dat ) {
	while ( I2C_open( addr ) == I2C_BUSY ) ;
	I2C_SetDataXferCompleteCallBack( I2C_writeAddress_Uint16_CB, &dat );
	address = address << 8 | address >> 8;
	I2C_SetDataBuffer( &address, sizeof(address) );
	I2C_master_read_write( M_WRITE );
	while ( I2C_close() == I2C_BUSY ) ;
}

i2c_operation_t I2C_writeAddress_Block_CB( void *p ) {
	i2c_block_t *pp = (i2c_block_t *)p;
	
	I2C_SetDataBuffer( pp->buff, pp->len );
	I2C_SetDataXferCompleteCallBack( I2C_Stop_CB, NULL );
	
	return i2c_contine;
}
//
void I2C_writeAddress_Block( i2c_address_t addr, uint16_t address, uint8_t *buffer, uint8_t len ) {
	i2c_block_t block;
	
	block.buff = buffer;
	block.len = len;
	
	while ( I2C_open( addr ) == I2C_BUSY ) ;
	I2C_SetDataXferCompleteCallBack( I2C_writeAddress_Block_CB, &block );
	address = address << 8 | address >> 8;
	I2C_SetDataBuffer( &address, sizeof(address) );
	I2C_master_read_write( M_WRITE );
	while ( I2C_close() == I2C_BUSY ) ;
}

//

i2c_operation_t I2C_readAddress_Byte_CB( void *p ) {
	I2C_SetDataBuffer( p, 1 );
	I2C_SetDataXferCompleteCallBack( I2C_Stop_CB, NULL );
	return i2c_restart_read;
}
//
uint8_t I2C_readAddress_Byte( i2c_address_t addr, uint16_t address ) {
	uint8_t		dat;
	
	while ( I2C_open( addr ) == I2C_BUSY ) ;
	I2C_SetDataXferCompleteCallBack( I2C_readAddress_Byte_CB, &dat );
	address = address << 8 | address >> 8;
	I2C_SetDataBuffer( &address, sizeof(address) );
	I2C_master_read_write( M_WRITE );
	while ( I2C_close() == I2C_BUSY ) ;
	
	return dat;
}

i2c_operation_t I2C_readAddress_Uint16_CB( void *p ) {
	I2C_SetDataBuffer( p, 2 );
	I2C_SetDataXferCompleteCallBack( I2C_Stop_CB, NULL );
	return i2c_restart_read;
}
//
uint16_t I2C_readAddress_Uint16( i2c_address_t addr, uint16_t address ) {
	uint16_t		dat;
	
	while ( I2C_open( addr ) == I2C_BUSY ) ;
	I2C_SetDataXferCompleteCallBack( I2C_readAddress_Uint16_CB, &dat );
	address = address << 8 | address >> 8;
	I2C_SetDataBuffer( &address, sizeof(address) );
	I2C_master_read_write( M_WRITE );
	while ( I2C_close() == I2C_BUSY ) ;
	
	return dat;		
}

i2c_operation_t I2C_readAddress_Block_CB( void *p ) {
	i2c_block_t *pp = (i2c_block_t *)p;
	
	I2C_SetDataBuffer( pp->buff, pp->len );
	I2C_SetDataXferCompleteCallBack( I2C_Stop_CB, NULL );
	
	return i2c_restart_read;
}
//
void I2C_readAddress_Block( i2c_address_t addr, uint16_t address, uint8_t *buffer, uint8_t len ) {
	i2c_block_t block;
	
	block.buff = buffer;
	block.len = len;
	
	while ( I2C_open( addr ) == I2C_BUSY ) ;
	I2C_SetDataXferCompleteCallBack( I2C_readAddress_Block_CB, &block );
	address = address << 8 | address >> 8;
	I2C_SetDataBuffer( &address, sizeof(address) );
	I2C_master_read_write( M_WRITE );
	while ( I2C_close() == I2C_BUSY ) ;
}

