/*
 * twi.h
 *
 * Created: 2021-10-25 오후 7:02:36
 *  Author: Tae Min Shin
 */ 


#ifndef TWI_H_
#define TWI_H_

typedef uint8_t		i2c_address_t;

typedef enum {
	i2c_stop,
	i2c_restart_read,
	i2c_restart_write,
	i2c_contine
} i2c_operation_t;

typedef enum {
	I2C_NOERR,
	I2C_BUSY,
	I2C_FAIL
} i2c_error_t;

typedef i2c_operation_t (*i2c_callback_t)( void * p );

typedef enum {
	I2C_IDLE = 0,
	I2C_SEND_ADR_READ,
	I2C_SEND_ADR_WRITE,
	I2C_TX,
	I2C_RX,
	I2C_TX_COMPLETE,
	I2C_STOP,
	I2C_NAK_STOP,
	I2C_NAK_RESTART_READ,
	I2C_NAK_RESTART_WRITE,
	I2C_ADDRESS_NAK,
	I2C_BUS_COLLISION,
	I2C_BUS_ERROR,
	I2C_RESET
} i2c_fsm_states_t;

typedef i2c_fsm_states_t	(*stateHandler_t)(void);

typedef struct {
	uint8_t				busy : 1;
	uint8_t				inUse : 1;
	uint8_t				bufferFree : 1;
	uint8_t				addressNAKCheck : 1;
	
	i2c_address_t		address;
	uint8_t	*			data_ptr;
	uint8_t				data_length;
	i2c_fsm_states_t	state;
	i2c_error_t			error;
	
	i2c_callback_t		callbackDataXferComplete;
	void *				callbackParameter;
} i2c_info_t;


void InitializeTWI( void );

void I2C_writeCommand( i2c_address_t addr, uint8_t cmd );
void I2C_writeBlock( i2c_address_t addr, uint8_t *data, uint8_t len );
void I2C_writeCmd_Byte( i2c_address_t addr, uint8_t cmd, uint8_t dat );
void I2C_writeCmd_Uint16( i2c_address_t addr, uint8_t cmd, uint16_t dat );
uint8_t I2C_readCmd_Byte( i2c_address_t addr, uint8_t cmd );
uint16_t I2C_readCmd_Uint16( i2c_address_t addr, uint8_t cmd );
void I2C_readCmd_Block( i2c_address_t addr, uint8_t cmd, uint8_t *buffer, uint8_t len );

void I2C_writeAddress_Byte( i2c_address_t addr, uint16_t address, uint8_t dat );
void I2C_writeAddress_Uint16( i2c_address_t addr, uint16_t address, uint16_t dat );
void I2C_writeAddress_Block( i2c_address_t addr, uint16_t address, uint8_t *buffer, uint8_t len );
uint8_t I2C_readAddress_Byte( i2c_address_t addr, uint16_t address );
uint16_t I2C_readAddress_Uint16( i2c_address_t addr, uint16_t address );
void I2C_readAddress_Block( i2c_address_t addr, uint16_t address, uint8_t *buffer, uint8_t len );

#endif /* TWI_H_ */