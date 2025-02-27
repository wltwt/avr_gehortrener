/*
 * File:   main.c
 * Author: crfosse
 * 
 * Dette programmet gjennomfører et frekvenssveip ved hjelp av direkte digital syntese (DDS) på DACen til AVR128DB48. Sveipet gjennomføres ved hjelp av telleren TCA0 og er avbruddsstyrt for å få en jevn
 * punktprøvingsfrekvens. Prinsippene for DDS og frekvenssveip, samt noen praktiske detaljer, er beskrevet det det tekniske notatet [1]. Sveipet bruker en cosinus-oppslagstabell på 2^13 punkter, lagret i "cosine_table.h".
 * Noen detaljer:
 * - Utgangspinne for DAC: pinne PD6.
 * - Prossessorfrekvens: 24MHz.
 * - Punktprøvingsfrekvens: 16384 Hz.
 *
 * Kilder:
 * [1] - L. Lundheim: Generering av frekvenssveip, internt notat, NTNU, 2025
 * Created on 6. februar 2025, 14:42
 * 
 * Redigert:
 * 27.02.25
 * Olav Telneset
 * 
 * Kilder:
 * [2] - J. Wolfe: Note names, MIDI numbers and frequencies, https://newt.phys.unsw.edu.au/jw/notes.html, hentet 26.02.25
 */

/* Faste konstanter - disse trenger man i utgangspunktet ikke å justere*/
//#define F_CPU 24000000UL
#define F_SAMPLE 16384 /* Hz - Samplingsfrekvens */
#define M_SAMPLES 8192 /* Antall punktprøver i oppslagstabellen */

#include "usart.h"


#include <avr/cpufunc.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <time.h>


#include "cosine_table.h"
#include "notes.h"


/* Parametere for frekvenssveip. Justering av disse endrer oppførselen til sveipet. */
#define F_0 100  /* Hz - Startfrekvens */ 
#define F_1 3000 /* Hz - Sluttfrekvens */ 
#define T_SWEEP 3 /* s - sveipperiode */ 

/* Beregning av initialverdi for d_k og konstanten dd_k. */
#define K_ZERO (float)M_SAMPLES*F_0/(float)F_SAMPLE*((uint32_t)1<<16) /* Ligning 16 i [1] */
#define DELTA_DELTA_K (float)M_SAMPLES*(F_1-F_0)/((float)T_SWEEP*F_SAMPLE*F_SAMPLE)*((uint32_t)1<<16) /* Ligning 19 i [1]*/


/* States */
typedef enum {
    IDLE,
    PLAY_SINGLE_TONE,
    RUN_SWEEP
} State;


volatile State current_state = IDLE;


/* Globale variabler for ISR-bruk */
volatile uint16_t k = 0;
volatile uint32_t d_k = 0;
volatile uint32_t dd_k = 0;

/* Enkel timer */
volatile uint32_t isr_counter = 0; 


volatile float prev_freq = 0;


static inline float get_current_frequency(uint32_t d_k_val) {
    return (float)d_k_val / (float)(1UL<<16) * ( (float)F_SAMPLE / (float)M_SAMPLES );
}

/* Start timer: */
void start_timer(void) {
    TCA0.SINGLE.CNT = 0;  // Nullstill teller
    TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm; 
}

/* Stopp timer: */
void stop_timer(void) {
    TCA0.SINGLE.CTRLA &= ~TCA_SINGLE_ENABLE_bm;
}

/**
 * @brief Initialiserer hovedklokkka (F_CPU) til maksimal hastighet: 24MHz. 
 * Det er mulig å endre til andre klokkefrekvenser i funksjonen.
 */
void CLK_configuration(void)
{
	/* Setter OSCHF-klokka til 24 MHz */
	ccp_write_io ((uint8_t *) & CLKCTRL.OSCHFCTRLA, CLKCTRL_FRQSEL_24M_gc 
                                                   | CLKCTRL_AUTOTUNE_bm);
}

 /**
 * @brief Initialiserer DACen. Den bruker pinne PD6.
 */
 void DAC0_init(void)
 {
     /* Konfigurerer pinne PD6*/
	 /* Deaktiverer digital input buffer */
	 PORTD.PIN6CTRL &= ~PORT_ISC_gm;
	 PORTD.PIN6CTRL |= PORT_ISC_INPUT_DISABLE_gc;
     
	 /* Deaktiverer opptrekksmotstand */
	 PORTD.PIN6CTRL &= ~PORT_PULLUPEN_bm;
     
     VREF.DAC0REF |= VREF_REFSEL_VDD_gc;

	 /* Aktiverer DACen, utgangsbuffer */
	 DAC0.CTRLA = DAC_ENABLE_bm | DAC_OUTEN_bm;
 }

  /**
 * @brief Setter ønsket verdi på DACen.
 *
 * @Param val 10 bits verdi som skal konverters.
 */
 void DAC0_set_val(uint16_t val)
 {
	 /* Lagrer de to LSbene i DAC0.DATAL */
	 DAC0.DATAL = (val & (0x03)) << 6;
	 /* Lagrer de åtte MSbene i DAC0.DATAH */
	 DAC0.DATAH = val >> 2;
 }
 
 /**
 * @brief Initialiserer telleren TCA0. Telleren aktiveres ikke.
 */
void TCA0_init() {
    /* Konfigrerer tellerperioden (hvor mange klokkesykluser den skal telle).
    *  Formel: F_CPU/(F_CNT*SKALERING)
    */
	TCA0.SINGLE.PER = (uint16_t)(F_CPU/(F_SAMPLE*1));
    
    /* Aktiverer avbrudd fra telleroverflyt */
	TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
	
    /* Setter telleren i 16-bits modus med en klokkefrekvens på F_FPU/1. */
  	TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1_gc; 

}

 /**
 * @brief Setter i gang et sveip ved å aktivere telleren.
 */
void run_sweep() {
    /* Tenner LED0 for å markere start på sveip */
    PORTB.OUT &= ~PIN3_bm;

    /* Aktiverer telleren for å starte sveip*/
    TCA0.SINGLE.CTRLA = TCA_SINGLE_ENABLE_bm; 
}

 /**
  * @brief Avbruddsrutine for telleren. Denne håndterer frekvenssveipet.
  */
 ISR(TCA0_OVF_vect) {
    
    /* Definerer variabler for å holde punktprøver*/
    static int16_t curr_sample = 0; /* Verdier med fortegn fra cosinustabellen */
    static uint16_t dac_val = 0; /* Verdier uten fortegn som skal skrives til DACen*/

    /* Definerer variabler for å håndtere sveipet og frekvensgenrerering. 
    *  Se notatet [1] for detaljer.
    */
    static uint16_t k = 0;

    /* Bruker modulo for å sette d_k til 0 når frekvensen F_1 er nådd. */
    //d_k = (d_k + dd_k)  % ((uint32_t)F_1 << (15)); 
    
    /* Bruker modulo for å sette k til 0 etter en gjennomgang av oppslagstabellen */
    k = (k+(d_k>>16)) % M_SAMPLES;
        
    curr_sample = sineLookupTable[k];

    /* Konverterer til positiv 10-bits verdi*/
    dac_val = (curr_sample >> 6)+512; 
    DAC0_set_val(dac_val);
    
    
    if (current_state == RUN_SWEEP) {
        d_k += dd_k;
    }
    
    /* Oppdater ISR kjøretid */
    isr_counter++;
    
    /* Tilbakestiller avbruddsflagget for TCA0 */
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}

 
/* ISR knapp */
ISR(PORTB_PORT_vect)
{
    _delay_ms(30);

    if (!(PORTB.IN & PIN2_bm)) {  // trykket knapp (PB2)
        if (current_state == IDLE) {
            current_state = PLAY_SINGLE_TONE;
            
            /* Definer f0 som tonen F i oktav 5 */
            float freq = find_frequency("F", 5);
            
            if (freq < 0) {
                printf("Error");
                return;
            }
            
            printf("Trykk knappen når du tror du hører F5! Frekvensen er %.2f\n", freq);
            
            prev_freq = freq;
            d_k = (uint32_t)((float)M_SAMPLES * freq / (float)F_SAMPLE * (1UL<<16));
            dd_k = 0;
            isr_counter = 0;
            start_timer();
        }
        else if (current_state == RUN_SWEEP) {
            stop_timer();

            /* Beregn note ut i fra frekvens */
            float freq = get_current_frequency(d_k);
            Note n = find_note_name(freq);
            
            double delta_freq = fabs(freq-prev_freq);
            /* Returner resultat til bruker */
            printf("Du var %.2f Hz unna målet! Nærmest note du traff er %s i oktav %d, du traff %.2f Hz \n", delta_freq, n.name, n.index, freq);

            PORTB.OUT = PIN3_bm;
            current_state = IDLE;
        }
    }

    PORTB.INTFLAGS = PIN2_bm;
}


int main(void) {
   
    /* Setter hovedklokka til 24MHz for å få høy samplingsfrekvens. */
    CLK_configuration();
    
    /* Start uart usb kommunikasjon */
    USART3_init(9600);

    /* Initialiserer DACen*/
    DAC0_init();

    /* Intitaliserer telleren. Merk: den aktiveres ikke.*/
    TCA0_init();
    
    /* Setter pinne PB2, BTN0 på kortet, som inngang. */
    PORTB.DIR &= ~ PIN2_bm;
    PORTB.PIN2CTRL |= PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;    

    /* Setter pinne PB3, LED0 på kortet, som utgang. */
    PORTB.DIRSET = PIN3_bm;
    PORTB.OUT = PIN3_bm;
    
    sei();
    
    while (1) 
    {
        switch (current_state)
        {
            case IDLE:
                break;

            case PLAY_SINGLE_TONE:
                /* timer for å toppe enkel tonegenerasjonen f0 */
                if (isr_counter >= F_SAMPLE) {
                    stop_timer();
                    current_state = RUN_SWEEP;
                    
                    /* Sett opp verdier for run_sweep */
                    d_k = (uint32_t)K_ZERO;
                    dd_k = (uint32_t)DELTA_DELTA_K;
                    isr_counter = 0;
                    start_timer();
                }
                break;

            case RUN_SWEEP:
                /* Timer for å sjekke om tiden løper ut, da sveipen er ferdig */
                if (isr_counter >= ((uint32_t)T_SWEEP * F_SAMPLE)) {
                    stop_timer();
                    current_state = IDLE;
                }
                break;
        }
    }
}