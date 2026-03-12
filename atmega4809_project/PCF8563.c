#define F_CPU	5000000UL		// Max System Clock Frequency at 1.8V ~ 5.5V VDD

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdio.h>

#include "twi.h"
#include "PCF8563.h"

char*	DAY_OF_WEEK_LONG[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" }; 
char*	DAY_OF_WEEK_SHORT[] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" }; 
	
CLOCK_t		Watch = { 0 };

static uint8_t BIN2BCD( uint8_t bin ) {
	return ((bin / 10) << 4) + (bin % 10);
}

static uint8_t BCD2BIN( uint8_t bcd ) {
	return ((bcd >> 4) * 10) + (bcd & 0x0f);
}

void PCF8563_wrieTimeDate( uint8_t hr, uint8_t min, uint8_t sec, uint8_t yr, uint8_t mon, uint8_t day, uint8_t dow ) {
	uint8_t send_buff[10];
	
	if ( mon == 0 || mon > 12 ) mon = 1;
	if ( day == 0 || day > 31 ) day = 1;
	if ( dow > 6 )				dow = 0;
	
	if ( sec > 59 ) sec = 0;
	if ( min > 59 ) min = 0;
	if ( hr > 23 )  hr  = 0;
	if ( yr > 99 )  yr  = 0;
	
	send_buff[0] = PCF8563_ControlStatus1;
	send_buff[1] = 0x00;
	send_buff[2] = 0x00;
	send_buff[3] = BIN2BCD( sec );
	send_buff[4] = BIN2BCD( min );
	send_buff[5] = BIN2BCD( hr );
	send_buff[6] = BIN2BCD( day );
	send_buff[7] = dow;
	send_buff[8] = BIN2BCD( mon );
	send_buff[9] = BIN2BCD( yr );
	
	I2C_writeBlock( PCF8563_ADDR, send_buff, sizeof( send_buff ) );
}

static void PCF8563_readTimeDate( void ) {
	uint8_t recv_buff[7];
	
	I2C_readCmd_Block( PCF8563_ADDR, PCF8563_Seconds, recv_buff, sizeof( recv_buff ));
	
	Watch.seconds	= BCD2BIN( recv_buff[0] & 0x7f );
	Watch.minutes	= BCD2BIN( recv_buff[1] & 0x7f );
	Watch.hours		= BCD2BIN( recv_buff[2] & 0x3f );
	Watch.days		= BCD2BIN( recv_buff[3] & 0x3f );
	Watch.weekdays	= recv_buff[4] & 0x07;
	Watch.months	= BCD2BIN( recv_buff[5] & 0x1f );
	Watch.years		= BCD2BIN( recv_buff[6] );
}

uint16_t PCF8563_readMinSec( void ) {
	uint16_t minsec;
	
	PCF8563_readTimeDate();
	
	minsec = (BIN2BCD(Watch.minutes) << 8) + BIN2BCD(Watch.seconds);
	
	return minsec; 
}

void PCF8563_readDateStringKR( char * buff ) {
	PCF8563_readTimeDate();
	sprintf( buff, "20%02d-%02d-%02d", Watch.years, Watch.months, Watch.days );
}

void PCF8563_readDateStringUS( char * buff ) {
	PCF8563_readTimeDate();
	sprintf( buff, "%02d-%02d-20%02d", Watch.days, Watch.months, Watch.years );
}

void PCF8563_readTimeString( char buff[], bool ap ) {
	uint8_t	am;
	bool pmFlag = false;
	
	PCF8563_readTimeDate();
	am = Watch.hours;
	if ( ap && ( am > 12 ) ) {
		 pmFlag = true;
		 am -= 12;
	}
	
	sprintf( buff, "%02d:%02d:%02d", am, Watch.minutes, Watch.seconds );
	if ( ap ) {
		buff[8] = ( pmFlag )? 'p' : 'a';
		buff[9] = 'm';
		buff[10] = 0;
	}
}

void PCF8563_readDayOfWeek( char* buff, bool b ) {
	char *dw;
	
	PCF8563_readTimeDate();
	dw = ( b )? DAY_OF_WEEK_LONG[Watch.weekdays] : DAY_OF_WEEK_SHORT[Watch.weekdays];
	
	while ( *dw ) *buff++ = *dw++;
	*buff = 0;
}