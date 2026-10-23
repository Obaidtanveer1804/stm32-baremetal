/**
 * @file    melody.h
 * @brief   Non-blocking tune player on a PWM channel (passive buzzer),
 *          with a tiny RTTTL-like text notation:
 *
 *     "c4 e4 g4 c5:2 p g4 c5:4"
 *      note + octave [# sharp] [:length]   length 1 = whole ... 8 = eighth
 *      'p' = pause.  Tempo in BPM at play time.
 *
 * Note frequencies come from one table of the octave 4 and shifts:
 * f(octave n) = f(octave 4) * 2^(n-4) -- a power of two, so no math.
 */
#ifndef MELODY_H
#define MELODY_H

#include "hal_pwm.h"

typedef struct {
    hal_pwm_t   ch;
    const char *song;
    const char *pos;
    uint32_t    tempo_bpm;
    uint32_t    note_end_ms;
    bool        playing;
    bool        gap;       /* short silence between notes (articulation) */
} melody_t;

void     melody_init(melody_t *m, hal_pwm_t ch);
void     melody_play(melody_t *m, const char *song, uint32_t tempo_bpm);
void     melody_stop(melody_t *m);
bool     melody_is_playing(const melody_t *m);
/** Call from the main loop. */
void     melody_update(melody_t *m, uint32_t now_ms);
/** Frequency of a note name ("a4" -> 440, "c#5" -> 554); 0 = invalid. */
uint32_t melody_note_hz(const char *note);

extern const char *const melody_songs[];      /* NULL terminated */
extern const char *const melody_song_names[];

#endif /* MELODY_H */
