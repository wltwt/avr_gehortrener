/* 
 * File:   config.h
 * Author: crfosse
 *
 * Created on 11. oktober 2024, 14:01
 */

#ifndef CONFIG_H
#define	CONFIG_H

#ifdef	__cplusplus
extern "C" {
#endif

    

//#define F_CPU 24000000UL

/* Definition of main clock frequency for timing calculations in the program.*/


/* Target voltage of the system, used for calculations*/
#define VOLTAGE_REFERENCE_VDD 3300 //mV 

#ifdef	__cplusplus
}
#endif

#endif	/* CONFIG_H */
