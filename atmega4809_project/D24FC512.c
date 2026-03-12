#define F_CPU	5000000UL		// Max System Clock Frequency at 1.8V ~ 5.5V VDD

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/sfr_defs.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "twi.h"
#include "D24FC512.h"

uint8_t EEPROM_ReadByte( uint16_t adr ) {
	return I2C_readAddress_Byte( D24FC512_ADDR, adr );
}

uint16_t EEPROM_ReadUint16( uint16_t adr ) {
	return I2C_readAddress_Uint16( D24FC512_ADDR, adr );
}

void EEPROM_ReadBlock( uint16_t adr, uint8_t *buffer, uint8_t len ) {
	I2C_readAddress_Block( D24FC512_ADDR, adr, buffer, len );	
}

void EEPROM_WriteByte( uint16_t adr, uint8_t dat ) {
	I2C_writeAddress_Byte( D24FC512_ADDR, adr, dat );
}

void EEPROM_WriteUint16( uint16_t adr, uint16_t dat ) {
	I2C_writeAddress_Uint16( D24FC512_ADDR, adr, dat );
}


#define PAGE_NO		128

void EEPROM_WritePage( uint16_t adr, uint8_t *buffer, uint8_t len ) {
	uint16_t	pageStart, pageEnd;
	
	pageStart = adr & ~(PAGE_NO - 1);
	pageEnd = pageStart + PAGE_NO;
	if ( (adr + len) <= pageEnd ) 
		I2C_writeAddress_Block( D24FC512_ADDR, adr, buffer, len );
	else
		I2C_writeAddress_Block( D24FC512_ADDR, adr, buffer, pageEnd - adr );
	_delay_ms(5);
}


void EEPROM_WriteAnyBlock( uint16_t adr, uint8_t *pbytes, uint16_t siz ) {
	uint16_t adr_base, first_siz, block_no;
	
	adr_base = adr & ~(PAGE_NO - 1);
	first_siz = PAGE_NO - (adr - adr_base);
	if ( siz > first_siz ) {
		EEPROM_WritePage( adr, pbytes, (uint8_t)first_siz );			// 1 step
		siz -= first_siz; pbytes += first_siz; adr += first_siz;
		
		block_no = siz / PAGE_NO;
		if ( block_no ) {
			for ( uint16_t i = 0 ; i < block_no ; i++ ) {
				EEPROM_WritePage( adr, pbytes, (uint8_t)PAGE_NO );		// 2 step * block_no
				siz -= PAGE_NO; pbytes += PAGE_NO; adr += PAGE_NO;
			}		
		} 
		if ( siz ) {
			EEPROM_WritePage( adr, pbytes, (uint8_t)siz );				// 3 step
		}
	} else {
		EEPROM_WritePage( adr, pbytes, (uint8_t)siz );					// == EEPROM_WritePage
	}
}
