#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>

#include "adc.h"

//#define TEMPERATURE_SENSOR
	
AdcInfo_t AdcBuffer[] = {
	{ AIN1_CDS_SENSOR,   0 },
	{ AIN3_STRAIN_GAUGE, 0 }
};

uint16_t MA_FILTER( uint16_t adc );

extern uint16_t AdcResult;
extern bool	gAdcFlag;
extern enum ch_no  AinNo;

#define	MA_TAP_SIZE		32					// 2^n < 256
#define MA_TAP_MASK		MA_TAP_SIZE - 1

uint16_t MaBuffer[MA_TAP_SIZE] = { 0 };

#ifdef TEMPERATURE_SENSOR
int8_t sigrow_offset;
uint8_t sigrow_gain;
#endif

void InitializeADC( void ) {
	PORTD.PIN1CTRL = PORTD.PIN2CTRL = PORTD.PIN3CTRL = PORT_ISC_INPUT_DISABLE_gc;	// AIN Port Digital domain disconnected
	
#ifdef TEMPERATURE_SENSOR
	// Internal Temperature sensor
	VREF.CTRLA |= VREF_ADC0REFSEL_1V1_gc;  VREF.CTRLB |= VREF_ADC0REFEN_bm;
	ADC0.CTRLC |= ADC_SAMPCAP_bm | ADC_PRESC_DIV8_gc;
	ADC0.MUXPOS = ADC_MUXPOS_TEMPSENSE_gc;
	ADC0.CTRLD |= ADC_INITDLY_DLY32_gc;
	ADC0.SAMPCTRL = 20;
	ADC0.EVCTRL = ADC_STARTEI_bm;
	ADC0.CTRLA |= ADC_ENABLE_bm;
	ADC0.INTCTRL = ADC_RESRDY_bm;

	sigrow_offset = SIGROW.TEMPSENSE1;	// Read signed value from signature row
	sigrow_gain	  = SIGROW.TEMPSENSE0;	// Read unsigned value from signature row
	
	AinNo = AIN_INTERNAL_TEMP;
#else
	// External Sensor
	VREF.CTRLA |= VREF_ADC0REFSEL_2V5_gc;  VREF.CTRLB |= VREF_ADC0REFEN_bm;
	
	ADC0.CTRLC |= ADC_SAMPCAP_bm | ADC_PRESC_DIV8_gc;
	ADC0.MUXPOS = AdcBuffer[AIN_CDS_SENSOR].chan;
	ADC0.EVCTRL = ADC_STARTEI_bm;
	ADC0.CTRLA |= ADC_ENABLE_bm;
	ADC0.INTCTRL = ADC_RESRDY_bm;
#endif	
}

ISR( ADC0_RESRDY_vect ) {
#ifdef TEMPERATURE_SENSOR
	uint16_t adc_reading;   // ADC conversion result with 1.1 V internal reference
	uint32_t temp;
	
	adc_reading = ADC0.RES;
	temp = adc_reading - sigrow_offset;	
	temp *= sigrow_gain;	// Result might overflow 16 bit variable (10bit+8bit)
	temp += 0x80;			// Add 1/2 to get correct rounding on division below
	temp >>= 8;				// Divide result to get Kelvin
	AdcResult = temp - 272;	
#else
	static uint8_t ch_idx = 0;
	
	if ( ch_idx == AIN_CDS_SENSOR ) AdcBuffer[ch_idx++].value = MA_FILTER(ADC0.RES);
	else AdcBuffer[ch_idx++].value = ADC0.RES;
	
	if ( ch_idx >= sizeof(AdcBuffer)/sizeof(AdcInfo_t) ) ch_idx = 0;
	
	ADC0.MUXPOS = AdcBuffer[ch_idx].chan;
#endif	
	
	gAdcFlag = true;
}

uint16_t MA_FILTER( uint16_t adc ) {
	static uint8_t tapIndex = 0;
	static int32_t sumMA = 0;
	
	sumMA += (int32_t)adc; sumMA -= (int32_t)MaBuffer[tapIndex];
	MaBuffer[tapIndex++] = adc;  tapIndex &= MA_TAP_MASK;
	
	return (uint16_t)(sumMA/MA_TAP_SIZE);
}