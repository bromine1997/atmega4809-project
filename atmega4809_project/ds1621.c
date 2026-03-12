#define F_CPU	5000000UL		// Max System Clock Frequency at 1.8V ~ 5.5V VDD

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdio.h>

#include "twi.h"
#include "ds1621.h"

void DS1621_StartConverT( void ) {
	I2C_writeCommand( DS1621_ADDR, DS1621_START_CONVERT_T );
}

void DS1621_StopConverT( void ) {
	I2C_writeCommand( DS1621_ADDR, DS1621_STOP_CONVERT_T );
}

uint16_t DS1621_ReadTemperature( void ) {
	return I2C_readCmd_Uint16( DS1621_ADDR, DS1621_READ_TEMPERATURE );
}
