/*
Copyright (C) 2011  Dirk Rudolph, Lucas Stach, Paul Ritter, Heiko Schmidt

Dieses Programm ist freie Software. Sie können es unter den Bedingungen der GNU
General Public License Version 2, wie von der Free Software Foundation veröffentlicht,
weitergeben und/oder modifizieren.

Die Veröffentlichung dieses Programms erfolgt in der Hoffnung, daß es Ihnen von Nutzen
sein wird, aber OHNE IRGENDEINE GARANTIE, sogar ohne die implizite Garantie der
MARKTREIFE oder der VERWENDBARKEIT FÜR EINEN BESTIMMTEN ZWECK. Details finden Sie in
der GNU General Public License.

Sie sollten ein Exemplar der GNU General Public License zusammen mit diesem Programm
erhalten haben. Falls nicht, schreiben Sie an die Free Software Foundation, Inc., 51
Franklin St, Fifth Floor, Boston, MA 02110, USA.
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include "../globalconf.h"
#include "../output/display_12_10.h"
#include "clock.h"
#include "../input/input.h"
#include "themes_12_10/themes_12_10.h"

 /**
 * struct timefunc_dispatch_t holds pointers to time related functions
 */
struct timefunc_dispatch_t {
    void (*increaseTime)(void);
    void (*switchToNextMode)(void);
};

/* -------------------------------------------------------------------------- */

volatile struct timeval_t time = {
    .seconds = 0,
    .minutes = 0,
    .hours = 0,
    .milliseconds = 0,
    .dirty = 0
};

#ifdef STOP_WATCH
enum CLOCK_MODE currentMode = MODE_IDLE;
#else
enum CLOCK_MODE currentMode = MODE_NONE;
#endif

struct timefunc_dispatch_t dispatch_table;

/* -------------------------------------------------------------------------- */

static inline void stopClock_normal()
{
    TCCR2 &= ~0x05;
}

static inline void startClock_normal()
{
    TCNT2 = 0;   // set count reg to 0
    TCCR2 |= 0x05;  // prescaler 128
}

void increaseTime_normal()
{
    switch(currentMode) {
        case MODE_NONE:
            time.seconds++;
            break;
        case MODE_SECOND:
            if(getKeyRepeated(KEY0)) {
                time.seconds++;
            }
            break;
        case MODE_MINUTE:
            if(getKeyRepeated(KEY0)) {
                time.minutes++;
            }
            break;
        case MODE_HOUR:
            if(getKeyRepeated(KEY0)) {
                time.hours++;
            }
            break;
        default: break;
    }

    if(time.seconds == 60) {
        time.minutes++;
        time.seconds = 0;
    }

    if(time.minutes == 60) {
        time.hours++;
        time.minutes = 0;
    }

    if(time.hours == 24) {
        time.hours = 0;
    }

    time.dirty = 1;
}

void switchToNextMode_normal()
{
    switch(currentMode) {
        case MODE_HOUR:
            currentMode = MODE_MINUTE;
            break;
        case MODE_MINUTE:
            currentMode = MODE_SECOND;
            break;
        case MODE_SECOND:
            currentMode = MODE_NONE;
            TCCR2 |= 0x01; // set 1 to get back the 128 prescaler
            break;
        case MODE_NONE:
            currentMode = MODE_HOUR;
            TCCR2 &= 0xfe; // clear 1 to get the prescaler to 64
            break;
        default: break;
    }
}

#ifdef STOP_WATCH
static inline void stopClock_stopwatch()
{
    TCCR2 &= ~0x01;
}

static inline void startClock_stopwatch()
{
    TCNT2 = 92;  // set count reg to 92
    TCCR2 |= 0x01;  // no prescaler
}

void increaseTime_stopwatch()
{
    switch(currentMode) {
        case MODE_NONE:
            time.milliseconds += 5; // 1000ms / 200 interupts/s
            TCNT2 = 92; // preload to get 200 interupts per second
            break;
        case MODE_IDLE:
        default: break;
    }

    if(time.milliseconds == 1000) {
        time.seconds++;
        time.milliseconds = 0;
    }

    if(time.seconds == 60) {
        time.minutes++;
        time.seconds = 0;
    }

    if(time.minutes == 60) {
        time.hours++;
        time.minutes = 0;
    }

    if(time.hours == 24) {
        time.hours = 0;
    }

    time.dirty = 1;
}

void switchToNextMode_stopwatch()
{
        switch(currentMode) {
        case MODE_IDLE:
            currentMode = MODE_NONE;
            resetTime();
            startClock_stopwatch();
            break;
        case MODE_NONE:
            currentMode = MODE_IDLE;
            stopClock_stopwatch();
            break;
        default: break;
        }
}
#endif // STOP_WATCH

void initClock(const enum CLOCK_TYPE type)
{
    ASSR |= 0x08; // wire external timesource to tcnt2
    TIMSK |= 0x40;  // enable overflow interupt for counter 2

    switch(type) {
        case TYPE_NORMAL:
            dispatch_table.increaseTime = increaseTime_normal;
            dispatch_table.switchToNextMode = switchToNextMode_normal;
            startClock_normal();
            break;
        #ifdef STOP_WATCH
        case TYPE_STOPWATCH:
            dispatch_table.increaseTime = increaseTime_stopwatch;
            dispatch_table.switchToNextMode = switchToNextMode_stopwatch;
            break;
        #endif
    }

}

void resetTime()
{
    time.milliseconds = 0;
    time.hours = 0;
    time.minutes = 0;
    time.seconds = 0;
    time.dirty = 1;
}

void switchToNextMode()
{
    dispatch_table.switchToNextMode();
}

void printTime(bitmap_t destination, const enum CLOCK_THEME theme)
{
    if(time.dirty) {
        switch(theme) {
            case THEME_ANALOG:
                pt_analogClock(destination, time, currentMode);
                break;
            case THEME_BARS:
                pt_raisingBars(destination, time, currentMode);
                break;
            case THEME_BINARY:
                pt_simpleBinary(destination, time, currentMode);
                break;
            case THEME_DICES:
                pt_dices(destination, time, currentMode);
                break;
        }

        time.dirty = 0;
    }
}

// Interupt service routine for clock overflow
ISR(TIMER2_OVF_vect)
{
    dispatch_table.increaseTime();
}
