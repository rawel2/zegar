// Clock/calander programm for JY 3208 Clock from DX. 32*8 LED Matrix + HT1632C + ATmega8;
//
// Byoung Oh. 9/30/12
// Based on code from DrJones
//
//
// Key1 : Setup Button. Hold 3 sec to enter or exit setup mode. In setup mode, use to switch mode.
// Key2 : Date Button. Press to display date. In setup mode, increase number.
// Key3 : LED Brightness Button. In setup mode, decrease number.

//#define F_CPU 8000000

// #define DZIADEK Kuba1

#include <avr/pgmspace.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>
#include "HT1632/ht1632c.h"
#include "DS3231/ds3231.h"
#include "I2C_SOFT/i2c_soft.h"
#include "FONT/font5x7.c"

// Font Map
#define upper_char_offset -55
#define lower_char_offset -61

//pins and macros

#define key_set  ((PIND&(1<<7))==0)
#define key_up   ((PIND&(1<<6))==0)
#define key_down ((PIND&(1<<5))==0)

#define keysetup() do{ DDRD&=0xff-(1<<7)-(1<<6)-(1<<5); PORTD|=(1<<7)+(1<<6)+(1<<5); }while(0)  //input, pull up
#define OPOZNIENIE 19


uint8_t leds[32];  //the screen array, 1 uint8_t = 1 column, left to right, lsb at top.


// Global Variables
volatile uint8_t sec = 0;
volatile uint8_t sec16 = 0;

uint8_t sec0 = 200, minute, hour, day, month, temp;
uint16_t year;
uint8_t setup = 0; // setup =1 ustawianie
uint8_t mode = 0;  // mode=0 godziny mode=1 minuty
uint8_t bright = 0;

TDATETIME datetime;
TTEMP temperature;
uint8_t tani_prad;



// End Global Variables


// =============================== CALANDER CODE ==========================

inline void clocksetup() {  // CLOCK, interrupt every second
    ASSR |= ( 1 << AS2 );    //timer2 async from external quartz
    //TCCR2=0b00000101;    //normal,off,/128; 32768Hz/256/128 = 1 Hz
    TCCR2 = 0b00000010;  //normal,off,/128; 32768Hz/256/32 = 16 Hz
    TIMSK |= ( 1 << TOIE2 ); //enable timer2-overflow-int
    sei();               //enable interrupts
}

// CLOCK interrupt
ISR( TIMER2_OVF_vect ) {   //timer2-overflow-int
    sec16++;
    if (( sec16 % 16 ) == 0 ) sec++;
}

uint8_t odwroc_bity( uint8_t x ) {
    uint8_t y = 0;
    for ( uint8_t i = 0;i < 8;i++ )
        if ( x & ( 1 << i ) )
            y |= ( 128 >> i );
    return( y );
}

void renderclock( void ) {
    uint8_t col = 0;

    for ( uint8_t i = 0;i < 5;i++ ) {
        if ( setup && mode == 0 ) { // Hour Setup Mode
            if ( sec % 2 )
                leds[col++] = pgm_read_byte( &font[( hour/10 )][i] );
            else
                leds[col++] = 0;
        } else {
            leds[col++] = pgm_read_byte( &font[( hour/10 )][i] );
        }
    }
    leds[col++] = 0;
    for ( uint8_t i = 0;i < 5;i++ ) {
        if ( setup && mode == 0 ) { // Hour Setup Mode
            if ( sec % 2 )
                leds[col++] = pgm_read_byte( &font[hour%10][i] );
            else
                leds[col++] = 0;
        } else {
            leds[col++] = pgm_read_byte( &font[hour%10][i] );
        }
    }

    leds[col++] = 0;
    if ( setup && mode == 2 ) {
        leds[col++] = 0x66;
        leds[col++] = 0x66;
    } else
        if ( sec % 2 || 1) {
#ifdef DZIADEK
            leds[col++] = 0;
            leds[col++] = 0;
#else
            leds[col++] = 0x66 + tani_prad * 0x18;
            leds[col++] = 0x66 + tani_prad * 0x18;
#endif
        } else {
            leds[col++] = 0;
            leds[col++] = 0;
        } // Use this to blink colon


    leds[col++] = 0;

    for ( uint8_t i = 0;i < 5;i++ ) {
        if ( setup && mode == 1 ) { // Minute Setup Mode
            if ( sec % 2 )
                leds[col++] = pgm_read_byte( &font[( minute/10 )][i] );
            else
                leds[col++] = 0;
        } else {
            leds[col++] = pgm_read_byte( &font[minute/10][i] );
        }
    }

    leds[col++] = 0;
    for ( uint8_t i = 0;i < 5;i++ ) {
        if ( setup && mode == 1 ) { // Minute Setup Mode
            if ( sec % 2 )
                leds[col++] = pgm_read_byte( &font[( minute%10 )][i] );
            else
                leds[col++] = 0;
        } else {
            leds[col++] = pgm_read_byte( &font[( minute%10 )][i] );
        }
    }

    leds[col++] = 0;

#ifdef DZIADEK
    leds[col++] = 0;
    leds[col++] = 0;
    leds[col++] = 0;
    leds[col++] = 0;
    leds[col++] = 0;
#else
    leds[col++] = odwroc_bity(( 1 << bright ) ); 
    leds[col++] = odwroc_bity(( 1 << bright ) );
    leds[col++] = 0;
    leds[col++] = odwroc_bity( temp );
    leds[col++] = odwroc_bity( temp );                                 
    
#endif



}

void renderdate( void ) { // Render Date

    uint8_t col = 0;

    leds[col++] = 0;

    for ( uint8_t i = 0;i < 5;i++ ) {
        if ( setup && mode == 3 ) { // Setup Month Mode
            if ( sec % 2 )
                leds[col++] = pgm_read_byte( &font[( month/10 )][i] );
            else
                leds[col++] = 0;
        } else {
            leds[col++] = pgm_read_byte( &font[( month/10 )][i] );
        }

    }

    leds[col++] = 0;

    for ( uint8_t i = 0;i < 5;i++ ) {
        if ( setup && mode == 3 ) { // Setup Month Mode
            if ( sec % 2 )
                leds[col++] = pgm_read_byte( &font[( month%10 )][i] );
            else
                leds[col++] = 0;
        } else {
            leds[col++] = pgm_read_byte( &font[( month%10 )][i] );
        }
    }

    leds[col++] = 0;

    for ( uint8_t i = 0;i < 5;i++ ) {
        leds[col++] = pgm_read_byte( &font[69][i] );
    }

    leds[col++] = 0;

    for ( uint8_t i = 0;i < 5;i++ ) {
        if ( setup && mode == 4 ) { // Setup Day Mode
            if ( sec % 2 )
                leds[col++] = pgm_read_byte( &font[( day/10 )][i] );
            else
                leds[col++] = 0;
        } else {
            leds[col++] = pgm_read_byte( &font[( day/10 )][i] );
        }
    }

    leds[col++] = 0;

    for ( uint8_t i = 0;i < 5;i++ ) {
        if ( setup && mode == 4 ) { // Setup Day Mode
            if ( sec % 2 )
                leds[col++] = pgm_read_byte( &font[( day%10 )][i] );
            else
                leds[col++] = 0;
        } else {
            leds[col++] = pgm_read_byte( &font[( day%10 )][i] );
        }
    }

    leds[col++] = 0;
    leds[col++] = 0;

}

void rendertemp( void ) {
    uint8_t col = 0;
    uint8_t fr[4] = {0, 2, 5, 7};

    //znak jesli ujemna
    if ( temperature.cel < 0 ) {
        // minus
        for ( uint8_t i = 0;i < 5;i++ )  leds[col++] = pgm_read_byte( &font[69][i] );
    } else {
        for ( uint8_t i = 0;i < 5;i++ ) leds[col++] = 0;
    }
    leds[col++] = 0;

    //pierwsza cyfra
    for ( uint8_t i = 0;i < 5;i++ ) {
        leds[col++] = pgm_read_byte( &font[temperature.cel/10][i] );
    }
    leds[col++] = 0;

    //druga cyfra
    for ( uint8_t i = 0;i < 5;i++ ) {
        leds[col++] = pgm_read_byte( &font[temperature.cel%10][i] );
    }
    leds[col++] = 0;

    // kropka
    for ( uint8_t i = 0;i < 2;i++ ) {
        leds[col++] = 0b11000000;
    }
    leds[col++] = 0;

    // po przecinku
    for ( uint8_t i = 0;i < 5;i++ ) {
        leds[col++] = pgm_read_byte( &font[fr[temperature.fract]][i] );
    }
    leds[col++] = 0;

    leds[col++] = 0b00000111;
    leds[col++] = 0b00000101;
    leds[col++] = 0b00000111;

    leds[col++] = 0;
    leds[col++] = 0;

}

// ====================== Clock Code End =============================

void napisz_litere( char lit, int x, int y ) {
    for ( int i = -1;i < 6;i++ ) {
        if (( x + i >= 0 ) && ( x + i < 32 ) ) {
            if (( i == -1 ) || ( i == 5 ) )
                leds[x+i] = 0;
            else {
                if ( y == 0 )
                    leds[x+i] = pgm_read_byte( &font5x7[( lit )*5+i] );
                else if ( y > 0 )
                    leds[x+i] = ( pgm_read_byte( &font5x7[( lit )*5+i] ) << y );
                else
                    leds[x+i] = ( pgm_read_byte( &font5x7[( lit )*5+i] ) >> -y );
            }
        }
    }

}

void napisz_tekst( char *str, int x, int y ) {
    char znak;
    uint8_t i = 0;
    while (( znak = *( str++ ) ) ) {
        napisz_litere( znak, x + i, y );
        i = i + 6;
    }
}

void napisz_gora_dol( char *str ) {

    for ( int i = -10;i < 1;i++ ) {
        napisz_tekst( str, 0, i );
        HTsendscreen();
        _delay_ms( 100 );
    }
    HTbrightness( 15 );
    _delay_ms( 500 );
    HTbrightness( bright );
    for ( int i = 0;i < 10;i++ ) {
        napisz_tekst( str, 0, i );
        HTsendscreen();
        _delay_ms( 100 );
    }

}

void napisz_przewin( char * str ) {
    int dl = ( -6 ) * strlen( str );
    for ( int i = 32; i > dl;i-- ) {
        napisz_tekst( str, i, 0 );
        HTsendscreen();
        _delay_ms( 10 );
    }
}

void ustaw_czas( void ) {

    napisz_przewin( "ustaw czas:" );
    setup = 1;

    mode = 0; // godziny
    while ( !key_set ) {
        renderclock();
        HTsendscreen();
        _delay_ms( 200 );
        if ( key_up ) {
            if ( ++hour > 23 ) hour = 0;
        }

        if ( key_down ) {
            if ( hour > 0 ) hour--;
        }

    }

    while ( key_set ) _delay_ms( 10 );

    mode = 1; // minuty
    while ( !key_set ) {
        renderclock();
        HTsendscreen();
        _delay_ms( 200 );
        if ( key_up ) {
            if ( ++minute > 59 ) minute = 0;
        }

        if ( key_down ) {
            if ( minute > 0 ) minute--;
        }
    }

    _delay_ms( 50 );
    napisz_przewin( " Zapisac ?" );
    _delay_ms( 20 );
    napisz_przewin( "  up - TAK" );
    _delay_ms( 20 );
    napisz_przewin( "  down - NIE" );
    _delay_ms( 10 );

    mode = 2;
    while ( !key_up && !key_down ) {
        renderclock();
        HTsendscreen();
        _delay_ms( 200 );
    }
    if ( key_up ) {
        while ( key_up ) _delay_ms( 10 );
        DS3231_set_time( hour, minute, 0 );
    }

    if ( key_down ) {
        while ( key_down ) _delay_ms( 10 );
    }

    setup = 0;
    napisz_gora_dol( "czas: " );
    _delay_ms( 100 );
}

void pomoc( void ) {
    napisz_przewin( "   set - ustawianie" );
    _delay_ms( 200 );
    napisz_przewin( "   up - temperatura" );
    _delay_ms( 200 );
    napisz_przewin( "   down - jasnosc" );
    _delay_ms( 100 );

}

uint8_t ustaw_temp( void ) {
    uint8_t tmp;

    if ( temperature.cel < 17 )
        tmp = 1 + 2;
    else if ( temperature.cel > 31 )
        tmp = 128 + 64;
    else
        tmp = ( 1 << (( temperature.cel - 17 ) / 2 ) );
    return( tmp );
}

uint8_t jest_pomiedzy( uint8_t h1, uint8_t m1, uint8_t h2, uint8_t m2, uint8_t o ) {
    //uint8_t poprzez_polnoc;
    //if((h1>h2) || ((h1=h2) && (m1>m2))) poprzez_polnoc = 1;



    return( 0 );
}


uint8_t czy_tani_prad( void ) {
    if ( jest_pomiedzy( 13, 1, 16, 1, OPOZNIENIE ) || jest_pomiedzy( 22, 1, 6, 1, OPOZNIENIE ) )
        return( 1 );
    else
        return( 0 );
}



int main( void ) {  //========================= main ==================

    setup = 0;  // 0 - normal mode, 1 - setup mode
    mode = 0;  //  0 - hour, 1 - minutes, 2- year, 3 - month, 4 - day


    HTpinsetup();
    HTsetup();
    HTbrightness( bright*2 );
    keysetup();
    clocksetup();
    i2c_init();
    DS3231_init();
    DS3231_get_temp( &temperature );
    temp = ustaw_temp();


//  for (uint8_t i=0;i<32;i++) leds[i]=0b01010101<<(i%2);  HTsendscreen();

    napisz_gora_dol( " Kuba " );
    _delay_ms( 500 );

    pomoc();                             

    //DS3231_set_time( 18, 51, 0 );
    //DS3231_set_date( 14, 8, 7, 4 );

    while ( 1 ) {
        if ( sec0 != sec ) {
            DS3231_get_datetime( &datetime );
            hour = datetime.hh;
            minute = datetime.mm;
            tani_prad = czy_tani_prad();
            renderclock();
            HTsendscreen();
            sec0 = sec;
        }

        _delay_ms( 100 );

        if ( sec % 32 ) {
            DS3231_get_temp( &temperature );
            temp = ustaw_temp();
        }

        if ( key_up ) {
            napisz_gora_dol( "temp: " );
            DS3231_get_temp( &temperature );
            rendertemp();
            HTsendscreen();
            _delay_ms( 3000 );
            napisz_gora_dol( "czas: " );
        }

        if ( key_set ) {
            while ( key_set ) _delay_ms( 10 );
            ustaw_czas();
        }

        if ( key_down ) {
            while ( key_down ) _delay_ms( 10 );
            if ( ++bright > 7 ) bright = 0;
            HTbrightness( bright*2 );
            renderclock();
            HTsendscreen();
        }
    }


}//main