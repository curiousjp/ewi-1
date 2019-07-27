#include <MIDI.h>
#include <Wire.h>

MIDI_CREATE_DEFAULT_INSTANCE();

#define ANALOG_PIN A6
#define DIGITAL_PIN1 5
#define DIGITAL_PIN2 6
#define DIGITAL_PIN3 7

// these numbers determined by observation
// of raw returns from analogRead()
#define SENSOR_L 92
#define SENSOR_H 1023
#define SENSOR_THRESHOLD 120

// wind controllers are supposed to use
// continuous controller two, but I struggle
// to find patches that understand it. seven
// is straight up "volume" and will do the job
// for being able to direct swells
//#define MIDI_CC 2
#define MIDI_CC 7
#define MIDI_CHANNEL 1

#define STATE_SILENT 0
#define STATE_SOUNDING 1

// send CC updates no more often than X milliseconds
#define CC_INTERVAL 70

// The following material relates to the DS1803 dual digital potentiometer functions
#define DS1803 1
#ifdef DS1803
#define DS_CONTROLBYTE  B0101000 // 0101 + Address Zero (Wire library appends the last bit.)
#define DS_COMMANDBYTE  B10101111 // Write both pots with one value
#endif


int unitState = STATE_SILENT;
int currentNote = 0;
int currentCCState = 0;
unsigned long lastCCSent = 0;

const int scale[8] = { 0x3C, 0x3F, 0x41, 0x42, 0x43, 0x46, 0x48, 0x4B };
#define GETNOTE() scale[ !digitalRead( DIGITAL_PIN1 ) | ( !digitalRead( DIGITAL_PIN2 )<<1 ) | ( !digitalRead( DIGITAL_PIN3 ) << 2 ) ]

void setup() {
  pinMode( DIGITAL_PIN1, INPUT_PULLUP );
  pinMode( DIGITAL_PIN2, INPUT_PULLUP );
  pinMode( DIGITAL_PIN3, INPUT_PULLUP );
  unitState = STATE_SILENT;
#ifdef DS1803
  Wire.begin();
#endif
  MIDI.begin();
  Serial.begin(115200);
  delay(1000);
  MIDI.sendProgramChange(22, MIDI_CHANNEL);
}

void loop() {
  int sensorValue = max( analogRead( ANALOG_PIN ), SENSOR_L );

#ifdef DS1803
  Wire.beginTransmission( DS_CONTROLBYTE );
  Wire.write( DS_COMMANDBYTE );
  Wire.write( byte( 255 - map( sensorValue, SENSOR_L, SENSOR_H, 0, 255 ) ) );
  Wire.endTransmission();
#endif

  if( unitState == STATE_SILENT ) {
    if( sensorValue >= SENSOR_THRESHOLD ) {
      currentNote = GETNOTE();
      currentCCState = map( sensorValue, SENSOR_THRESHOLD, SENSOR_H, 1, 127 );
      MIDI.sendControlChange( MIDI_CC, currentCCState, MIDI_CHANNEL );
      MIDI.sendNoteOn( currentNote, 127, MIDI_CHANNEL );
      lastCCSent = millis();
      unitState = STATE_SOUNDING;
    }
    return;
  }

  if( unitState == STATE_SOUNDING && sensorValue < SENSOR_THRESHOLD ) {
      MIDI.sendNoteOff( currentNote, 0, MIDI_CHANNEL );
      unitState = STATE_SILENT;
      return;
  }

  if( unitState == STATE_SOUNDING ) {
    int newNote = GETNOTE();
    int newCCState = map( sensorValue, SENSOR_THRESHOLD, SENSOR_H, 1, 127 );

    // deal with potential changes in CC state
    if( newCCState != currentCCState && ( millis() - lastCCSent >= CC_INTERVAL ) ) {
      currentCCState = newCCState;
      lastCCSent = millis();
      MIDI.sendControlChange( MIDI_CC, currentCCState, MIDI_CHANNEL );
    }

    // deal with potential changes in note
    if( newNote != currentNote ) {
        MIDI.sendNoteOff( currentNote, 0, MIDI_CHANNEL );
        MIDI.sendNoteOn( newNote, 127, MIDI_CHANNEL );
        currentNote = newNote;
    }
    return;
  }
}
