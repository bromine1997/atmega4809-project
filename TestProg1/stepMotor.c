/*
 * stepMotor.c
 *
 * Created: 2021-11-30 오후 4:57:37
 *  Author: Tae Min Shin
 */ 
#define F_CPU	5000000UL		// Max System Clock Frequency at 1.8V ~ 5.5V VDD

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/sfr_defs.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "stepMotor.h"

#define Km 			400 	//number of steps per one revolution : Half Step (0.9degree)
#define	gearRatio	1
#define F 			10000 	//interrupt frequency 10kHz
//
float 	vs = 5, vM = 50.0, aM = 50.0, dM = 40.0;				// m = aM/dM

float	K, T, m, ta, tv, td, ss, aa, vv;						// a[rad/sec^2], v[rad/sec], s[rad]
double 	alfa, Af, Vsf, Df; 										// Af[steps/Ts^2], Vsf[steps/Ts]
double	vel, pos;
// cN-current number of steps, dN-desired number of steps
long 	N, Na, Nv, Nd, cN, np = -1;

bool 	dir;
bool	stop = true;

void InitializeStepMotor( void ) {
	PORTD.DIRSET = PIN5_bm | PIN6_bm | PIN7_bm;
	PORTB.DIRSET = PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm;
	PORTD.OUTSET = PIN5_bm | PIN6_bm | PIN7_bm;							// PD7 : SM_M0 = 1, PD6 : SM_M1 = 1 ==> Half Mode, PD5 : SM_Enable = 1
	
	setupMotor();
}
//
void setupMotor( void ) {
	K = Km*gearRatio;

	aa = aM;
	vv = vM;
	
	m	 = aM/dM;
	alfa = 2.0*M_PI/K;
	Af   = aM/alfa/F/F;
	Vsf  = vs/alfa/F;
	Df   = -Af/m;
}

// interrupt procedure, every 1/10000 [sec]=1/10[ms] program calls this procedure
ISR(TCB1_INT_vect){
	//
	if ( !stop ) {
		vel += Af;
		pos += vel;
		if ( (pos - np) >= 1.0 )	{
			PORTB.OUTSET = L_PULSE;  _delay_us(4);  PORTB.OUTCLR = L_PULSE;
			np++;
			if ( np >= N )				 stop = true;
			if ( np >= Na && np < N-Nd ) Af = 0.0; 			//speed is constant
			if ( np >= N-Nd ) 			 Af = Df; 			//begin of deceleration
		}		
	}
	TCB1.INTFLAGS = TCB_CAPT_bm;
}


//trajectory planning procedure
bool planTrajec( long npulses ) {
	N = npulses; 																//[steps]
	if ( N == 0 ) { np = -1; return false; } 
	
	dir = ( N >= 0 )? false : true;		// false : CW
	if ( dir ) PORTB.OUTSET = L_DIR;
	else       PORTB.OUTCLR = L_DIR;
	
	N = abs(N);
	float s_ = N*alfa;
	float m1aa = (1.0+m)/aa;
	T = sqrt( (vs*m1aa)*(vs*m1aa) + 2.0*s_*m1aa ) - vs*m1aa; 					//N = round(s/alfa);
	ta = T/(1.0+m); vv = vs + aa*ta; 											//td = (T - ta); tv = 0;
	if ( vv <= vM ) {		//TRIANGULAR PROFILE
		Na = (long)(N/(1+m)); Nd = N - Na; Nv = 0;
	} else { 				//TRAPEZOIDAL PROFILE
		vv = vM; ta = (vM-vs)/aa; T = (1.0+m)*ta; ss = vs*T + aa*T*T/(2*(1+m));
		//ss : is the part of movement in the phases of acceleration and deceleration only
		//np : number of steps on the movement ss
		np = (long)(ss/alfa); T = (1.0+m)*ta+(s_-ss)/vv; 						//tv = (s_- ss)/vv; td = T - ta - tv;
		Na = (long)(np/(1.0+m)); Nd = np - Na; Nv = N - np; 					//Nv : number of steps in the phase v=const.
	}
	//Vsf [steps/0.1ms] : the start speed, Af[steps/(0.1ms)^2] : acceleration, Df : deceleration
	Vsf = vs/alfa/F; Af = aa/alfa/F/F; Df = -Af/m;
	vel = Vsf; pos = 0.0; np = 0;

	return true;
}
