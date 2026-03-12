#ifndef ADC_H_
#define ADC_H_

#define AIN1_CDS_SENSOR			ADC_MUXPOS1_bp
#define AIN2_NotConnected		ADC_MUXPOS2_bp
#define AIN3_STRAIN_GAUGE		ADC_MUXPOS3_bp

void InitializeADC( void );

enum ch_no { AIN_CDS_SENSOR, AIN_STRAIN_GAUGE, AIN_INTERNAL_TEMP };
	
typedef struct adc_info {
	uint8_t		chan;
	uint16_t	value;	
} AdcInfo_t;

#endif /* ADC_H_ */