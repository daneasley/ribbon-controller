
/* Arduino Ribbon(s) Controller
 * 
 * 20180210 Dan Easley.
 * 
 * Linear Soft Pot reading code based on examples from "Arduino Music and Audio Projects" by Mike Cook
 * Smoothing code from Arduino tutorials
 * 
 * Configured for: Two-Ribbon Controller by DE 2017-2018
 * 
 */

/* LIBRARY REQUIREMENTS */

// MIDI Library required for MIDI interface
#include <MIDI.h>

// Something's no doubt required for (unimplemented) MIDI-over-USB interface

// OSC Library required for (unimplemented) OSC interface
// #include <ArduinoUnit.h>
// #include <OSCMessage.h>



/* BEGIN CONFIGURATION */

// Hardware Setup

const int NUMBEROFRIBBONS = 2;

// you must define the pins for each ribbon. 
// mistakes could lead to softpot damage.
//
//                          Vcc2
//                      Vcc1 |      Vcc3, ...
//                        |  |       |
const int ribbonPin1[] = {1, 3}; //, 5, ...};
//
//                          Gnd2
//                      Gnd1 |      Gnd3, ...
//                        |  |       |
const int ribbonPin2[] = {2, 4}; //, 6, ... };
//
//                              Wiper2
//                         Wiper1 |     Wiper3, ...
//                            |   |       |
const int ribbonPinRead[] = {A0, A1}; //,A2  , ...};


// Volume / Attack / Decay Configuration

// An attackRate or decayRate of 1 would be very slow, while an attackRate or decayRate equal to maxVolume would be instant.

const int attackRate = 15;
const int decayRate = 5;

const int maxVolume = 127; // 1-127


// Sensitivity of Ribbon (Mike Cook's Ribbon Reading Code)

const int ribbonSensitivity = 30;

// Ribbon Smoothing (Arduino Exmple Code)

// Define the number of samples to keep track of. The higher the number, the
// more the readings will be smoothed, but the slower the output will respond to
// the input. Using a constant rather than a normal variable lets us use this
// value to determine the size of the readings array.

const int NUMBEROFREADINGS = 10;


// PLAY MODE OPTIONS

// Play Mode can be either (P)itchbend or (N)otes.  (L)ookup is not yet implemented.

// Notes converts ribbon presses to a note within a configured range.
// Pitchbend converts ribbon presses to a single note with a pitchbend assuming 12 semitones.
// Lookup converts ribbon presses to a note within a configured range, plus a pitchbend assuming 2 semitones, using a lookup table.

const char PLAYMODE = "P"; // Notes not yet implemented.

// you must define values for each ribbon.

// Notes Options:
                        //  1,  2, ...
    const int lowNote[] = {57, 64};
    const int highNote[] = {81, 88};

// Pitchbend Options:
                           //  1,  2, ...
    const int singleNote[] = {69, 76};

// INTERFACE OPTIONS

// Interface can be either (S)erial, (O)SC, MIDI over (U)SB, or (M)IDI.

//const char INTERFACE = "S";   // Uncomment this line for Serial interface (debugging).

//const char INTERFACE = "O";   // Uncomment this line for OSC interface (not yet implemented!)

//const char INTERFACE = "U";   // Uncomment this line for MIDI-over-USB interface (not yet implemented!)

const char INTERFACE = "M";      // Uncomment this line...
MIDI_CREATE_DEFAULT_INSTANCE();  // ... and this line for MIDI interface.

/* END CONFIGURATION */



// Lookup Tables

// There must be a lookup table for each ribbon.  Start counting at zero.

int pitchLookupTable[NUMBEROFRIBBONS][2][1024] = { 
        {       // Ribbon 1
          {},   // notenumber
          {}    // pitchbend
        } 
       ,{       // Ribbon 2
          {},   // notenumber
          {}    // pitchbend
        }
    // ,{
    //    {},
    //    {}
    //  }
    };

// Note Logic

// Flag variables - is note on, should it be started, should it be stopped.
int noteRunning[NUMBEROFRIBBONS];
int noteStart[NUMBEROFRIBBONS];
int noteStop[NUMBEROFRIBBONS];

// Pitch infomation - what's the softpot reading, what note does that translate to, what pitchbend is that.

int ribbonReadingList[NUMBEROFRIBBONS][NUMBEROFREADINGS];
int ribbonReadingIndex[NUMBEROFRIBBONS];
int ribbonReadingTotal[NUMBEROFRIBBONS];
int ribbonReadingAverage[NUMBEROFRIBBONS];

int ribbonReadingEstablished[NUMBEROFRIBBONS];

int ribbonChannel[NUMBEROFRIBBONS];
int ribbonNoteNumber[NUMBEROFRIBBONS];
int ribbonPitchBend[NUMBEROFRIBBONS];
int ribbonVolume[NUMBEROFRIBBONS];

char ribbonEnvelope[NUMBEROFRIBBONS];


void setup() 
{

    // Setup Interface
    
    switch (INTERFACE) 
    {
        case 'S' :  // Serial Interface (debugging)
            Serial.begin(38400);
            break;
        case 'O' :  // OSC Interface (not yet implemented)
            break;
        case 'U' :  // MIDI-over-USB Interface (not yet implemented)
            break;
        default :   // MIDI Interface
            MIDI.begin(MIDI_CHANNEL_OMNI);
            break;
    }


    // Setup Variables
    
    for ( int current = 0; current < (NUMBEROFRIBBONS - 1); current++ ) 
    {
        noteRunning[current] = 0;
        noteStart[current] = 0;
        noteStop[current] = 0;

        for ( int reading = 0; reading < (NUMBEROFREADINGS); reading++ ) {
            ribbonReadingList[current][reading] = 0;
        }
        ribbonReadingIndex[current] = 0;
        ribbonReadingTotal[current] = 0;
        ribbonReadingAverage[current] = 0;
        ribbonReadingEstablished[current] = 0;
        
        ribbonChannel[current] = 0;
        ribbonNoteNumber[current] = 0;
        ribbonPitchBend[current] = 0;
        ribbonVolume[current] = 0;
        ribbonEnvelope[current] = "N";   // A = Attack; D = Decay; N = Neither
        
        // set up pot end pins
        pinMode(ribbonPin1[current], INTERFACE);
        pinMode(ribbonPin2[current], INTERFACE);
        pinMode(ribbonPinRead[current], INPUT);
    }
}

    // MAIN LOOP

void loop() 
{
    // Iterate through pitch ribbons, starting, stopping, or changing notes as instructed.
    
    for (int current = 0; current < (NUMBEROFRIBBONS - 1); current++) {
        readRibbon(current);
        if (!noteRunning[current]) {        // If note is not on...
            if (!noteStart[current]) {      //             ... and doesn't need to be...
                                            //                                      ...then do nothing.
            } else {                        // ... but if there is to be a new note...
                playNote(current);          //                                        ...play it.
            }            
        } 
        else 
        {                                   // If note is on...
            if (!noteStop[current]) {       //                  ...and should be...
                updateNote(current);        //                                      ...update it.
            } else {                        //    ... but if it should be stopped...
                stopNote(current);          //                                      ...stop it.
            }
        }
    }

    // Read MIDI/OSC Input (not yet used)

    switch (INTERFACE) {
    case 'O' :                                  // Read OSC Input 
        // OSC reading not yet implemented
        // OSC evaluation not yet implemented
        break;
    case 'U' :                                  // Read MIDI-over-USB Input 
        // MIDI-over-USB reading not yet implemented
        // MIDI-over-USB evaluation not yet implemented
        break;
    case 'M' :                                  // Read MIDI Input
        MIDI.read();
        // MIDI evaluation not yet implemented
        break;
    }
}


void readRibbon(int current) {
    digitalWrite(ribbonPin1[current], HIGH);
    digitalWrite(ribbonPin2[current], LOW);
    int reading1 = analogRead(ribbonPinRead[current]); // read one way
    digitalWrite(ribbonPin2[current], HIGH);
    digitalWrite(ribbonPin1[current], LOW);
    int reading2 = analogRead(ribbonPinRead[current]); // read the other way
    
    if (abs((reading1 + reading2)-1023) < ribbonSensitivity) {
        // if reading is not zero...
        
        // subtract the last reading from the total:
        ribbonReadingTotal[current] -= ribbonReadingList[current][ribbonReadingIndex[current]];
        // replace last reading with new one:
        ribbonReadingList[current][ribbonReadingIndex[current]] = reading1;
        // add the reading to the total:
        ribbonReadingTotal[current] += ribbonReadingList[current][ribbonReadingIndex[current]];
        // advance to the next position in the array:
        ribbonReadingIndex[current]++;
        
        // if we're at the end of the array...
        if (ribbonReadingIndex[current] >= NUMBEROFREADINGS) {
            // ...wrap around to the beginning:
            ribbonReadingIndex[current] = 0;
        }
        
        // calculate the average:
        ribbonReadingAverage[current] = ribbonReadingTotal[current] / NUMBEROFREADINGS;

        // if currentReading = averagedReading, set as established reading.
        if (reading1 == ribbonReadingAverage[current]) {
            ribbonReadingEstablished[current] = reading1;
            
            // now that there's an established reading,
            if (!noteRunning) {  // if the note's not on...
                noteStart[current] = 1;     // start it.
            }
        }
    } else {
        // if reading is zero...
        
        if (noteRunning) {   // and note is currently on...            
                noteStop[current] = 1;  // then stop it.
        }
    }
}

void playNote(int current) {
    noteRunning[current] = 1;
    noteStart[current] = 0;
    ribbonEnvelope[current] = 'A';
    ribbonVolume[current] = 0;

    calculatePitch(current);
    calculateVolumeChange(current);

    switch (INTERFACE) {
        case 'S' :  // Serial Interface (debugging)
            Serial.print("Note on: (Note #, Volume, Channel) ");
            Serial.print(ribbonNoteNumber[current]);
            Serial.print(" ");
            Serial.print(ribbonVolume[current]);
            Serial.print(" ");
            Serial.println(ribbonChannel[current]);
            break;
        case 'O' :  // OSC Interface (not yet implemented)
            break;
        case 'U' :  // MIDI-over-USB Interface (not yet implemented)
            break;
        default :   // MIDI Interface
            MIDI.sendNoteOn(ribbonNoteNumber[current], ribbonVolume[current], ribbonChannel[current]);
            break;
    }
}

void stopNote(int current) {
    noteStop[current] = 0;
    ribbonEnvelope[current] = 'D';
}

void updateNote(int current) {
    int oldNoteNumber = ribbonNoteNumber[current];
    int oldPitchBend = ribbonPitchBend[current];
    int oldVolume = ribbonVolume[current];
    
    calculatePitch(current);
    calculateVolumeChange(current);
    
    if (oldNoteNumber != ribbonNoteNumber[current]) {                                       // if note changed (can only happen in Note or Lookup modes)
        switch (INTERFACE) {
            case 'S' :  // Serial Interface (debugging)
                Serial.print("New Ribbon Reading: (Number) ");
                Serial.print(ribbonReadingEstablished[current]);
                if (PLAYMODE != 'P') {
                    Serial.print("New Note Number: (Number, Channel) ");
                    Serial.print(ribbonNoteNumber[current], ribbonChannel[current]);
                }
                break;
            case 'O' :  // OSC Interface (not yet implemented)
                break;
            case 'U' :  // MIDI-over-USB Interface (not yet implemented)
                break;
            default :   // MIDI Interface
                MIDI.sendControlChange(123, 0, ribbonChannel[current]);                                    // ...kill old one...
                MIDI.sendNoteOn(ribbonNoteNumber[current], ribbonVolume[current], ribbonChannel[current]);  // then start new one
                break;
        }
    }
    if (oldPitchBend != ribbonPitchBend[current]) {                                         // send pitchbend if changed...
        switch (INTERFACE) {
            case 'S' :  // Serial Interface (debugging)
                Serial.print("Pitch bend: (Bend, Channel) ");
                Serial.print(ribbonPitchBend[current], ribbonChannel[current]);
                break;
            case 'O' :  // OSC Interface (not yet implemented)
                break;
            case 'U' :  // MIDI-over-USB Interface (not yet implemented)
                break;
            default :   // MIDI Interface
                MIDI.sendPitchBend(ribbonPitchBend[current], ribbonChannel[current]);
                break;
        }
    }
    if (oldVolume != ribbonVolume[current]) {                                               // send volume if changed
        switch (INTERFACE) {
            case 'S' :  // Serial Interface (debugging)
                Serial.print("  Volume: (Volume, Channel) ");
                Serial.println(ribbonVolume[current], ribbonChannel[current]);
                break;
            case 'O' :  // OSC Interface (not yet implemented)
                break;
            case 'U' :  // MIDI-over-USB Interface (not yet implemented)
                break;
            default :   // MIDI Interface
                MIDI.sendControlChange(7, ribbonVolume[current], ribbonChannel[current]);
                break;
        }
    }
    if ( ribbonVolume[current] == 0 ) {                                                     // if volume = 0, send all notes off.
        noteRunning[current] = 0;                                                           // and turn off noteRunning
        switch (INTERFACE) {
            case 'S' :  // Serial Interface (debugging)
                Serial.println("All notes off.");
                break;
            case 'O' :  // OSC Interface (not yet implemented)
                break;
            case 'U' :  // MIDI-over-USB Interface (not yet implemented)
                break;
            default :   // MIDI Interface
                MIDI.sendControlChange(123, 0, ribbonChannel[current]);
                break;
        }
    }
}

void calculatePitch(int current) {
    switch (PLAYMODE) 
    {
        case 'P' :  // Pitchbend Mode
            ribbonNoteNumber[current] = singleNote[current];
            ribbonPitchBend[current] = map(ribbonReadingEstablished[current], 0, 1023, -8192, 8191);
            break;
        case 'N' :  // Notes Mode
            ribbonNoteNumber[current] = map(ribbonReadingEstablished[current], 0, 1023, lowNote[current], highNote[current]);
            ribbonPitchBend[current] = 0;
            break;
        case 'L' :  // Lookup Table (Notes and Pitchbend) Mode
            // Magic happens here.
            break;
    }
}

void calculateVolumeChange(int current) {
    switch(ribbonEnvelope[current]) 
    {
        case 'A' :                                        // if we're in the attack of a note,
            ribbonVolume[current] += attackRate;          //    increase the volume by specified amount
            if ( ribbonVolume[current] >= maxVolume ) {   //     if note is equal to or above max,
                ribbonVolume[current] = maxVolume;        //      set volume to max
                ribbonEnvelope[current] = 'N';            //      and stop attack.
            }
            break;
        case 'D' :                                        // if we're in the decay of a note,
            ribbonVolume[current] -= decayRate;           //    decrease the volume by specified amount
            if ( ribbonVolume[current] <= 0 ) {           //     if note is equal to or below zero,
                ribbonVolume[current] = 0;                //      set volume to zero,
                ribbonEnvelope[current] = 'N';            //      and stop decay.
            }
            break;
    }
}


