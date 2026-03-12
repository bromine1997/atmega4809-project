/*
 * TestProg1.c
 *
 * Created: 2021-09-06 오후 4:44:25
 * Author : Tae Min Shin
 */ 
#define F_CPU	5000000UL		// Max System Clock Frequency at 1.8V ~ 5.5V VDD

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/sfr_defs.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "rotarysw.h"
#include "uart.h"
#include "adc.h"
#include "twi.h"
#include "ds1621.h"
#include "PCF8563.h"
#include "spi.h"
#include "D24FC512.h"
#include "stepMotor.h"

void ScanButtonISR( void );
void Seg7DisplayISR( uint16_t );
void putty_process( char ptbuff[], uint8_t ptIdx );
void PigGameISR( void );
void PigGameAutoISR( void );
void InitPigGame( void );
float kalman_filter( float z_k );

#define USER_LED	PIN5_bm
#define USER_BUTTON	PIN6_bm	
#define DIGIT4		PORTF.OUT
#define SEGMENT		PORTC.OUT

typedef enum blink { blinkNONE, blinkFAST, blinkMEDIUM, blinkSLOW } blink_t;
typedef enum onoff { OFF, ON } onoff_t;
	
extern bool	stop;

volatile bool gBtFlag		= false;
volatile bool g7SegFlag		= true;
volatile bool g1SecFlag		= true;
volatile bool gSwFlag		= false;
volatile bool gSwSecFlag	= false;
volatile bool gRotFlag		= false;
volatile bool gAdcFlag		= false;
volatile bool gkswFlag		= false;
volatile bool gPigFlag		= false;
volatile bool gEchoFlag		= false;

volatile uint8_t  gkswScanCode;
volatile int8_t btCounter = 0;
volatile uint16_t AdcResult;
volatile uint16_t EchoCounter = 0;

uint16_t CountEchoCapture = 0;

extern AdcInfo_t AdcBuffer[];

struct bit_attri {
	uint8_t		BLANK : 1;
	uint8_t		BLINK : 2;
	uint8_t		POINT : 1;
};

struct {
	bool	FP, CP;
	bool	P1, P2;
	uint8_t	DIE, SUR;
	uint8_t TR1, TR2;
	
	bool	ROL, HOLD, NEW_GAME;	// Input SW	
	bool	WIN;
} PigData;

typedef struct seg_data {
	uint8_t segmentPattern;
	union {
		uint8_t	byteAttribute;
		struct bit_attri bitAttribute;
	} SegmentAttribute;	
} segment_t;

volatile segment_t segmentData[4] = { 0 };
//										0	  1     2     3     4     5     6     7     8     9     a     b     c     d     e     f     -
const uint8_t	Num7segPattern[] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x27, 0x7f, 0x67, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71, 0x40 };
const uint8_t	KeyLookupTable[] = {  1, 2, 3, 'A', 4, 5, 6, 'B', 7, 8, 9, 'C', '*', 0, '#', 'D' };

const uint16_t Sec1CntValue = 1000;

uint16_t btValue = 1000;
enum ch_no  AinNo = AIN_CDS_SENSOR;
uint16_t BatchNo = 0;

uint16_t NumberValue = 0;

enum pigmode { PigNONE, Pig1Auto, Pig2Player } Pig = PigNONE;
typedef enum { Player1, Player2 } player_t;


// PIT 1000Hz(1ms)
ISR(TCB0_INT_vect) {
	static uint16_t Counter1ms = 0;
	static uint16_t Counter1sec = Sec1CntValue;
	uint16_t c1ms;
	
	Counter1ms++;  c1ms = Counter1ms % 5;
	
	if ( c1ms == 0 ) ScanButtonISR();
	if ( c1ms == 1 ) Seg7DisplayISR( Counter1ms/5 );
	if ( c1ms == 2 ) ScanRotarySwISR();
	if ( c1ms == 3 ) ScanKeySwISR();  else  SendLCDbySpiISR();		// mutual exclusive running
//	if ( c1ms == 4 ) ADC0.COMMAND = ADC_STCONV_bm;					// Fs = 200Hz
	
	ScanRotaryEncoderISR();
	
	if ( Pig == Pig2Player ) PigGameISR();
	if ( Pig == Pig1Auto )   PigGameAutoISR();
	
	Counter1sec--;
	if ( Counter1sec == 0 ) {
		g1SecFlag = true;  Counter1sec = Sec1CntValue;
	}
	
	TCB0.INTFLAGS = TCB_CAPT_bm;
}


void Bin2BcdBuff( uint16_t bin, uint8_t bcd[] ) {
	bcd[0] = bin/1000;
	bcd[1] = (bin%1000)/100;
	bcd[2] = (bin%100)/10;
	bcd[3] = bin%10;
}

void BcdDisplayFND( uint8_t bcd[] ) {
	segmentData[0].segmentPattern = Num7segPattern[bcd[0]];
	segmentData[1].segmentPattern = Num7segPattern[bcd[1]];
	segmentData[2].segmentPattern = Num7segPattern[bcd[2]];
	segmentData[3].segmentPattern = Num7segPattern[bcd[3]];
}

void Uint16DisplayFND( uint16_t bcd ) {
	segmentData[0].segmentPattern = Num7segPattern[bcd>>12 & 0x0f];
	segmentData[1].segmentPattern = Num7segPattern[bcd>> 8 & 0x0f];
	segmentData[2].segmentPattern = Num7segPattern[bcd>> 4 & 0x0f];
	segmentData[3].segmentPattern = Num7segPattern[bcd & 0x0f];
}

void Bin2HexDisplayFND( uint16_t bin ) {
	segmentData[0].segmentPattern = Num7segPattern[bin >> 12];
	segmentData[1].segmentPattern = Num7segPattern[(bin >> 8) & 0x0f];
	segmentData[2].segmentPattern = Num7segPattern[(bin >> 4) & 0x0f];
	segmentData[3].segmentPattern = Num7segPattern[bin & 0x0f];
}


void Pig2DecDisplayFND( uint16_t bin ) {
	uint8_t SegPatternTmp[4];
	
	SegPatternTmp[0] = ( (Pig == Pig1Auto) && PigData.CP )? Num7segPattern[0x0c] : Num7segPattern[bin / 1000];
	SegPatternTmp[1] = Num7segPattern[(bin%1000)/100];
	SegPatternTmp[2] = Num7segPattern[(bin%100)/10];
	SegPatternTmp[3] = Num7segPattern[bin%10];

	if ( SegPatternTmp[2] == 0x3f ) SegPatternTmp[2] = 0x00;

	for ( uint8_t i = 0 ; i < 4 ; i++ )
		segmentData[i].segmentPattern = SegPatternTmp[i];
}


void Bin2DecDisplayFND( uint16_t bin ) {
	uint8_t SegPatternTmp[4];
	
	if ( bin > 9999 ) {
		segmentData[0].segmentPattern = segmentData[1].segmentPattern = segmentData[2].segmentPattern = segmentData[3].segmentPattern = 0x40;
		return;
	}
	SegPatternTmp[0] = Num7segPattern[bin/1000];
	SegPatternTmp[1] = Num7segPattern[(bin%1000)/100];
	SegPatternTmp[2] = Num7segPattern[(bin%100)/10];
	SegPatternTmp[3] = Num7segPattern[bin%10];
	for ( uint8_t i = 0 ; i < 3 ; i++ ){
		if ( SegPatternTmp[i] == 0x3f ) SegPatternTmp[i] = 0x00;
		else break;
	}
	for ( uint8_t i = 0 ; i < 4 ; i++ )
		segmentData[i].segmentPattern = SegPatternTmp[i];
}

void Tmp2DecDisplayFND( int16_t bin ) {		//  -55 < (bin/256) < 125
	uint8_t SegPatternTmp[4];

	if ( bin < 0 ) {
		bin = -bin;
		bin = (bin >> 8) * 10 + ((bin & 0x0080)? 5 : 0);
		SegPatternTmp[0] = 0x40;		// '-'
		SegPatternTmp[1] = Num7segPattern[(bin%1000)/100];
		SegPatternTmp[2] = Num7segPattern[(bin%100)/10];
		SegPatternTmp[3] = Num7segPattern[bin%10];		
		if ( SegPatternTmp[1] == 0x3f ) SegPatternTmp[1] = 0x00;
	} else {
		bin = (bin >> 8) * 10 + ((bin & 0x0080)? 5 : 0);
		SegPatternTmp[0] = Num7segPattern[bin/1000];
		SegPatternTmp[1] = Num7segPattern[(bin%1000)/100];
		SegPatternTmp[2] = Num7segPattern[(bin%100)/10];
		SegPatternTmp[3] = Num7segPattern[bin%10];	
		for ( uint8_t i = 0 ; i < 2 ; i++ ){
			if ( SegPatternTmp[i] == 0x3f ) SegPatternTmp[i] = 0x00;
			else break;
		}
	}
	for ( uint8_t i = 0 ; i < 4 ; i++ )
		segmentData[i].segmentPattern = SegPatternTmp[i];
}

//
//
void ClearAttribute( void ) {
	for ( int i = 0 ; i < 4 ; i++ )
		segmentData[i].SegmentAttribute.byteAttribute = 0x00;
}

void FndTest( void ) {
	for ( uint8_t i = 0 ; i < 4 ; i++ )
		segmentData[i].segmentPattern = 0xff;
}

void FndClear( void ) {
	for ( uint8_t i = 0 ; i < 4 ; i++ )
		segmentData[i].segmentPattern = 0x00;
	
	ClearAttribute();
}

void BlinkDisplay( blink_t bt ) {
	segmentData[0].SegmentAttribute.bitAttribute.BLINK = segmentData[1].SegmentAttribute.bitAttribute.BLINK = segmentData[2].SegmentAttribute.bitAttribute.BLINK = segmentData[3].SegmentAttribute.bitAttribute.BLINK = bt;
}

void BlinkDigitDisplay( uint8_t dt, blink_t bt ) {
	BlinkDisplay( blinkNONE );
	segmentData[dt].SegmentAttribute.bitAttribute.BLINK = bt;
}

void BlankDisplay( void ) {
	segmentData[0].SegmentAttribute.bitAttribute.BLANK = segmentData[1].SegmentAttribute.bitAttribute.BLANK = segmentData[2].SegmentAttribute.bitAttribute.BLANK = segmentData[3].SegmentAttribute.bitAttribute.BLANK = true;
}

void BlankDigitDisplay( uint8_t dt, onoff_t of ) {
	segmentData[dt].SegmentAttribute.bitAttribute.BLANK = of;
}

void PointDigitDisplay( uint8_t dt, onoff_t of ) {
	segmentData[dt].SegmentAttribute.bitAttribute.POINT = of;
}

enum fnd_mode  { FND_UserButton, FND_AdcValue, FND_Temper, FND_Date, FND_KeySw, FND_PigGame, FND_ECHO, FND_End } FND_MODE = FND_UserButton;
enum clcd_mode { AUTO_CMODE, BAR_CMODE, ADC_CMODE, TIME_CMODE, PIG_CMODE, ECHO_CMODE, NONE_CMODE } CLCD_MODE = ECHO_CMODE;
//
//

int main(void)
{
	uint8_t	 digit_idx = 3;
	uint8_t	 DigitBcdBuff[4] = { 0 };
	bool EncoderEnable = false;
	
	char  puttyData;
	char  puttyBuffer[32] = { 0 };
	uint8_t	 puttyIndex = 0;
	int16_t	temp;
	
	char	buff1[16], buff2[16];
	float	DistanceRaw, DistanceFiltered;
	
	union {
		uint8_t		cbuffer[530];
		uint16_t	ibuffer[530/2];
	} EEPROM_Buffer;
		
	// Main CLK 5MHz
	CCP = CCP_IOREG_gc;
	CLKCTRL.MCLKCTRLB = CLKCTRL_PDIV_4X_gc | CLKCTRL_PEN_bm;
	
	// USER_LED Initialization
	PORTF.DIRSET = USER_LED;								// PF5:USER_LED Output Mode
	PORTF.OUTSET = USER_LED;								// PF5:USER_LED LED Off
	// USER_BUTTON Initialization
	PORTF.DIRCLR = USER_BUTTON;								// PF6:USER_BUTTON Input Mode
	PORTF.PIN6CTRL = PORT_PULLUPEN_bm;
	
	// Port Initialization for 7-Segment LED display
	PORTC.DIR = 0xff;										// PORTC All Output mode : Segment pattern
	PORTF.DIR |= 0x0f;										// PORTF 0~3 Output mode : Digit select 
	
	// PF5 <-- WO5
	PORTMUX.TCAROUTEA = PORTMUX_TCA0_PORTF_gc;
	// TCA0 for Trig of HC-SR04
	TCA0.SPLIT.CTRLD = TCA_SPLIT_ENABLE_bm;
	TCA0.SPLIT.HPER = 243;									// 20Hz frequency N : 1024 fCLK_PER = 5M
	TCA0.SPLIT.CTRLB = TCA_SPLIT_HCMP2EN_bm;				// WG Mode SS-PWM
	TCA0.SPLIT.HCMP2 = 10;
	TCA0.SPLIT.CTRLA = TCA_SPLIT_CLKSEL_DIV1024_gc | TCA_SPLIT_ENABLE_bm;
	//
	// TCB1 for stepMotor 10KHz
	TCB1.CCMP = 500;										// 5MHz/10000Hz(0.1ms) = 500
	TCB1.CTRLA |= TCB_ENABLE_bm;							// TCB1 ENABLE
	TCB1.INTCTRL = TCB_CAPT_bm;								// Local CAPT Int Enable	
	//
	// TCB2 for Width Measurement of ECHO Pulse
	TCB2.CTRLB |= TCB_CNTMODE_PW_gc;
	TCB2.EVCTRL |= TCB_CAPTEI_bm;
	TCB2.CTRLA |= TCB_CLKSEL_CLKDIV2_gc | TCB_ENABLE_bm;
	TCB2.INTCTRL = TCB_CAPT_bm;
	// 
	EVSYS.CHANNEL3 = EVSYS_GENERATOR_PORT1_PIN0_gc;			// Ch3 Source <- PD0 : ECHO 
	EVSYS.USERTCB2 = EVSYS_CHANNEL_CHANNEL3_gc;
	
	//
	// TCB3 for Start of Conversion of ADC
	TCB3.CCMP =	25000;										//	200Hz = 5000000/25000
	TCB3.EVCTRL |= TCB_CAPTEI_bm;
	TCB3.CTRLA |= TCB_ENABLE_bm;
	//
	EVSYS.CHANNEL0 = EVSYS_GENERATOR_TCB3_CMP0_gc;
	EVSYS.USERADC0 = EVSYS_CHANNEL_CHANNEL0_gc;
	
	InitializeRotarySW();
	InitializeUsart0( 115200L );
	InitializeADC();
	InitializeTWI();
	InitializeSPI();	
	InitializeStepMotor();
	//
	sei();													// Global Interrupt Enable
	
	InitializeIOX();
	  	
	// TCB0 Initialization : PIT 1000Hz
	TCB0.CCMP = 5000;										// 5MHz/1000Hz(1ms) = 5000
	TCB0.CTRLA |= TCB_ENABLE_bm;							// TCB0 ENABLE
	TCB0.INTCTRL = TCB_CAPT_bm;								// Local CAPT Int Enable
	
	InitializeIOX_CLCD();

	DS1621_StartConverT();
	PCF8563_wrieTimeDate( 16, 45, 0, 21, 11, 2, 2 );
	
	FndTest();  
	if ( EEPROM_ReadUint16( 20 ) != 0x5a5a ) {
		IOX_CLCD_DisplayString( 0, 0, "Hello ATMEGA4809");
		_delay_ms(1000);
		EEPROM_WriteUint16( 20, 0x5a5a );		// 0b01011010
	} else {
		IOX_CLCD_DisplayString( 0, 1, "Hello again!!");
		_delay_ms(400);
	}
	for ( uint16_t i = 0 ; i < 530/2 ; i++ ) EEPROM_Buffer.ibuffer[i] = i*2;
	EEPROM_WriteAnyBlock( 1000, EEPROM_Buffer.cbuffer, 530 );
									
	FndClear();  IOX_CLCD_ClearDisplay();
	if ( FND_MODE == FND_UserButton ) Bin2DecDisplayFND(EEPROM_ReadUint16( btValue ));
	
    /* Replace with your application code */
	printf("Welcome to PC!\nEnter 1-line command (? : Display command list)\n\n");
	
    while (1) 
    {
		if ( g1SecFlag ) {
			g1SecFlag = false;
			temp = (int16_t)DS1621_ReadTemperature();		//0xf080(-15.5)
			if ( FND_MODE == FND_Temper ) {
				Tmp2DecDisplayFND( temp );	PointDigitDisplay( 2, ON );
			}
			if ( FND_MODE == FND_Date ) {
				Uint16DisplayFND(PCF8563_readMinSec());  PointDigitDisplay( 1, ON );				
			}
		}
		
		////
		if ( gBtFlag ) {
			gBtFlag = false;
			if ( !EncoderEnable ) { 
				if ( FND_MODE == FND_UserButton ) Bin2DecDisplayFND(EEPROM_ReadUint16( btValue ));
				btValue += 2;
			}
		}
		
		////
		if ( gSwFlag ) {
			gSwFlag = false;
			if ( EncoderEnable ) {
				digit_idx++;  if ( digit_idx > 3 ) digit_idx = 0;
				btCounter = DigitBcdBuff[digit_idx];
			
				if ( FND_MODE == FND_UserButton ) {
					PointDigitDisplay( 0, OFF );  PointDigitDisplay( 1, OFF );  PointDigitDisplay( 2, OFF );  PointDigitDisplay( 3, OFF );
					PointDigitDisplay( digit_idx, ON );	
				}
			}	
		}
		if ( gSwSecFlag ) {
			gSwSecFlag = false;
			
			EncoderEnable = ( EncoderEnable )? false : true;
			if ( EncoderEnable ) {
				digit_idx = 3;  gSwFlag = true;  
				Bin2BcdBuff( btValue, DigitBcdBuff);  if ( FND_MODE == FND_UserButton ) BcdDisplayFND( DigitBcdBuff );
			} else {
				btValue = DigitBcdBuff[0]*1000 + DigitBcdBuff[1] * 100 + DigitBcdBuff[2] * 10 + DigitBcdBuff[3];
				if ( FND_MODE == FND_UserButton ) {
					PointDigitDisplay( 0, OFF );  PointDigitDisplay( 1, OFF );  PointDigitDisplay( 2, OFF );  PointDigitDisplay( 3, OFF );
					Bin2DecDisplayFND( btValue );									
				}
			}
		}
		if ( gRotFlag ) {
			gRotFlag = false;
			if ( EncoderEnable ) {
				DigitBcdBuff[digit_idx] = btCounter;
				if ( FND_MODE == FND_UserButton ) BcdDisplayFND( DigitBcdBuff );				
			}		
		}
		
		////
		if ( USART0_CheckRxData() ) {
			puttyData = getchar();  
			if ( puttyData == '\010' ) {		// Putty Setting : backspace code ^H('\010')
				if ( puttyIndex > 0 ) {
					putchar( puttyData );  putchar(' ');  putchar('\010');  puttyIndex--;
				}
			} else {
				putchar( puttyData );  puttyBuffer[puttyIndex++] = puttyData;
			}
			if ( puttyData == '\015' ) {		// Enter 0x0d
				putchar( '\012' );				// CR    0x0a
				puttyBuffer[puttyIndex] = '\000';
				puttyIndex = 0;  while ( puttyBuffer[puttyIndex] == ' ' ) puttyIndex++;
				
				putty_process( puttyBuffer, puttyIndex);
				
				puttyIndex = 0;
			}
				  		
		}  // if usart
		
		////
		if ( gAdcFlag ) {
			gAdcFlag = false;
			switch ( AinNo ) {
				case AIN_CDS_SENSOR :
					AdcResult = AdcBuffer[AIN_CDS_SENSOR].value;
					break;
				case AIN_STRAIN_GAUGE :
					AdcResult = AdcBuffer[AIN_STRAIN_GAUGE].value;
					break;
					
				case AIN_INTERNAL_TEMP :
					break;
				default:
					AdcResult = 10000;
					break;
			}
			if ( FND_MODE == FND_AdcValue ) Bin2DecDisplayFND( AdcResult ); 
			if ( BatchNo > 0 ) {
				BatchNo--;	printf("%d\n", AdcResult);
			}
		}
		
		////
		if ( gkswFlag ) {
			gkswFlag = false;
			temp = KeyLookupTable[gkswScanCode];
			if ( temp < 10 ) {	// Numeric key
				NumberValue %= 1000;
				NumberValue = NumberValue * 10 + temp;		// 0 ~ 9999
			} else {			// Function key
				switch ( temp ) {
					case 'B' :
						if ( stop ) { 
							planTrajec(1200); stop = false; 
						}
						break;
					case 'A' :
						if ( stop ) { 
							planTrajec(-1200); stop = false; 
						}
						break;					
					case 'C' :
						FND_MODE = FND_ECHO;
						CLCD_MODE = ECHO_CMODE;
						IOX_CLCD_ClearDisplay();
						Bin2HexDisplayFND( EchoCounter );	
						break;				
					case 'D' :
						Pig = ( Pig == Pig2Player )? Pig1Auto : Pig2Player;
						InitPigGame(); gPigFlag = true;
						FND_MODE = FND_PigGame;  CLCD_MODE = PIG_CMODE;
						BlinkDisplay( blinkNONE );
						break;					
					case '*' :
						if ( !Pig ) NumberValue = 0;
						break;
					case '#' :
						if ( !Pig ) FND_MODE = FND_AdcValue;
						break;
					default:	break;				
				}
			}
			if ( FND_MODE == FND_KeySw ) Bin2DecDisplayFND( NumberValue );					
		}
		
		////
		if ( Pig && gPigFlag ) {
			gPigFlag = false;
			if ( CLCD_MODE == PIG_CMODE ) {
				sprintf( buff1, "Player 1:%3d", PigData.TR1 );  IOX_CLCD_DisplayString( 0, 0, buff1 );
				if ( Pig == Pig1Auto ) {
					sprintf( buff1, "Computer:%3d", PigData.TR2 );  IOX_CLCD_DisplayString( 1, 0, buff1 );
				} else {
					sprintf( buff1, "Player 2:%3d", PigData.TR2 );  IOX_CLCD_DisplayString( 1, 0, buff1 );
				}
					
				if ( PigData.WIN )  {
					IOX_CLCD_DisplayString( (uint8_t)PigData.CP, 12, " WIN" );
					IOX_CLCD_DisplayString( (uint8_t)(!PigData.CP), 12, "    " );
				} else {
					IOX_CLCD_DisplayString( (uint8_t)PigData.CP, 12, "    " );
					IOX_CLCD_DisplayString( (uint8_t)(!PigData.CP), 12, "    " );				
				}
			}

			if ( FND_MODE == FND_PigGame ) {
				temp = ((uint16_t)PigData.CP + 1) * 1000 + PigData.DIE * 100 + PigData.SUR;
				Pig2DecDisplayFND( temp );  PointDigitDisplay( 0, ON );
				BlinkDigitDisplay( 0, ( PigData.WIN )? blinkMEDIUM : blinkNONE );
				BlinkDigitDisplay( 1, ( PigData.WIN )? blinkNONE : blinkMEDIUM );
				BlankDigitDisplay( 1, ( PigData.ROL || PigData.WIN )? true : false );
			}
		}
		
		///
		if ( gEchoFlag ) {
			gEchoFlag = false;
			
			DistanceRaw = (float)EchoCounter * 0.4 / 58.0;
			DistanceFiltered = kalman_filter(DistanceRaw);
			if ( FND_MODE == FND_ECHO ) {
				Bin2DecDisplayFND( (uint16_t)(DistanceFiltered * 10 + 0.5) );  PointDigitDisplay( 2, ON );
			}
			if ( CLCD_MODE == ECHO_CMODE ) {
				sprintf(  buff1, "%8.3f", DistanceRaw );	   IOX_CLCD_DisplayString( 0, 1, buff1 );
				sprintf(  buff2, "%8.3f", DistanceFiltered );  IOX_CLCD_DisplayString( 1, 1, buff2 );
				if ( CountEchoCapture ) {
					printf("%s %s\n", buff1, buff2 );
					CountEchoCapture--;
				}
			}
		}
		
		///

	}	// while
}


bool getButton( void ) {
	return (PORTF.IN & USER_BUTTON)? false : true;
}

typedef enum { btILDE, btPRESSING, btPRESSED, btAUTOREPEAT, btRELEASING } ButtonState_t;
	
void ScanButtonISR( void ) {		// in ISR
	static ButtonState_t ButtonState = btILDE;
	static uint16_t autoRepeatCtr1, autoRepeatCtr2, ctr20;
	
	switch ( ButtonState ) {
		case btILDE :
			if ( getButton() ) ButtonState = btPRESSING;
			break;
		case btPRESSING :
			if ( getButton() ) {
				gBtFlag = true;
				autoRepeatCtr1 = 200;
				ButtonState = btPRESSED;
			} else 
				ButtonState = btILDE;
			break;
		case btPRESSED :
			if ( !getButton() ) ButtonState = btRELEASING;
			if ( autoRepeatCtr1 > 0 ) {
				autoRepeatCtr1--;
				if ( autoRepeatCtr1 == 0 ) {
					autoRepeatCtr2 = ctr20 = 20;
					ButtonState = btAUTOREPEAT;
				}
			}
			break;
		case btAUTOREPEAT :
			if ( !getButton() ) ButtonState = btRELEASING;
			if ( autoRepeatCtr2 > 0 ) {
				autoRepeatCtr2--;
				if ( autoRepeatCtr2 == 0 ) {
					gBtFlag = true;
					if ( ctr20 > 1 ) ctr20--;
					autoRepeatCtr2 = ctr20;
				}
			}
			break;
		case btRELEASING :
			ButtonState = ( getButton() )? btPRESSED : btILDE;
			break;
		default:
			ButtonState = btILDE;
			break;
	}
}

void Seg7DisplayISR( uint16_t idx ) {
	uint8_t segtmp;
	
	segtmp = segmentData[idx & 0x03].segmentPattern;
	if ( segmentData[idx & 0x03].SegmentAttribute.bitAttribute.POINT ) segtmp |= 0x80;
	if ( segmentData[idx & 0x03].SegmentAttribute.bitAttribute.BLANK ) segtmp = 0x00;
	switch ( segmentData[idx & 0x03].SegmentAttribute.bitAttribute.BLINK ) {
		case blinkNONE : break;
		case blinkFAST   : segtmp = ( idx & 0x0008 )? segtmp : 0;  break;
		case blinkMEDIUM : segtmp = ( idx & 0x0040 )? segtmp : 0;  break;
		case blinkSLOW   : segtmp = ( idx & 0x0100 )? segtmp : 0;  break;
	}
	PORTF.OUT &= 0xf0;					// DIGIT ALL CLEAR
	PORTC.OUT = segtmp;
	PORTF.OUT |= 1 << (idx & 0x03);
}

void putty_process( char ptbuff[], uint8_t ptIdx ) {
	char buffer[32];
	
	switch ( ptbuff[ptIdx++] ) {
		case 'A' : case 'a' :
			FND_MODE = FND_AdcValue;
			sscanf( &ptbuff[ptIdx], "%d", (int *)&AinNo );
			break;
		case 'B' : case 'b' :
			FND_MODE = FND_UserButton;
			if ( ptbuff[ptIdx] == '\015' ) {
				printf("%4d\n", btValue );
			} else {
				sscanf( &ptbuff[ptIdx], "%d", &btValue );
			}
			if ( FND_MODE == FND_UserButton ) Bin2DecDisplayFND( btValue );
			break;
		case 'C' : case 'c' :
			sscanf( &ptbuff[ptIdx], "%d", &BatchNo );
			break;
		case 'T' :
		case 't' :
			switch ( ptbuff[ptIdx] ) {
				case '\015' :
					PCF8563_readDateStringKR( buffer );			printf( "%s ", buffer );
					PCF8563_readTimeString( buffer, false );	printf( "%s ", buffer );
					PCF8563_readDayOfWeek( buffer, true );		printf( "%s\n", buffer );
					break;
				case '1' :
					PCF8563_readTimeString( buffer, false );	printf( "%s\n", buffer );
					break;
				case '2' :
					PCF8563_readTimeString( buffer, true );		printf( "%s\n", buffer );
					break;
				case '3' :
					PCF8563_readDateStringKR( buffer );			printf( "%s\n", buffer );
					break;
				case '4' :
					PCF8563_readDateStringUS( buffer );			printf( "%s\n", buffer );
					break;
				case '5' :
					PCF8563_readDayOfWeek( buffer, false );		printf( "%s\n", buffer );
					break;
				default:
					break;			
			}
			break;
		case 'e' :
		case 'E' :
			sscanf( &ptbuff[ptIdx], "%d", &CountEchoCapture );
			break;
		case '?' :
			printf(" Ann   : Set nn to AinNo\n");
			printf(" B     : Display btValue\n");
			printf(" Bnnnn : Set nnnn to btValue\n");
			printf(" Cnnnn : Set nnnn to Sensor value Number\n");
			printf(" Ennnn : Display nnnn Echo Distances\n" );
			printf(" Tn    : Display Time\n");	// "" : All, "1' : Time, "2" : Time AP, "3" : Date KR, "4" : Date US 
			break;
		//
	}
}

typedef enum { pig_INIT, pig_BEGIN, pig_ROL, pig_ONE, pig_ROH, pig_TEST, pig_WIN } pig_state_t;
	
void PigGameISR( void ) {
	static pig_state_t PigState = pig_INIT;
	
	switch ( PigState ) {
		case pig_INIT :
			PigData.TR1 = PigData.TR2 = 0;
			PigData.CP = PigData.FP;
			PigData.WIN = false;  gPigFlag = true;
			PigState = pig_BEGIN;
			break;
		case pig_BEGIN :
			PigData.SUR = 0;
			if ( PigData.ROL ) { PigState = pig_ROL; gPigFlag = true; }
			break;
		case pig_ROL :
			PigData.DIE = rand()%6 + 1;
			if ( !PigData.ROL ) PigState = pig_ONE;
			break;
		case pig_ONE :
			if ( PigData.DIE == 1 ) {
				PigData.CP = !PigData.CP;  PigData.SUR = 0;
				PigState = pig_BEGIN;
			} else {
				PigData.SUR += PigData.DIE;
				PigState = pig_ROH;
			}
			gPigFlag = true;
			break;
		case pig_ROH :
			if ( PigData.ROL ) PigState = pig_ROL;
			else {
				if ( PigData.HOLD ) {
					if ( PigData.CP ) PigData.TR2 += PigData.SUR;
					else			  PigData.TR1 += PigData.SUR;
					PigState = pig_TEST;
				}
				gPigFlag = true;
			}
			break;
		case pig_TEST :
			if ( PigData.CP == Player2 ) {
				if ( PigData.TR2 < 100 )  { PigData.CP = !PigData.CP; PigState = pig_BEGIN; }
				if ( PigData.TR2 >= 100 ) PigState = pig_WIN;
			} else {
				if ( PigData.TR1 < 100 )  { PigData.CP = !PigData.CP; PigState = pig_BEGIN; }
				if ( PigData.TR1 >= 100 ) PigState = pig_WIN;
			}
			break;
		case pig_WIN :
			PigData.WIN = true;
			if ( PigData.CP == Player2 ) { PigData.P2 = true; PigData.P1 = false; }
			else                         { PigData.P1 = true; PigData.P2 = false; }
			
			if ( PigData.NEW_GAME ) {
				PigData.FP = !PigData.FP;
				PigState = pig_INIT;
				gPigFlag = true;
			}
			break;
		default:
			PigState = pig_INIT;
			break;
	}
}

void PigGameAutoISR( void ) {
	static pig_state_t PigState = pig_INIT;
	static uint16_t HoldCounter;
	
	switch ( PigState ) {
		case pig_INIT :
			PigData.TR1 = PigData.TR2 = PigData.DIE = 0;
			PigData.CP = PigData.FP;
			gPigFlag = true;
			PigState = pig_BEGIN;
			break;
		case pig_BEGIN :
			PigData.SUR = 0;
			if ( PigData.CP == Player2 ) { PigState = pig_ROL;  HoldCounter = 500; }
			else 
				if ( PigData.ROL )		   PigState = pig_ROL;
			break;
		case pig_ROL :
			PigData.DIE = rand()%6 + 1;
			if ( PigData.CP == Player2 ) {
				if ( --HoldCounter == 0 )  { PigState = pig_ONE;  PigData.WIN = false; }
			} else {
				if ( !PigData.ROL ) { PigState = pig_ONE;  PigData.WIN = false; }
			}
			break;
		case pig_ONE :
			if ( PigData.DIE == 1 ) {
				PigData.CP = !PigData.CP;  PigData.SUR = 0;
				PigState = pig_BEGIN;
			} else {
				PigData.SUR += PigData.DIE;
				PigState = pig_ROH;
				if ( PigData.CP == Player2 ) {
					if ( PigData.SUR < 20) { PigState = pig_ROL;  HoldCounter = 500; }
					else 
						HoldCounter = 1500; 
				}
			}
			gPigFlag = true;
			break;
		case pig_ROH :
			if ( PigData.CP == Player2 ) {
				if ( --HoldCounter == 0 ) { PigData.TR2 += PigData.SUR; PigState = pig_TEST; gPigFlag = true; }
			} else {
				if ( PigData.ROL ) PigState = pig_ROL;
				else {
					if ( PigData.HOLD ) { PigData.TR1 += PigData.SUR;  PigState = pig_TEST;	}
				}		
				gPigFlag = true;
			}
			break;
		case pig_TEST :
			if ( PigData.CP == Player2 ) {
				if ( PigData.TR2 < 100 )  { PigData.CP = !PigData.CP; PigState = pig_BEGIN; }
				if ( PigData.TR2 >= 100 ) PigState = pig_WIN;
			} else {
				if ( PigData.TR1 < 100 )  { PigData.CP = !PigData.CP; PigState = pig_BEGIN; }
				if ( PigData.TR1 >= 100 ) PigState = pig_WIN;
			}
			break;
		case pig_WIN :
			PigData.WIN = true;
			if ( PigData.CP == Player2 ) { PigData.P2 = true; PigData.P1 = false; }
			else                         { PigData.P1 = true; PigData.P2 = false; }
			
			if ( PigData.NEW_GAME ) {
				PigData.FP = !PigData.FP;
				PigState = pig_INIT;
				gPigFlag = true;
			}
			break;
		default:
			PigState = pig_INIT;
			break;
	}
}

void InitPigGame( void ) {
	PigData.FP = Player1;		
	PigData.DIE = 0;
	PigData.WIN = false;
}

ISR(TCB2_INT_vect) {
	EchoCounter = TCB2.CCMP;
	gEchoFlag = true;
	
	TCB2.INTFLAGS |= TCB_CAPT_bm;
}

#define		Q	2.5291E-5	// 1e-5	
#define		R	2.5291E-5	// 2.92e-3	

float kalman_filter( float z_k ) {
	static float	x_hat_k_predict	= 0;
	static float	x_hat_k_1		= 0;
	static float	Pk_predict		= 0;
	static float	Pk_1			= 0;
	static float	x_hat_k			= 0;
	static float	Kk	= 0;
	static float	Pk	= 0;

    //prediction
    x_hat_k_predict = x_hat_k_1;
    Pk_predict = Pk_1 + Q;
    
    //update by measurement
    Kk = Pk_predict / (Pk_predict + R);
    x_hat_k = x_hat_k_predict + Kk * (z_k - x_hat_k_predict);
    Pk = (1 - Kk) * Pk_predict;
    
	//time Delay
	x_hat_k_1 = x_hat_k;
	Pk_1 = Pk;
	
    return x_hat_k;
}
