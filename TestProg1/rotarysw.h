/*
 * rotarysw.h
 *
 * Created: 2021-09-27 오후 5:04:31
 *  Author: Tae Min Shin
 */ 


#ifndef ROTARYSW_H_
#define ROTARYSW_H_

void InitializeRotarySW( void );
bool getRotarySW( void );
void ScanRotarySwISR( void );
void ScanRotaryEncoderISR( void );

#endif /* ROTARYSW_H_ */