#ifndef DS1621_H_
#define DS1621_H_

#define		DS1621_ADDR		0x48

#define		DS1621_READ_TEMPERATURE	0xAA
#define		DS1621_ACCESS_CONFIG	0xAC
#define		DS1621_START_CONVERT_T	0xEE
#define		DS1621_STOP_CONVERT_T	0x22

void DS1621_StartConverT( void );
void DS1621_StopConverT( void );
uint16_t DS1621_ReadTemperature( void );

#endif /* DS1621_H_ */