/* 
 * File:   usart.h
 * Author: crfosse
 *
 * Created on 11. oktober 2024, 13:57
 */

#ifndef USART_H
#define	USART_H

#ifdef	__cplusplus
extern "C" {
#endif
    
/* Global configurations for the project */
#include "config.h"    
    
#define F_CPU 24000000UL
    
    
#include <avr/io.h>
#include <string.h>
#include <stdio.h>

//#ifndef F_CPU
//    #define F_CPU 4000000UL
//#endif

/* Initialiserer USART3 med en gitt baudrate. */
void USART3_init(unsigned baud);

                    
void USART3_sendString(char *str);


#ifdef	__cplusplus
}
#endif

#endif	/* USART_H */
