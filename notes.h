/* 
 * File:   notes.h
 * Author: tln
 *
 * Created on 26. februar 2025, 21:39
 */

#ifndef NOTES_H
#define	NOTES_H

#ifdef	__cplusplus
extern "C" {
#endif
    

    
#include <stdint.h>

typedef struct {
    const char *name;
    int16_t index;
} Note;

    
float find_frequency(const char *note, int16_t octave);
Note find_note_name(float freq);



#ifdef	__cplusplus
}
#endif

#endif	/* NOTES_H */

