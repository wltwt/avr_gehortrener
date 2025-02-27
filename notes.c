/*
 * File:   notes.c
 * Author: tln
 *
 * Created on 26. februar 2025, 21:38
 */

#include <math.h>
#include <string.h>

#include "notes.h"

/* Notetabell */
static const Note notes[] = {
    {"A",      0},  {"A#/Bb",  1},  {"B",      2},  
    {"C",      3},  {"C#/Db",  4},  {"D",      5},
    {"D#/Eb",  6},  {"E",      7},  {"F",      8},
    {"F#/Gb",  9},  {"G",     10},  {"G#/Ab", 11}
};

/* Hjelpemetode */
int16_t find_note_index(const char *note) {
    for (size_t i = 0; i < sizeof(notes) / sizeof(notes[0]); i++) {
        if (strcmp(notes[i].name, note) == 0) {
            return notes[i].index;
        }
    }
    return -1;
}
/* Hjelpemetode */
float find_note_frequency(int16_t note_index, int16_t octave) {
    int16_t note_index_offset = (octave - 5) * 12 + note_index;
    return 440.0f * powf(2.0f, note_index_offset / 12.0f);
}
/* Hjelpemetode */
int16_t find_nearest_index_note(float freq) {
    return (int16_t)lroundf(12.0f * log2f(freq / 440.0f));
}
/* Hjelpemetode */
int16_t note_octave(int16_t note_index_offset) {
    return round(4 + (note_index_offset / 12.0f));
}

float find_frequency(const char *note, int16_t octave) {
    int16_t index = find_note_index(note);
    if (index == -1) {
        return -1.0f;
    }
    return find_note_frequency(index, octave);
}

Note find_note_name(float freq) {
    /* Finn index */
    int16_t note_index_offset = find_nearest_index_note(freq);
    /* Håndter verdier > 12 */
    int16_t index = (note_index_offset % 12 + 12) % 12;
    /* Finn hvilken oktav vi befinner oss i */
    int16_t octave = note_octave(note_index_offset);

    Note n;
    n.name  = notes[index].name;
    n.index = octave; 
    return n;
}




