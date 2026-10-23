/**
 * @file    melody.c
 * @brief   Tune player (see melody.h).
 */
#include "melody.h"
#include "strutil.h"

#include <stddef.h>

#define MELODY_GAP_MS      (20U)
#define MELODY_DEFAULT_LEN (4U)

/* octave 4 in Hz x 100 (C4 = 261.63 Hz) for precise shifting */
static const uint32_t octave4_x100[12] = {
    26163U, 27718U, 29366U, 31113U, 32963U, 34923U,
    36999U, 39200U, 41530U, 44000U, 46616U, 49388U
};

/* semitone index of the letters c d e f g a b */
static const uint8_t letter_semitone[7] = { 9U, 11U, 0U, 2U, 4U, 5U, 7U };   /* a..g */

const char *const melody_songs[] = {
    "c4 d4 e4 f4 g4 a4 b4 c5:2",
    "e5:8 e5:8 p:8 e5:8 p:8 c5:8 e5 g5 p g4",
    "g4:8 g4:8 a4 g4 c5 b4:2 g4:8 g4:8 a4 g4 d5 c5:2",
    "a4:8 p:8 a4:8 p:8 a4 f4:8 c5:16 a4 f4:8 c5:16 a4:2",
    NULL
};

const char *const melody_song_names[] = {
    "scale", "intro", "birthday", "march", NULL
};

static const char *skip_spaces(const char *p)
{
    while ((*p == ' ') || (*p == ',')) {
        p++;
    }
    return p;
}

/* Parse one token; returns the frequency (0 = pause) and the length. */
static const char *parse_note(const char *p, uint32_t *hz, uint32_t *len, bool *ok)
{
    const char c = chr_to_lower(*p);
    *ok  = true;
    *hz  = 0U;
    *len = MELODY_DEFAULT_LEN;

    if (c == 'p') {
        p++;
    } else if ((c >= 'a') && (c <= 'g')) {
        int32_t semi = letter_semitone[c - 'a'];
        p++;
        if (*p == '#') {
            semi++;
            p++;
        }
        int32_t octave = 4;
        if (chr_is_digit(*p)) {
            octave = *p - '0';
            p++;
        }
        if (semi >= 12) {          /* b# = c of the next octave */
            semi -= 12;
            octave++;
        }
        uint32_t f = octave4_x100[semi];
        f = (octave >= 4) ? (f << (uint32_t)(octave - 4)) : (f >> (uint32_t)(4 - octave));
        *hz = (f + 50U) / 100U;
    } else {
        *ok = false;
        return p;
    }
    if (*p == ':') {
        p++;
        uint32_t l = 0U;
        while (chr_is_digit(*p)) {
            l = (l * 10U) + (uint32_t)(*p - '0');
            p++;
        }
        *len = (l == 0U) ? MELODY_DEFAULT_LEN : l;
    }
    return p;
}

uint32_t melody_note_hz(const char *note)
{
    uint32_t hz;
    uint32_t len;
    bool     ok;
    const char *end = parse_note(note, &hz, &len, &ok);
    return (ok && (*end == '\0')) ? hz : 0U;
}

void melody_init(melody_t *m, hal_pwm_t ch)
{
    m->ch      = ch;
    m->song    = NULL;
    m->pos     = NULL;
    m->playing = false;
}

void melody_play(melody_t *m, const char *song, uint32_t tempo_bpm)
{
    m->song        = song;
    m->pos         = song;
    m->tempo_bpm   = (tempo_bpm == 0U) ? 120U : tempo_bpm;
    m->playing     = true;
    m->gap         = false;
    m->note_end_ms = 0U;
}

void melody_stop(melody_t *m)
{
    m->playing = false;
    hal_pwm_set_duty(m->ch, 0U);
}

bool melody_is_playing(const melody_t *m)
{
    return m->playing;
}

void melody_update(melody_t *m, uint32_t now_ms)
{
    if (!m->playing || ((int32_t)(now_ms - m->note_end_ms) < 0)) {
        return;
    }
    if (!m->gap && (m->note_end_ms != 0U)) {
        /* end of a note: short silence so repeated notes are distinct */
        hal_pwm_set_duty(m->ch, 0U);
        m->gap         = true;
        m->note_end_ms = now_ms + MELODY_GAP_MS;
        return;
    }
    m->gap = false;
    m->pos = skip_spaces(m->pos);
    if (*m->pos == '\0') {
        melody_stop(m);
        return;
    }
    uint32_t hz;
    uint32_t len;
    bool     ok;
    m->pos = parse_note(m->pos, &hz, &len, &ok);
    if (!ok) {
        melody_stop(m);            /* syntax error: stop rather than loop */
        return;
    }
    /* a whole note lasts 4 beats: duration = 4 * 60000 / bpm / len */
    const uint32_t dur = (240000U / m->tempo_bpm) / len;
    if ((hz != 0U) && (hal_pwm_init(m->ch, hz) == HAL_OK)) {
        hal_pwm_set_duty(m->ch, 500U);
    } else {
        hal_pwm_set_duty(m->ch, 0U);
    }
    m->note_end_ms = now_ms + ((dur > MELODY_GAP_MS) ? (dur - MELODY_GAP_MS) : dur);
}
