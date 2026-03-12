#ifndef PCF8563_H_
#define PCF8563_H_

#define	PCF8563_ADDR			0x51

#define PCF8563_ControlStatus1	0x00
#define PCF8563_Seconds			0x02

typedef struct PCF8563_REG {
	uint8_t	seconds;
	uint8_t minutes;
	uint8_t hours;
	uint8_t days;
	uint8_t weekdays;
	uint8_t months;
	uint8_t years;	
} CLOCK_t;

void PCF8563_wrieTimeDate( uint8_t hr, uint8_t min, uint8_t sec, uint8_t yr, uint8_t mon, uint8_t day, uint8_t dow );
uint16_t PCF8563_readMinSec( void );
void PCF8563_readDateStringKR( char * buff );
void PCF8563_readDateStringUS( char * buff );
void PCF8563_readTimeString( char buff[], bool ap );
void PCF8563_readDayOfWeek( char* buff, bool b );

#endif /* PCF8563_H_ */