/*
 * rotarysw.c
 *
 * Created: 2021-09-27 오후 5:04:06
 *  Author: Tae Min Shin
 */ 

#include <avr/io.h>
#include <stdbool.h>

#include "rotarysw.h"

#define ROTARY_KEY	PIN1_bm
#define ROTARY_S1	PIN2_bm
#define ROTARY_S2	PIN3_bm

extern volatile bool gSwFlag;
extern volatile bool gSwSecFlag;
extern volatile bool gRotFlag;
extern volatile int8_t btCounter;

void InitializeRotarySW( void ) {
	PORTE.DIRCLR =  ROTARY_KEY | ROTARY_S1 | ROTARY_S2;
}

bool getRotarySW( void ) {
	return (PORTE.IN & ROTARY_KEY)? false : true;
}

typedef enum { rswILDE, rswPRESSING, rswPRESSED, rswRELEASING } RotarySwState_t;
	
void ScanRotarySwISR( void ) {		// in ISR
	static RotarySwState_t RotaryState = rswILDE;
	static uint16_t autoRepeatCtr1;
	
	switch ( RotaryState ) {
		case rswILDE :
			if ( getRotarySW() ) RotaryState = rswPRESSING;
			break;
		case rswPRESSING :
			if ( getRotarySW() ) {
				gSwFlag = true;
				autoRepeatCtr1 = 100;
				RotaryState = rswPRESSED;
			} else 
				RotaryState = rswILDE;
			break;
		case rswPRESSED :
			if ( !getRotarySW() ) RotaryState = rswRELEASING;
			if ( autoRepeatCtr1 > 0 ) {
				autoRepeatCtr1--;
				if ( autoRepeatCtr1 == 0 ) {
					gSwSecFlag = true;
				}
			}
			break;
		case rswRELEASING :
			RotaryState = ( getRotarySW() )? rswPRESSED : rswILDE;
			break;
		default:
			RotaryState = rswILDE;
			break;
	}
}


typedef enum { S00, S01, S10, S11 } rotaryState_t;
	
uint8_t getRotaryEncoder( void ) {
	return (~PORTE.IN & ( ROTARY_S1 | ROTARY_S2 )) >> 2;
}

void ScanRotaryEncoderISR( void ) {
	uint8_t rotarySw;
	static rotaryState_t  RotaryState;
	static int8_t SwCounter = 0;
	
	rotarySw = getRotaryEncoder();
	switch( RotaryState ) {
		case S00 :		// detent position
			if ( SwCounter == 4 )  { 
				SwCounter = 0;  gRotFlag = true; 
				btCounter++;  if ( btCounter > 9 ) btCounter = 0;
			}
			if ( SwCounter == -4 ) { 
				SwCounter = 0;  gRotFlag = true; 
				btCounter--;  if ( btCounter < 0 ) btCounter = 9;
			}
				
			if ( rotarySw == 0b01 ) { SwCounter++;  RotaryState = S01; }
			if ( rotarySw == 0b10 ) { SwCounter--;  RotaryState = S10; }
			break;
		case S01 :
			if ( rotarySw == 0b11 ) { SwCounter++;  RotaryState = S11; }
			if ( rotarySw == 0b00 ) { SwCounter--;  RotaryState = S00; }
			break;
		case S11 :
			if ( rotarySw == 0b10 ) { SwCounter++;  RotaryState = S10; }
			if ( rotarySw == 0b01 ) { SwCounter--;  RotaryState = S01; }
			break;
		case S10 :
			if ( rotarySw == 0b00 ) { SwCounter++;  RotaryState = S00; }
			if ( rotarySw == 0b11 ) { SwCounter--;  RotaryState = S11; }
			break;
		default:
			RotaryState = S00;
			break;	
	}
	
}