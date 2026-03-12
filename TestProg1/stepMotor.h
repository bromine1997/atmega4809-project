/*
 * stepMotor.h
 *
 * Created: 2021-11-30 오후 4:57:56
 *  Author: Tae Min Shin
 */ 


#ifndef STEPMOTOR_H_
#define STEPMOTOR_H_

#include <avr/io.h>

#define		L_PULSE		PIN0_bm
#define		L_DIR		PIN2_bm

#define		R_PULSE		PIN1_bm
#define		R_DIR		PIN3_bm

void InitializeStepMotor( void );
void setupMotor( void );
bool planTrajec( long npulses );

#endif /* STEPMOTOR_H_ */