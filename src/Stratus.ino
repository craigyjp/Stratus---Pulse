/*
  Stratus Pulse 12 bit DAC - Firmware Rev 1.5

  Includes code by:
    Dave Benn - Handling MUXs, a few other bits and original inspiration  https://www.notesandvolts.com/2019/01/teensy-synth-part-10-hardware.html

  Arduino IDE
  Tools Settings:
  Board: "Teensy3.6"
  USB Type: "Serial + MIDI"
  CPU Speed: "180"
  Optimize: "Fastest"

  Additional libraries:
    Agileware CircularBuffer available in Arduino libraries manager
    Replacement files are in the Modified Libraries folder and need to be placed in the teensy Audio folder.
*/

#include <SPI.h>
#include <SD.h>
#include <MIDI.h>
#include "MidiCC.h"
#include "Constants.h"
#include "Parameters.h"
#include "PatchMgr.h"
#include "HWControls.h"
#include "EepromMgr.h"
#include "Settings.h"
#include <ShiftRegister74HC595.h>
#include <RoxMux.h>

#define PARAMETER 0      //The main page for displaying the current patch and control (parameter) changes
#define RECALL 1         //Patches list
#define SAVE 2           //Save patch page
#define REINITIALISE 3   // Reinitialise message
#define PATCH 4          // Show current patch bypassing PARAMETER
#define PATCHNAMING 5    // Patch naming page
#define DELETE 6         //Delete patch page
#define DELETEMSG 7      //Delete patch message page
#define SETTINGS 8       //Settings page
#define SETTINGSVALUE 9  //Settings page

unsigned int state = PARAMETER;

#include "ST7735Display.h"

boolean cardStatus = false;

//MIDI 5 Pin DIN
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);  //RX - Pin 0

int patchNo = 0;
unsigned long buttonDebounce = 0;

//
//Shift Register setup
//
ShiftRegister74HC595<4> srpanel(6, 8, 7);

#define OCTO_TOTAL 4
#define BTN_DEBOUNCE 50
RoxOctoswitch<OCTO_TOTAL, BTN_DEBOUNCE> octoswitch;

// pins for 74HC165
#define PIN_DATA 35  // pin 9 on 74HC165 (DATA)
#define PIN_LOAD 34  // pin 1 on 74HC165 (LOAD)
#define PIN_CLK 33   // pin 2 on 74HC165 (CLK))

#define MUX_TOTAL 4
Rox74HC595<MUX_TOTAL> srp;
//
// pins for 74HC595
#define OUT_DATA 29   // pin 14 on 74HC595 (DATA)
#define OUT_CLK 39    // pin 11 on 74HC595 (CLK)
#define OUT_LATCH 38  // pin 12 on 74HC595 (LATCH)

void setup() {
  // SPI.setDataMode(SPI_MODE1);
  SPI.begin();
  setupDisplay();
  setUpSettings();
  setupHardware();

  cardStatus = SD.begin(BUILTIN_SDCARD);
  if (cardStatus) {
    Serial.println("SD card is connected");
    //Get patch numbers and names from SD card
    loadPatches();
    if (patches.size() == 0) {
      //save an initialised patch to SD card
      setVolumeZero();
      savePatch("1", INITPATCH);
      loadPatches();
    }
  } else {
    Serial.println("SD card is not connected or unusable");
    reinitialiseToPanel();
    showPatchPage("No SD", "conn'd / usable");
  }

  //Read MIDI Channel from EEPROM
  midiChannel = getMIDIChannel();
  Serial.println("MIDI Ch:" + String(midiChannel) + " (0 is Omni On)");

  //USB Client MIDI
  usbMIDI.setHandleControlChange(myControlChangeMunge);
  usbMIDI.setHandleProgramChange(myProgramChange);
  Serial.println("USB Client MIDI Listening");

  //MIDI 5 Pin DIN
  MIDI.begin(midiChannel);
  MIDI.setHandleControlChange(myControlChangeMunge);
  MIDI.setHandleProgramChange(myProgramChange);
  Serial.println("MIDI In DIN Listening");

  //Read Encoder Direction from EEPROM
  encCW = getEncoderDir();
  patchNo = getLastPatch();

  //  reinitialiseToPanel();

  octoswitch.begin(PIN_DATA, PIN_LOAD, PIN_CLK);
  octoswitch.setCallback(onButtonPress);
  octoswitch.setIgnoreAfterHold(25, true);

  srp.begin(OUT_DATA, OUT_CLK, OUT_LATCH);
  srpanel.set(EFFECT_LED, LOW);
  srpanel.set(LFO_LED, HIGH);
  srp.writePin(SIGNAL_MUTE, LOW);
  recallPatch(patchNo);  //Load first patch
}

void myControlChangeMunge(byte channel, byte number, byte value) {

  int newvalue = value << 3;
  myControlChange(channel, number, newvalue);
}

void allNotesOff() {
}

void setVolumeZero() {
  digitalWriteFast(DEMUX_S0, 0);
  digitalWriteFast(DEMUX_S1, 0);
  digitalWriteFast(DEMUX_S2, 1);
  digitalWriteFast(DEMUX_S3, 0);
  setVoltage(CHIP_SEL1, 1, 1, 0);  // volume
}

void getStratusOSCWaveform(int value) {
  switch (value) {

    case 0:
      StratusOSCWaveform = "Heeyaw";
      break;

    case 1:
      StratusOSCWaveform = "Foonik";
      break;

    case 2:
      StratusOSCWaveform = "RubberNaught";
      break;

    case 3:
      StratusOSCWaveform = "Woppam";
      break;

    case 4:
      StratusOSCWaveform = "Claribel";
      break;

    case 5:
      StratusOSCWaveform = "Dist Bell";
      break;

    case 6:
      StratusOSCWaveform = "Mawkish";
      break;

    case 7:
      StratusOSCWaveform = "Quagmire";
      break;

    case 8:
      StratusOSCWaveform = "Sine";
      break;

    case 9:
      StratusOSCWaveform = "Sine Octave";
      break;

    case 10:
      StratusOSCWaveform = "Sawtooth";
      break;

    case 11:
      StratusOSCWaveform = "Pulse";
      break;

    case 12:
      StratusOSCWaveform = "Hibox";
      break;

    case 13:
      StratusOSCWaveform = "Wipeout";
      break;

    case 14:
      StratusOSCWaveform = "DucksBack";
      break;

    case 15:
      StratusOSCWaveform = "Quiqui";
      break;
  }
}

void getStratusSUBWaveform(int value) {
  switch (value) {

    case 0:
      StratusSUBWaveform = "Boulud";
      break;

    case 1:
      StratusSUBWaveform = "Boulud +1";
      break;

    case 2:
      StratusSUBWaveform = "Boulud +2";
      break;

    case 3:
      StratusSUBWaveform = "Boulud +3";
      break;

    case 4:
      StratusSUBWaveform = "Kookop";
      break;

    case 5:
      StratusSUBWaveform = "Kookop +1";
      break;

    case 6:
      StratusSUBWaveform = "Kookop +2";
      break;

    case 7:
      StratusSUBWaveform = "Kookop +3";
      break;

    case 8:
      StratusSUBWaveform = "Oct Brass";
      break;

    case 9:
      StratusSUBWaveform = "Oct Brass +1";
      break;

    case 10:
      StratusSUBWaveform = "Oct Brass +2";
      break;

    case 11:
      StratusSUBWaveform = "Oct Brass +3";
      break;

    case 12:
      StratusSUBWaveform = "Clarinet";
      break;

    case 13:
      StratusSUBWaveform = "Clarinet +1";
      break;

    case 14:
      StratusSUBWaveform = "Clarinet +2";
      break;

    case 15:
      StratusSUBWaveform = "Clarinet +3";
      break;

    case 16:
      StratusSUBWaveform = "Sinewave";
      break;

    case 17:
      StratusSUBWaveform = "Sinewave +1";
      break;

    case 18:
      StratusSUBWaveform = "Sinewave +2";
      break;

    case 19:
      StratusSUBWaveform = "Sinewave +3";
      break;

    case 20:
      StratusSUBWaveform = "Sine Octave";
      break;

    case 21:
      StratusSUBWaveform = "Sine Oct +1";
      break;

    case 22:
      StratusSUBWaveform = "Sine Oct +2";
      break;

    case 23:
      StratusSUBWaveform = "Sine Oct +3";
      break;

    case 24:
      StratusSUBWaveform = "Sawtooth";
      break;

    case 25:
      StratusSUBWaveform = "Sawtooth +1";
      break;

    case 26:
      StratusSUBWaveform = "Sawtooth +2";
      break;

    case 27:
      StratusSUBWaveform = "Sawtooth +3";
      break;

    case 28:
      StratusSUBWaveform = "Pulse";
      break;

    case 29:
      StratusSUBWaveform = "Pulse +1";
      break;

    case 30:
      StratusSUBWaveform = "Pulse +2";
      break;

    case 31:
      StratusSUBWaveform = "Pulse +3";
      break;
  }
}

void updateUnisonSwitch() {
  if (unisonSW == 1) {
    showCurrentParameterPage("Unison", "On");
    srpanel.set(UNISON_LED, HIGH);  // LED on
    srp.writePin(UNISON_OUT, HIGH);
  } else {
    showCurrentParameterPage("Unison", "Off");
    srpanel.set(UNISON_LED, LOW);  // LED off
    srp.writePin(UNISON_OUT, LOW);
  }
}

void updateglide() {
  if (glide == 1) {
    showCurrentParameterPage("Glide", "On");
    srpanel.set(GLIDE_LED, HIGH);  // LED on
    srp.writePin(GLIDE_OUT, HIGH);
  } else {
    showCurrentParameterPage("Glide", "Off");
    srpanel.set(GLIDE_LED, LOW);  // LED off
    srp.writePin(GLIDE_OUT, LOW);
  }
}

void updateoctave1() {
  if (octave1 == 0) {
    showCurrentParameterPage("Octave 1", "Off");
    srpanel.set(OCTAVE1_LED, LOW);  // LED off
    srp.writePin(OCTAVE1_OUT, LOW);
  } else {
    showCurrentParameterPage("Octave 1", "On");
    srpanel.set(OCTAVE1_LED, HIGH);  // LED on
    srp.writePin(OCTAVE1_OUT, HIGH);
  }
}

void updateoctave2() {
  if (octave2 == 0) {
    showCurrentParameterPage("Octave 2", "Off");
    srpanel.set(OCTAVE2_LED, LOW);  // LED off
    srp.writePin(OCTAVE2_OUT, LOW);
  } else {
    showCurrentParameterPage("Octave 2", "On");
    srpanel.set(OCTAVE2_LED, HIGH);  // LED on
    srp.writePin(OCTAVE2_OUT, HIGH);
  }
}

void updateoctaveup() {
  if (octaveup == 1) {
    showCurrentParameterPage("KeyBoard", "Octave Up");
    srpanel.set(OCTAVE, HIGH);
    delay(50);
    srpanel.set(PLUS, HIGH);
    delay(20);
    srpanel.set(PLUS, LOW);
    srpanel.set(OCTAVE, LOW);
  }
}

void updateoctavedown() {
  if (octavedown == 1) {
    showCurrentParameterPage("Keyboard", "Octave Down");
    srpanel.set(OCTAVE, HIGH);
    delay(50);
    srpanel.set(MINUS, HIGH);
    delay(20);
    srpanel.set(MINUS, LOW);
    srpanel.set(OCTAVE, LOW);
  }
}

void updatefilterKeyTrack() {
  if (filterKeyTrack == 0) {
    showCurrentParameterPage("Filter KeyTrack", "Off");
    srpanel.set(KEYTRK_LED, LOW);  // LED off
    srp.writePin(KEYTRK_OUT, LOW);
  } else {
    showCurrentParameterPage("Filter KeyTrack", "On");
    srpanel.set(KEYTRK_LED, HIGH);  // LED on
    srp.writePin(KEYTRK_OUT, HIGH);
  }
}

void updatefilterPole() {
  if (filterPole == 0) {
    //showCurrentParameterPage("Filter Pole", "Low Pass");
    srpanel.set(POLE_LED, LOW);  // LED off
    srp.writePin(POLE_OUT, LOW);
  } else {
    //showCurrentParameterPage("Filter Pole", "High Pass");
    srpanel.set(POLE_LED, HIGH);  // LED on
    srp.writePin(POLE_OUT, HIGH);
  }
  updateFilterType();
}

void updatefilterEGinv() {
  if (filterEGinv == 0) {
    showCurrentParameterPage("Filter Env", "Positive");
    srpanel.set(EG_INV_LED, LOW);  // LED off
    srp.writePin(EG_INV_OUT, LOW);
  } else {
    showCurrentParameterPage("Filter Env", "Negative");
    srpanel.set(EG_INV_LED, HIGH);  // LED on
    srp.writePin(EG_INV_OUT, HIGH);
  }
}

void updatefilterLoop() {
  if (filterLoop == 0) {
    showCurrentParameterPage("Gated Looping", "Off");
    srpanel.set(LOOP_LED, LOW);
    srp.writePin(LOOP_OUT, LOW);
  } else {
    showCurrentParameterPage("Gated Looping", "On");
    srpanel.set(LOOP_LED, HIGH);  // LED on
    srp.writePin(LOOP_OUT, HIGH);
  }
}

void updatepitchEG() {
  if (pitchEG == 1) {
    showCurrentParameterPage("Envelope", "Pitch");
    srpanel.set(PITCH_EG_LED, HIGH);  // LED on
    srpanel.set(VCF_EG_LED, LOW);     // LED off
    srpanel.set(VCA_EG_LED, LOW);     // LED off
    filterEG = 0;
    ampEG = 0;
    if (pitchVelo == 1 && pitchEG == 1) {
      srpanel.set(VELO_LED, HIGH);  // LED On
      srp.writePin(PITCH_VELO_OUT, HIGH);
    } else {
      srpanel.set(VELO_LED, LOW);  // LED off
      srp.writePin(PITCH_VELO_OUT, LOW);
    }
  }
}

void updatefilterEG() {
  if (filterEG == 1) {
    showCurrentParameterPage("Envelope", "Filter");
    srpanel.set(VCF_EG_LED, HIGH);   // LED on
    srpanel.set(PITCH_EG_LED, LOW);  // LED off
    srpanel.set(VCA_EG_LED, LOW);    // LED off
    pitchEG = 0;
    ampEG = 0;
    if (filterVelo == 1 && filterEG == 1) {
      srpanel.set(VELO_LED, HIGH);  // LED On
      srp.writePin(FILTER_VELO_OUT, HIGH);
    } else {
      srpanel.set(VELO_LED, LOW);  // LED off
      srp.writePin(FILTER_VELO_OUT, LOW);
    }
  }
}

void updateampEG() {
  if (ampEG == 1) {
    showCurrentParameterPage("Envelope", "Amplifier");
    srpanel.set(VCA_EG_LED, HIGH);   // LED on
    srpanel.set(PITCH_EG_LED, LOW);  // LED off
    srpanel.set(VCF_EG_LED, LOW);    // LED off
    filterEG = 0;
    pitchEG = 0;
    if (ampVelo == 1 && ampEG == 1) {
      srpanel.set(VELO_LED, HIGH);  // LED On
      srp.writePin(AMP_VELO_OUT, HIGH);
    } else {
      srpanel.set(VELO_LED, LOW);  // LED off
      srp.writePin(AMP_VELO_OUT, LOW);
    }
  }
}

void updatevelocitySW() {
  if (velocitySW == 1) {
    if (pitchEG == 1) {
      showCurrentParameterPage("Pitch Velocity", "On");
      srpanel.set(VELO_LED, HIGH);  // LED On
      srp.writePin(PITCH_VELO_OUT, HIGH);
      pitchVelo = 1;
      velocitySW = 1;
    } else if (filterEG == 1) {
      showCurrentParameterPage("Filter Velocity", "On");
      srpanel.set(VELO_LED, HIGH);  // LED On
      srp.writePin(FILTER_VELO_OUT, HIGH);
      filterVelo = 1;
      velocitySW = 1;
    } else if (ampEG == 1) {
      showCurrentParameterPage("Amp Velocity", "On");
      srpanel.set(VELO_LED, HIGH);  // LED On
      srp.writePin(AMP_VELO_OUT, HIGH);
      ampVelo = 1;
      velocitySW = 1;
    }
  } else {
    if (pitchEG == 1) {
      showCurrentParameterPage("Pitch Velocity", "Off");
      srpanel.set(VELO_LED, LOW);  // LED off
      srp.writePin(PITCH_VELO_OUT, LOW);
      pitchVelo = 0;
      velocitySW = 0;
    } else if (filterEG == 1) {
      showCurrentParameterPage("Filter Velocity", "Off");
      srpanel.set(VELO_LED, LOW);  // LED off
      srp.writePin(FILTER_VELO_OUT, LOW);
      filterVelo = 0;
      velocitySW = 0;
    } else if (ampEG == 1) {
      showCurrentParameterPage("Amp Velocity", "Off");
      srpanel.set(VELO_LED, LOW);  // LED off
      srp.writePin(AMP_VELO_OUT, LOW);
      ampVelo = 0;
      velocitySW = 0;
    }
  }
}

void updateRecallvelocitySW() {
  if (pitchVelo == 1 && pitchEG == 1) {
    srpanel.set(VELO_LED, HIGH);  // LED On
    srp.writePin(PITCH_VELO_OUT, HIGH);
  } else if (filterVelo == 1 && filterEG == 1) {
    srpanel.set(VELO_LED, HIGH);  // LED On
    srp.writePin(FILTER_VELO_OUT, HIGH);
  } else if (ampVelo == 1 && ampEG == 1) {
    srpanel.set(VELO_LED, HIGH);  // LED On
    srp.writePin(AMP_VELO_OUT, HIGH);
  } else {
    srpanel.set(VELO_LED, LOW);  // LED On
    srp.writePin(PITCH_VELO_OUT, LOW);
    srp.writePin(FILTER_VELO_OUT, LOW);
    srp.writePin(AMP_VELO_OUT, LOW);
  }
}

void updatelfoDestVCO() {
  if (lfoDestVCO == 0) {
    showCurrentParameterPage("LFO to VCO", "Off");
    srpanel.set(VCO_SW_LED, LOW);  // LED off
    srp.writePin(VCO_OUT, LOW);
  } else {
    showCurrentParameterPage("LFO to VCO", "On");
    srpanel.set(VCO_SW_LED, HIGH);  // LED on
    srp.writePin(VCO_OUT, HIGH);
  }
}

void updatelfoDestVCF() {
  if (lfoDestVCF == 0) {
    showCurrentParameterPage("LFO to VCF", "Off");
    srpanel.set(VCF_SW_LED, LOW);  // LED off
    srp.writePin(VCF_OUT, LOW);
  } else {
    showCurrentParameterPage("LFO to VCF", "On");
    srpanel.set(VCF_SW_LED, HIGH);  // LED on
    srp.writePin(VCF_OUT, HIGH);
  }
}

void updatelfoDestVCA() {
  if (lfoDestVCA == 0) {
    showCurrentParameterPage("LFO to VCA", "Off");
    srpanel.set(VCA_SW_LED, LOW);  // LED off
    srp.writePin(VCA_OUT, LOW);
  } else {
    showCurrentParameterPage("LFO to VCA", "On");
    srpanel.set(VCA_SW_LED, HIGH);  // LED on
    srp.writePin(VCA_OUT, HIGH);
  }
}

void updateMonoMulti() {
  if (monoMulti == 0) {
    showCurrentParameterPage("LFO Retrigger", "Off");
    srpanel.set(MULTI_SW_LED, LOW);  // LED off
    srp.writePin(MULTI_OUT, LOW);
  } else {
    showCurrentParameterPage("LFO Retrigger", "On");
    srpanel.set(MULTI_SW_LED, HIGH);  // LED on
    srp.writePin(MULTI_OUT, HIGH);
  }
}

void updateEffects() {
  if (effects) {
    showCurrentParameterPage("Effects", "On");
  } else {
    showCurrentParameterPage("LFO Control", "On");
  }
}

void updateosc1EGinv() {
  if (osc1EGinv == 0) {
    showCurrentParameterPage("Osc1 EG Invert", "Off");
    srpanel.set(OSC1_EG_LED, LOW);  // LED off
    srp.writePin(OSC1_EG_OUT, LOW);
  } else {
    showCurrentParameterPage("Osc1 EG Invert", "On");
    srpanel.set(OSC1_EG_LED, HIGH);  // LED on
    srp.writePin(OSC1_EG_OUT, HIGH);
  }
}

void updateosc2EGon() {
  if (osc2EGon == 0) {
    showCurrentParameterPage("Osc2 EG", "Off");
    srpanel.set(OSC2_EG_LED, LOW);  // LED off
    srp.writePin(OSC2_EG_OUT, LOW);
  } else {
    showCurrentParameterPage("Osc2 EG", "On");
    srpanel.set(OSC2_EG_LED, HIGH);  // LED on
    srp.writePin(OSC2_EG_OUT, HIGH);
  }
}

void updatepot1() {
  switch (internal) {
    case 1:
      switch (effectNumber) {
        case 0:
          showCurrentParameterPage("Reverb Mix", int(pot1str));
          break;

        case 1:
          showCurrentParameterPage("Reverb Mix", int(pot1str));
          break;

        case 2:
          showCurrentParameterPage("Reverb Mix", int(pot1str));
          break;

        case 3:
          showCurrentParameterPage("Pitch", int(pot1str));
          break;

        case 4:
          showCurrentParameterPage("Pitch", int(pot1str));
          break;

        case 5:
          showCurrentParameterPage("-", int(pot1str));
          break;

        case 6:
          showCurrentParameterPage("Reverb Time", int(pot1str));
          break;

        case 7:
          showCurrentParameterPage("Reverb Time", int(pot1str));
          break;
      }
      break;

    case 0:
      switch (effectNumber) {
        case 0:
          showCurrentParameterPage("Phaser Rate", int(pot1str));
          break;

        case 1:
          showCurrentParameterPage("Flanger Rate", int(pot1str));
          break;

        case 2:
          showCurrentParameterPage("Delay 1", int(pot1str));
          break;

        case 3:
          showCurrentParameterPage("Delay Time", int(pot1str));
          break;

        case 4:
          showCurrentParameterPage("Low Pass", int(pot1str));
          break;

        case 5:
          showCurrentParameterPage("Phaser Rate", int(pot1str));
          break;

        case 6:
          showCurrentParameterPage("Delay", int(pot1str));
          break;

        case 7:
          showCurrentParameterPage("Intensity", int(pot1str));
          break;
      }
      break;
  }
}

void updatepot2() {
    switch (internal) {
    case 1:
      switch (effectNumber) {
        case 0:
          showCurrentParameterPage("Chorus Rate", int(pot2str));
          break;

        case 1:
          showCurrentParameterPage("Flange Rate", int(pot2str));
          break;

        case 2:
          showCurrentParameterPage("Trem Rate", int(pot2str));
          break;

        case 3:
          showCurrentParameterPage("-", int(pot2str));
          break;

        case 4:
          showCurrentParameterPage("Echo Delay", int(pot2str));
          break;

        case 5:
          showCurrentParameterPage("-", int(pot2str));
          break;

        case 6:
          showCurrentParameterPage("HF Filter", int(pot2str));
          break;

        case 7:
          showCurrentParameterPage("HF Filter", int(pot2str));
          break;
      }
      break;

    case 0:
      switch (effectNumber) {
        case 0:
          showCurrentParameterPage("Phaser Depth", int(pot2str));
          break;

        case 1:
          showCurrentParameterPage("Flanger Depth", int(pot2str));
          break;

        case 2:
          showCurrentParameterPage("Delay 2", int(pot2str));
          break;

        case 3:
          showCurrentParameterPage("Flange LFO", int(pot2str));
          break;

        case 4:
          showCurrentParameterPage("High Pass", int(pot2str));
          break;

        case 5:
          showCurrentParameterPage("Phaser Depth", int(pot2str));
          break;

        case 6:
          showCurrentParameterPage("Mix", int(pot2str));
          break;

        case 7:
          showCurrentParameterPage("Rate", int(pot2str));
          break;
      }
      break;
  }
}

void updatepot3() {
    switch (internal) {
    case 1:
      switch (effectNumber) {
        case 0:
          showCurrentParameterPage("Chorus Mix", int(pot3str));
          break;

        case 1:
          showCurrentParameterPage("Flange Mix", int(pot3str));
          break;

        case 2:
          showCurrentParameterPage("Trem Mix", int(pot3str));
          break;

        case 3:
          showCurrentParameterPage("-", int(pot3str));
          break;

        case 4:
          showCurrentParameterPage("Echo Mix", int(pot3str));
          break;

        case 5:
          showCurrentParameterPage("-", int(pot3str));
          break;

        case 6:
          showCurrentParameterPage("LF Filter", int(pot3str));
          break;

        case 7:
          showCurrentParameterPage("LF Filter", int(pot3str));
          break;
      }
      break;

    case 0:
      switch (effectNumber) {
        case 0:
          showCurrentParameterPage("Feedback", int(pot3str));
          break;

        case 1:
          showCurrentParameterPage("Feedback", int(pot3str));
          break;

        case 2:
          showCurrentParameterPage("Delay 3", int(pot3str));
          break;

        case 3:
          showCurrentParameterPage("Feedback", int(pot3str));
          break;

        case 4:
          showCurrentParameterPage("Volume", int(pot3str));
          break;

        case 5:
          showCurrentParameterPage("Feedback", int(pot3str));
          break;

        case 6:
          showCurrentParameterPage("Volume", int(pot3str));
          break;

        case 7:
          showCurrentParameterPage("Mix", int(pot3str));
          break;
      }
      break;
  }
}

void updatemix() {
  showCurrentParameterPage("Effect Mix", int(mixstr));
}

void updateinternal() {
  switch (internal) {
    case 1:
      srp.writePin(INT_EXT, LOW);
      break;

    case 0:
      srp.writePin(INT_EXT, HIGH);
      break;
  }
  updateeffectNumber();
}

void updateeffectNumber() {
  switch (internal) {
    case 1:
      switch (effectNumber) {
        case 0:
          FV1effect = "Chorus Rev";
          srp.writePin(EFFECT_S0, LOW);
          srp.writePin(EFFECT_S1, LOW);
          srp.writePin(EFFECT_S2, LOW);
          break;

        case 1:
          FV1effect = "Flange Rev";
          srp.writePin(EFFECT_S0, HIGH);
          srp.writePin(EFFECT_S1, LOW);
          srp.writePin(EFFECT_S2, LOW);
          break;

        case 2:
          FV1effect = "Tremelo Rev";
          srp.writePin(EFFECT_S0, LOW);
          srp.writePin(EFFECT_S1, HIGH);
          srp.writePin(EFFECT_S2, LOW);
          break;

        case 3:
          FV1effect = "Pitch Shift";
          srp.writePin(EFFECT_S0, HIGH);
          srp.writePin(EFFECT_S1, HIGH);
          srp.writePin(EFFECT_S2, LOW);
          break;

        case 4:
          FV1effect = "Pitch Echo";
          srp.writePin(EFFECT_S0, LOW);
          srp.writePin(EFFECT_S1, LOW);
          srp.writePin(EFFECT_S2, HIGH);
          break;

        case 5:
          FV1effect = "No Effect";
          srp.writePin(EFFECT_S0, HIGH);
          srp.writePin(EFFECT_S1, LOW);
          srp.writePin(EFFECT_S2, HIGH);
          break;

        case 6:
          FV1effect = "Reverb 1";
          srp.writePin(EFFECT_S0, LOW);
          srp.writePin(EFFECT_S1, HIGH);
          srp.writePin(EFFECT_S2, HIGH);
          break;

        case 7:
          FV1effect = "Reverb 2";
          srp.writePin(EFFECT_S0, HIGH);
          srp.writePin(EFFECT_S1, HIGH);
          srp.writePin(EFFECT_S2, HIGH);
          break;
      }
      break;

    case 0:
      switch (effectNumber) {
        case 0:
          FV1effect = "Phaser Bass";
          srp.writePin(EFFECT_S0, LOW);
          srp.writePin(EFFECT_S1, LOW);
          srp.writePin(EFFECT_S2, LOW);
          break;

        case 1:
          FV1effect = "Flanger Bass";
          srp.writePin(EFFECT_S0, HIGH);
          srp.writePin(EFFECT_S1, LOW);
          srp.writePin(EFFECT_S2, LOW);
          break;

        case 2:
          FV1effect = "Triple Delay";
          srp.writePin(EFFECT_S0, LOW);
          srp.writePin(EFFECT_S1, HIGH);
          srp.writePin(EFFECT_S2, LOW);
          break;

        case 3:
          FV1effect = "6 Tap Delay";
          srp.writePin(EFFECT_S0, HIGH);
          srp.writePin(EFFECT_S1, HIGH);
          srp.writePin(EFFECT_S2, LOW);
          break;

        case 4:
          FV1effect = "Cabinet Sim";
          srp.writePin(EFFECT_S0, LOW);
          srp.writePin(EFFECT_S1, LOW);
          srp.writePin(EFFECT_S2, HIGH);
          break;

        case 5:
          FV1effect = "Parallax";
          srp.writePin(EFFECT_S0, HIGH);
          srp.writePin(EFFECT_S1, LOW);
          srp.writePin(EFFECT_S2, HIGH);
          break;

        case 6:
          FV1effect = "Choir Saw";
          srp.writePin(EFFECT_S0, LOW);
          srp.writePin(EFFECT_S1, HIGH);
          srp.writePin(EFFECT_S2, HIGH);
          break;

        case 7:
          FV1effect = "Spacedash";
          srp.writePin(EFFECT_S0, HIGH);
          srp.writePin(EFFECT_S1, HIGH);
          srp.writePin(EFFECT_S2, HIGH);
          break;
      }
      break;
  }
  showCurrentParameterPage("Effect", FV1effect);
}

void updateGlideTime() {
  showCurrentParameterPage("Glide Time", int(GlideTimestr));
}

void updatefilterRes() {
  showCurrentParameterPage("Resonance", int(filterResstr));
}

void updatevolumeControl() {
  showCurrentParameterPage("Volume", int(volumeControlstr));
}

void updateLfoDelay() {
  showCurrentParameterPage("LFO Delay", String(LfoDelaystr * 10) + " Seconds");
}

void updateNoiseLevel() {
  showCurrentParameterPage("Noise Level", String(noiseLevelstr));
}

void updateFilterCutoff() {
  showCurrentParameterPage("Cutoff", String(filterCutoffstr) + " Hz");
}

void updateosc1Detune() {
  showCurrentParameterPage("OSC1 Detune", String(osc1Detunestr));
}

void updateLfoSlope() {
  showCurrentParameterPage("LFO Slope", int(LfoSlopestr));
}

void updateLfoDepth() {
  showCurrentParameterPage("LFO Depth", int(LfoDepthstr));
}

void updatePitchBend() {
  showCurrentParameterPage("Bender Range", PitchBendLevelstr);
}

void updatemodWheel() {
  showCurrentParameterPage("Mod Range", int(modWheelLevelstr));
}

void updateLfoRate() {
  showCurrentParameterPage("LFO Rate", String(LfoRatestr) + " Hz");
}

void updatebitCrush() {
  switch (bitCrushstr) {
    case 0:
      showCurrentParameterPage("Bit Crush ", "8 Bits");
      break;

    case 1:
      showCurrentParameterPage("Bit Crush ", "7 Bits");
      break;

    case 2:
      showCurrentParameterPage("Bit Crush ", "6 Bits");
      break;

    case 3:
      showCurrentParameterPage("Bit Crush ", "5 Bits");
      break;

    case 4:
      showCurrentParameterPage("Bit Crush ", "4 Bits");
      break;

    case 5:
      showCurrentParameterPage("Bit Crush ", "3 Bits");
      break;

    case 6:
      showCurrentParameterPage("Bit Crush ", "2 Bits");
      break;

    case 7:
      showCurrentParameterPage("Bit Crush ", "1 Bit");
      break;
  }
}

void updateosc1SubLevel() {
  showCurrentParameterPage("OSC1 SUB", int(osc1SubLevelstr));
}

void updateosc1MainLevel() {
  showCurrentParameterPage("OSC1 Main", int(osc1MainLevelstr));
}

void updateosc2SubLevel() {
  showCurrentParameterPage("OSC2 Sub", int(osc2SubLevelstr));
}

void updateosc2MainLevel() {
  showCurrentParameterPage("OSC2 Main", int(osc2MainLevelstr));
}

void updateosc1MainWave() {
  getStratusOSCWaveform(osc1MainWavestr);
  showCurrentParameterPage("OSC1 Main", String(StratusOSCWaveform));
}

void updateosc2MainWave() {
  getStratusOSCWaveform(osc2MainWavestr);
  showCurrentParameterPage("OSC2 Main", String(StratusOSCWaveform));
}

void updateosc1SubWave() {
  getStratusSUBWaveform(osc1SubWavestr);
  showCurrentParameterPage("OSC1 Sub", String(StratusSUBWaveform));
}

void updateosc2SubWave() {
  getStratusSUBWaveform(osc2SubWavestr);
  showCurrentParameterPage("OSC2 Sub", String(StratusSUBWaveform));
}

void updatemainAttack() {
  if (pitchEG == 1) {
    pitchAttack = mainAttack;
  }
  if (filterEG == 1) {
    filterAttack = mainAttack;
  }
  if (ampEG == 1) {
    ampAttack = mainAttack;
  }
  if (mainAttackstr < 1000) {
    showCurrentParameterPage("Attack", String(int(mainAttackstr)) + " ms");
  } else {
    showCurrentParameterPage("Attack", String(mainAttackstr * 0.001) + " s");
  }
}

void updatemainDecay() {
  if (pitchEG == 1) {
    pitchDecay = mainDecay;
  }
  if (filterEG == 1) {
    filterDecay = mainDecay;
  }
  if (ampEG == 1) {
    ampDecay = mainDecay;
  }
  if (mainDecaystr < 1000) {
    showCurrentParameterPage("Decay", String(int(mainDecaystr)) + " ms");
  } else {
    showCurrentParameterPage("Decay", String(mainDecaystr * 0.001) + " s");
  }
}

void updatemainSustain() {
  if (pitchEG == 1) {
    pitchSustain = mainSustain;
  }
  if (filterEG == 1) {
    filterSustain = mainSustain;
  }
  if (ampEG == 1) {
    ampSustain = mainSustain;
  }
  showCurrentParameterPage("Sustain", String(mainSustainstr));
}

void updatemainRelease() {
  if (pitchEG == 1) {
    pitchRelease = mainRelease;
  }
  if (filterEG == 1) {
    filterRelease = mainRelease;
  }
  if (ampEG == 1) {
    ampRelease = mainRelease;
  }
  if (mainReleasestr < 1000) {
    showCurrentParameterPage("Release", String(int(mainReleasestr)) + " ms");
  } else {
    showCurrentParameterPage("Release", String(mainReleasestr * 0.001) + " s");
  }
}

void updateStratusLFOWaveform() {
  switch (lfoAlt) {
    case 0:
      switch (LFOWaveformstr) {
        case 0:
          StratusLFOWaveform = "Saw +Oct";
          LFOWaveform = 0;
          break;

        case 1:
          StratusLFOWaveform = "Quad Saw";
          LFOWaveform = 150;
          break;

        case 2:
          StratusLFOWaveform = "Quad Pulse";
          LFOWaveform = 300;
          break;

        case 3:
          StratusLFOWaveform = "Tri Step";
          LFOWaveform = 450;
          break;

        case 4:
          StratusLFOWaveform = "Sine +Oct";
          LFOWaveform = 600;
          break;

        case 5:
          StratusLFOWaveform = "Sine +3rd";
          LFOWaveform = 725;
          break;

        case 6:
          StratusLFOWaveform = "Sine +4th";
          LFOWaveform = 850;
          break;

        case 7:
          StratusLFOWaveform = "Rand Slopes";
          LFOWaveform = 1023;
          break;
      }
      break;

    case 1:
      switch (LFOWaveformstr) {
        case 0:
          StratusLFOWaveform = "Sawtooth Up";
          LFOWaveform = 0;
          break;

        case 1:
          StratusLFOWaveform = "Sawtooth Down";
          LFOWaveform = 150;
          break;

        case 2:
          StratusLFOWaveform = "Squarewave";
          LFOWaveform = 300;
          break;

        case 3:
          StratusLFOWaveform = "Triangle";
          LFOWaveform = 450;
          break;

        case 4:
          StratusLFOWaveform = "Sinewave";
          LFOWaveform = 575;
          break;

        case 5:
          StratusLFOWaveform = "Sweeps";
          LFOWaveform = 725;
          break;

        case 6:
          StratusLFOWaveform = "Lumps";
          LFOWaveform = 850;
          break;

        case 7:
          StratusLFOWaveform = "Sample & Hold";
          LFOWaveform = 1023;
          break;
      }
      break;
  }
  showCurrentParameterPage("LFO Wave", StratusLFOWaveform);
}

void updateEGlevel() {
  if (pitchEG == 1) {
    pitchEGlevel = EGlevel;
  }
  if (filterEG == 1) {
    filterEGlevel = EGlevel;
  }
  if (ampEG == 1) {
    ampEGlevel = EGlevel;
  }
  showCurrentParameterPage("EG Level", EGlevelstr);
}

void updateMasterTune() {
  showCurrentParameterPage("Master Tune", String(MasterTunestr));
}

void updatelfoAlt() {
  switch (lfoAlt) {
    case 0:
      srp.writePin(LFOALT_OUT, LOW);
      break;

    case 1:
      srp.writePin(LFOALT_OUT, HIGH);
      break;
  }
  updateStratusLFOWaveform();
}

void updateLfoMult() {
  switch (lfoMultstr) {

    case 0:
      showCurrentParameterPage("LFO Mult", String("X 1.5"));
      lfoMult = 304;
      break;

    case 1:
      showCurrentParameterPage("LFO Mult", String("X 1.0"));
      lfoMult = 200;
      break;

    case 2:
      showCurrentParameterPage("LFO Mult", String("X 0.5"));
      lfoMult = 2;
      break;
  }
}

void updateFilterType() {
  switch (filterPole) {
    case 0:
      switch (filterType) {
        case 0:
          showCurrentParameterPage("Filter Type", String("18dB LP"));
          srp.writePin(FILTER_A_OUT, LOW);
          srp.writePin(FILTER_B_OUT, LOW);
          srp.writePin(FILTER_C_OUT, LOW);
          break;

        case 1:
          showCurrentParameterPage("Filter Type", String("6dB LP"));
          srp.writePin(FILTER_A_OUT, HIGH);
          srp.writePin(FILTER_B_OUT, LOW);
          srp.writePin(FILTER_C_OUT, LOW);
          break;

        case 2:
          showCurrentParameterPage("Filter Type", String("12dB HP"));
          srp.writePin(FILTER_A_OUT, LOW);
          srp.writePin(FILTER_B_OUT, HIGH);
          srp.writePin(FILTER_C_OUT, LOW);
          break;

        case 3:
          showCurrentParameterPage("Filter Type", String("18dB HP"));
          srp.writePin(FILTER_A_OUT, HIGH);
          srp.writePin(FILTER_B_OUT, HIGH);
          srp.writePin(FILTER_C_OUT, LOW);

          break;

        case 4:
          showCurrentParameterPage("Filter Type", String("12dB HP/6 LP"));
          srp.writePin(FILTER_A_OUT, LOW);
          srp.writePin(FILTER_B_OUT, LOW);
          srp.writePin(FILTER_C_OUT, HIGH);
          break;

        case 5:
          showCurrentParameterPage("Filter Type", String("12dB N"));
          srp.writePin(FILTER_A_OUT, HIGH);
          srp.writePin(FILTER_B_OUT, LOW);
          srp.writePin(FILTER_C_OUT, HIGH);
          break;

        case 6:
          showCurrentParameterPage("Filter Type", String("6dB HP"));
          srp.writePin(FILTER_A_OUT, LOW);
          srp.writePin(FILTER_B_OUT, HIGH);
          srp.writePin(FILTER_C_OUT, HIGH);
          break;

        case 7:
          showCurrentParameterPage("Filter Type", String("18dB AP"));
          srp.writePin(FILTER_A_OUT, HIGH);
          srp.writePin(FILTER_B_OUT, HIGH);
          srp.writePin(FILTER_C_OUT, HIGH);
          break;
      }
      break;

    case 1:
      switch (filterType) {
        case 0:
          showCurrentParameterPage("Filter Type", String("24dB LP"));
          srp.writePin(FILTER_A_OUT, LOW);
          srp.writePin(FILTER_B_OUT, LOW);
          srp.writePin(FILTER_C_OUT, LOW);
          break;

        case 1:
          showCurrentParameterPage("Filter Type", String("12dB LP"));
          srp.writePin(FILTER_A_OUT, HIGH);
          srp.writePin(FILTER_B_OUT, LOW);
          srp.writePin(FILTER_C_OUT, LOW);
          break;

        case 2:
          showCurrentParameterPage("Filter Type", String("12dB HP/6 LP"));
          srp.writePin(FILTER_A_OUT, LOW);
          srp.writePin(FILTER_B_OUT, HIGH);
          srp.writePin(FILTER_C_OUT, LOW);
          break;

        case 3:
          showCurrentParameterPage("Filter Type", String("18dB HP/6 LP"));
          srp.writePin(FILTER_A_OUT, HIGH);
          srp.writePin(FILTER_B_OUT, HIGH);
          srp.writePin(FILTER_C_OUT, LOW);
          break;

        case 4:
          showCurrentParameterPage("Filter Type", String("24dB BP"));
          srp.writePin(FILTER_A_OUT, LOW);
          srp.writePin(FILTER_B_OUT, LOW);
          srp.writePin(FILTER_C_OUT, HIGH);
          break;

        case 5:
          showCurrentParameterPage("Filter Type", String("12dB N/6 LP"));
          srp.writePin(FILTER_A_OUT, HIGH);
          srp.writePin(FILTER_B_OUT, LOW);
          srp.writePin(FILTER_C_OUT, HIGH);
          break;

        case 6:
          showCurrentParameterPage("Filter Type", String("12dB BP"));
          srp.writePin(FILTER_A_OUT, LOW);
          srp.writePin(FILTER_B_OUT, HIGH);
          srp.writePin(FILTER_C_OUT, HIGH);
          break;

        case 7:
          showCurrentParameterPage("Filter Type", String("18dB AP/6 LP"));
          srp.writePin(FILTER_A_OUT, HIGH);
          srp.writePin(FILTER_B_OUT, HIGH);
          srp.writePin(FILTER_C_OUT, HIGH);
          break;
      }
      break;
  }
}


void updatePatchname() {
  showPatchPage(String(patchNo), patchName);
}

void myControlChange(byte channel, byte control, int value) {

  //  Serial.println("MIDI: " + String(control) + " : " + String(value));
  switch (control) {
    case CCvolumeControl:
      volumeControl = value;
      volumeControlstr = value / 8;
      updatevolumeControl();
      break;

    case CCoctave1:
      value > 0 ? octave1 = 1 : octave1 = 0;
      updateoctave1();
      break;

    case CCoctave2:
      value > 0 ? octave2 = 1 : octave2 = 0;
      updateoctave2();
      break;

    case CCunisonSW:
      value > 0 ? unisonSW = 1 : unisonSW = 0;
      updateUnisonSwitch();
      break;

    case CCglide:
      value > 0 ? glide = 1 : glide = 0;
      updateglide();
      break;

    case CCfilterKeyTrack:
      value > 0 ? filterKeyTrack = 1 : filterKeyTrack = 0;
      updatefilterKeyTrack();
      break;

    case CCfilterPole:
      value > 0 ? filterPole = 1 : filterPole = 0;
      updatefilterPole();
      break;

    case CCfilterEGinv:
      value > 0 ? filterEGinv = 1 : filterEGinv = 0;
      updatefilterEGinv();
      break;

    case CCfilterLoop:
      value > 0 ? filterLoop = 1 : filterLoop = 0;
      updatefilterLoop();
      break;

    case CCpitchEG:
      value > 0 ? pitchEG = 1 : pitchEG = 0;
      updatepitchEG();
      break;

    case CCfilterEG:
      value > 0 ? filterEG = 1 : filterEG = 0;
      updatefilterEG();
      break;

    case CCampEG:
      value > 0 ? ampEG = 1 : ampEG = 0;
      updateampEG();
      break;

    case CCvelocitySW:
      value > 0 ? velocitySW = 1 : velocitySW = 0;
      updatevelocitySW();
      break;

    case CClfoDestVCO:
      value > 0 ? lfoDestVCO = 1 : lfoDestVCO = 0;
      updatelfoDestVCO();
      break;

    case CClfoDestVCF:
      value > 0 ? lfoDestVCF = 1 : lfoDestVCF = 0;
      updatelfoDestVCF();
      break;

    case CClfoDestVCA:
      value > 0 ? lfoDestVCA = 1 : lfoDestVCA = 0;
      updatelfoDestVCA();
      break;

    case CCmonoMulti:
      value > 0 ? monoMulti = 1 : monoMulti = 0;
      updateMonoMulti();
      break;

    case CCeffects:
      value > 0 ? effects = 1 : effects = 0;
      updateEffects();
      break;

    case CCosc1EGinv:
      value > 0 ? osc1EGinv = 1 : osc1EGinv = 0;
      updateosc1EGinv();
      break;

    case CCosc2EGon:
      value > 0 ? osc2EGon = 1 : osc2EGon = 0;
      updateosc2EGon();
      break;

    case CCoctaveup:
      value > 0 ? octaveup = 1 : octaveup = 0;
      updateoctaveup();
      break;

    case CCoctavedown:
      value > 0 ? octavedown = 1 : octavedown = 0;
      updateoctavedown();
      break;

    case CCpot1:
      pot1 = value;
      pot1str = map(value, 0, 1023, 0, 127);
      updatepot1();
      break;

    case CCpot2:
      pot2 = value;
      pot2str = map(value, 0, 1023, 0, 127);
      updatepot2();
      break;

    case CCpot3:
      pot3 = value;
      pot3str = map(value, 0, 1023, 0, 127);
      updatepot3();
      break;

    case CCeffectNumber:
      effectNumber = map(value, 0, 1023, 0, 7);
      effectNumberstr = map(value, 0, 1023, 0, 7);
      updateeffectNumber();
      break;

    case CCmix:
      mix = value;
      mixa = mix;
      mixb = map(value, 0, 1023, 1023, 0);
      mixstr = map(value, 0, 1023, 0, 127);
      updatemix();
      break;

    case CCinternal:
      internal = map(value, 0, 1023, 0, 1);
      updateinternal();
      break;

    case CCGlideTime:
      GlideTime = value;
      GlideTimestr = map(value, 0, 1023, 0, 127);
      updateGlideTime();
      break;

    case CCosc1Detune:
      osc1Detunestr = PITCH_DETUNE[value / 8];
      osc1Detune = value;
      updateosc1Detune();
      break;

    case CCLfoDelay:
      LfoDelaystr = LINEAR[value / 8];
      LfoDelay = value;
      updateLfoDelay();
      break;

    case CCEGlevel:
      EGlevel = value;
      EGlevelstr = map(value, 0, 1023, 0, 127);
      updateEGlevel();
      break;

    case CCnoiseLevel:
      noiseLevel = value;
      noiseLevelstr = LINEARCENTREZERO[value / 8];
      updateNoiseLevel();
      break;

    case CCLfoSlope:
      LfoSlope = value;
      LfoSlopestr = map(value, 0, 1023, 0, 127);
      updateLfoSlope();
      break;

    case CCfilterCutoff:
      filterCutoff = value;
      filterCutoffstr = FILTERCUTOFF[value / 8];
      updateFilterCutoff();
      break;

    case CCfilterRes:
      filterRes = value;
      filterResstr = map(value, 0, 1023, 0, 127);
      updatefilterRes();
      break;

    case CCosc1MainWave:
      osc1MainWave = value + 17;
      osc1MainWavestr = map(value, 0, 1023, 0, 15);
      updateosc1MainWave();
      break;

    case CCosc2MainWave:
      osc2MainWave = value + 17;
      osc2MainWavestr = map(value, 0, 1023, 0, 15);
      updateosc2MainWave();
      break;

    case CCosc1SubWave:
      osc1SubWave = value + 17;
      osc1SubWavestr = map(value, 0, 1023, 0, 31);
      updateosc1SubWave();
      break;

    case CCosc2SubWave:
      osc2SubWave = value + 17;
      osc2SubWavestr = map(value, 0, 1023, 0, 31);
      updateosc2SubWave();
      break;

    case CCLfoDepth:
      LfoDepth = value;
      LfoDepthstr = map(value, 0, 1023, 0, 127);
      updateLfoDepth();
      break;

    case CCLfoRate:
      LfoRatestr = LFOTEMPOINV[value / 8];  // for display
      LfoRate = value;
      updateLfoRate();
      break;

    case CCbitCrush:
      bitCrush = value;
      bitCrushstr = map(value, 0, 1023, 0, 7);  // for display
      updatebitCrush();
      break;

    case CCPitchBend:
      PitchBendLevelstr = map(value, 0, 1023, 0, 7);
      PitchBendLevel = value;

      switch(PitchBendLevelstr) {

        case 0:
        PitchBendLevel = 0;
        break;

        case 1:
        PitchBendLevel = 242;
        break;

        case 2:
        PitchBendLevel = 307;
        break;

        case 3:
        PitchBendLevel = 361;
        break;

        case 4:
        PitchBendLevel = 432;
        break;

        case 5:
        PitchBendLevel = 489;
        break;

        case 6:
        PitchBendLevel = 560;
        break;

        case 7:
        PitchBendLevel = 625;
        break;

      }
      updatePitchBend();
      break;

    case CCosc1SubLevel:
      osc1SubLevel = value;
      osc1SubLevelstr = map(value, 0, 1023, 0, 127);  // for display
      updateosc1SubLevel();
      break;

    case CCosc1MainLevel:
      osc1MainLevel = value;
      osc1MainLevelstr = map(value, 0, 1023, 0, 127);  // for display
      updateosc1MainLevel();
      break;

    case CCosc2SubLevel:
      osc2SubLevel = value;
      osc2SubLevelstr = map(value, 0, 1023, 0, 127);  // for display
      updateosc2SubLevel();
      break;

    case CCosc2MainLevel:
      osc2MainLevel = value;
      osc2MainLevelstr = map(value, 0, 1023, 0, 127);  // for display
      updateosc2MainLevel();
      break;

    case CCmainAttack:
      mainAttack = value;
      mainAttackstr = ENVTIMES[value / 8];
      updatemainAttack();
      break;

    case CCmainDecay:
      mainDecay = value;
      mainDecaystr = ENVTIMES[value / 8];
      updatemainDecay();
      break;

    case CCmainSustain:
      mainSustain = value;
      mainSustainstr = LINEAR_FILTERMIXERSTR[value / 8];
      updatemainSustain();
      break;

    case CCmainRelease:
      mainRelease = value;
      mainReleasestr = ENVTIMES[value / 8];
      updatemainRelease();
      break;

    case CCLFOWaveform:
      //LFOWaveform = value;
      LFOWaveformstr = map(value, 0, 1023, 0, 7);
      updateStratusLFOWaveform();
      break;

    case CCMasterTune:
      MasterTune = value;
      MasterTunestr = PITCH[value / 8];
      updateMasterTune();
      break;

    case CClfoAlt:
      lfoAlt = map(value, 0, 1023, 0, 1);
      updatelfoAlt();
      break;

    case CClfoMult:
      lfoMult = value;
      lfoMultstr = map(value, 0, 1023, 0, 5);
      updateLfoMult();
      break;

    case CCfilterType:
      filterType = map(value, 0, 1023, 0, 7);
      updateFilterType();
      break;

    case CCmodwheel:
      modWheelLevel = value;
      modWheelLevelstr = map(value, 0, 1023, 0, 127);
      updatemodWheel();
      break;

    case CCallnotesoff:
      allNotesOff();
      break;
  }
}

void myProgramChange(byte channel, byte program) {
  state = PATCH;
  patchNo = program + 1;
  recallPatch(patchNo);
  Serial.print("MIDI Pgm Change:");
  Serial.println(patchNo);
  state = PARAMETER;
}

void recallPatch(int patchNo) {
  allNotesOff();
  srp.writePin(SIGNAL_MUTE, LOW);

  File patchFile = SD.open(String(patchNo).c_str());
  if (!patchFile) {
    Serial.println("File not found");
  } else {
    String data[NO_OF_PARAMS];  //Array of data read in
    recallPatchData(patchFile, data);
    setCurrentPatchData(data);
    patchFile.close();
    storeLastPatch(patchNo);
  }
  //srp.writePin(SIGNAL_MUTE, HIGH);
  unmuteDelay = 0;
}

void setCurrentPatchData(String data[]) {
  patchName = data[0];
  noiseLevel = data[1].toFloat();
  octave1 = data[2].toInt();
  unisonSW = data[3].toInt();
  GlideTime = data[4].toFloat();
  volumeControl = data[5].toFloat();
  ampVelo = data[6].toFloat();
  LfoDelay = data[7].toFloat();
  pitchEGlevel = data[8].toFloat();
  filterRes = data[9].toFloat();
  filterCutoff = data[10].toFloat();
  osc2SubWave = data[11].toFloat();
  osc1MainWave = data[12].toFloat();
  osc2MainWave = data[13].toFloat();
  LfoDepth = data[14].toFloat();
  osc2SubLevel = data[15].toFloat();
  osc2MainLevel = data[16].toFloat();
  octave2 = data[17].toInt();
  lfoAlt = data[18].toInt();
  glide = data[19].toInt();
  LfoRate = data[20].toFloat();
  bitCrush = data[21].toFloat();
  osc1SubLevel = data[22].toFloat();
  osc1MainLevel = data[23].toFloat();
  osc1SubWave = data[24].toFloat();
  ampAttack = data[25].toFloat();
  ampDecay = data[26].toFloat();
  ampSustain = data[27].toFloat();
  ampRelease = data[28].toFloat();
  LFOWaveform = data[29].toFloat();
  MasterTune = data[30].toFloat();
  lfoDestVCO = data[31].toInt();
  monoMulti = data[32].toInt();
  lfoMult = data[33].toInt();
  filterType = data[34].toInt();
  filterKeyTrack = data[35].toInt();
  osc1Detune = data[36].toFloat();
  LfoSlope = data[37].toFloat();
  PitchBendLevel = data[38].toFloat();
  lfoDestVCF = data[39].toInt();
  lfoDestVCA = data[40].toInt();
  filterA = data[41].toFloat();
  filterB = data[42].toFloat();
  filterC = data[43].toFloat();
  octave_select1 = data[44].toFloat();
  octave_select2 = data[45].toFloat();
  modWheelLevel = data[46].toFloat();
  filterEGinv = data[47].toInt();
  filterLoop = data[48].toInt();
  filterPole = data[49].toInt();
  osc1EGinv = data[50].toInt();
  osc2EGon = data[51].toInt();
  pitchAttack = data[52].toFloat();
  pitchDecay = data[53].toFloat();
  pitchSustain = data[54].toFloat();
  pitchRelease = data[55].toFloat();
  filterAttack = data[56].toFloat();
  filterDecay = data[57].toFloat();
  filterSustain = data[58].toFloat();
  filterRelease = data[59].toFloat();
  filterEGlevel = data[60].toFloat();
  ampEGlevel = data[61].toFloat();
  pitchEG = data[62].toInt();
  filterEG = data[63].toInt();
  ampEG = data[64].toInt();
  pitchVelo = data[65].toInt();
  filterVelo = data[66].toInt();
  FilterEnvPatch = data[67].toInt();
  effectNumber = data[68].toInt();
  pot1 = data[69].toInt();
  pot2 = data[70].toInt();
  pot3 = data[71].toInt();
  mixa = data[72].toInt();
  mixb = data[73].toInt();
  internal = data[74].toInt();

  //Switches

  updateoctave1();
  updateoctave2();
  updateglide();
  updateUnisonSwitch();
  updatefilterKeyTrack();
  updatefilterPole();
  updatefilterEGinv();
  updatefilterLoop();
  updateFilterType();
  updatepitchEG();
  updatefilterEG();
  updateampEG();
  updateRecallvelocitySW();
  updatelfoDestVCO();
  updatelfoDestVCF();
  updatelfoDestVCA();
  updateMonoMulti();
  updateosc1EGinv();
  updateosc2EGon();
  updatelfoAlt();
  updateinternal();
  updateeffectNumber();

  //  getFilterEnv();

  //Patchname
  updatePatchname();

  Serial.print("Set Patch: ");
  Serial.println(patchName);
}

String getCurrentPatchData() {
  return patchName + "," + String(noiseLevel) + "," + String(octave1) + "," + String(unisonSW) + "," + String(GlideTime) + "," + String(volumeControl) + "," + String(ampVelo)
         + "," + String(LfoDelay) + "," + String(pitchEGlevel) + "," + String(filterRes) + "," + String(filterCutoff) + "," + String(osc2SubWave) + "," + String(osc1MainWave)
         + "," + String(osc2MainWave) + "," + String(LfoDepth) + "," + String(osc2SubLevel) + "," + String(osc2MainLevel) + "," + String(octave2) + "," + String(lfoAlt)
         + "," + String(glide) + "," + String(LfoRate) + "," + String(bitCrush) + "," + String(osc1SubLevel) + "," + String(osc1MainLevel) + "," + String(osc1SubWave)
         + "," + String(ampAttack) + "," + String(ampDecay) + "," + String(ampSustain) + "," + String(ampRelease) + "," + String(LFOWaveform) + "," + String(MasterTune)
         + "," + String(lfoDestVCO) + "," + String(monoMulti) + "," + String(lfoMult) + "," + String(filterType) + "," + String(filterKeyTrack) + "," + String(osc1Detune)
         + "," + String(LfoSlope) + "," + String(PitchBendLevel) + "," + String(lfoDestVCF) + "," + String(lfoDestVCA) + "," + String(filterA) + "," + String(filterB)
         + "," + String(filterC) + "," + String(octave_select1) + "," + String(octave_select2) + "," + String(modWheelLevel) + "," + String(filterEGinv) + "," + String(filterLoop)
         + "," + String(filterPole) + "," + String(osc1EGinv) + "," + String(osc2EGon) + "," + String(pitchAttack) + "," + String(pitchDecay) + "," + String(pitchSustain) + "," + String(pitchRelease)
         + "," + String(filterAttack) + "," + String(filterDecay) + "," + String(filterSustain) + "," + String(filterRelease) + "," + String(filterEGlevel) + "," + String(ampEGlevel) + "," + String(pitchEG)
         + "," + String(filterEG) + "," + String(ampEG) + "," + String(pitchVelo) + "," + String(filterVelo) + "," + String(FilterEnvPatch)
         + "," + String(effectNumber) + "," + String(pot1) + "," + String(pot2) + "," + String(pot3) + "," + String(mixa) + "," + String(mixb) + "," + String(internal);
}

void checkMux() {

  mux1Read = adc->adc0->analogRead(MUX1_S);
  mux2Read = adc->adc0->analogRead(MUX2_S);

  if (mux1Read > (mux1ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux1Read < (mux1ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux1ValuesPrev[muxInput] = mux1Read;

    switch (muxInput) {
      case MUX1_osc1MainWave:
        myControlChange(midiChannel, CCosc1MainWave, mux1Read);
        break;
      case MUX1_filterType:
        myControlChange(midiChannel, CCfilterType, mux1Read);
        break;
      case MUX1_filterRes:
        myControlChange(midiChannel, CCfilterRes, mux1Read);
        break;
      case MUX1_filterCutoff:
        myControlChange(midiChannel, CCfilterCutoff, mux1Read);
        break;
      case MUX1_osc1MainLevel:
        myControlChange(midiChannel, CCosc1MainLevel, mux1Read);
        break;
      case MUX1_osc1SubLevel:
        myControlChange(midiChannel, CCosc1SubLevel, mux1Read);
        break;
      case MUX1_osc2MainLevel:
        myControlChange(midiChannel, CCosc2MainLevel, mux1Read);
        break;
      case MUX1_osc2SubLevel:
        myControlChange(midiChannel, CCosc2SubLevel, mux1Read);
        break;
      case MUX1_volumeControl:
        myControlChange(midiChannel, CCvolumeControl, mux1Read);
        break;
      case MUX1_bitCrush:
        myControlChange(midiChannel, CCbitCrush, mux1Read);
        break;
      case MUX1_mainAttack:
        myControlChange(midiChannel, CCmainAttack, mux1Read);
        break;
      case MUX1_mainDecay:
        myControlChange(midiChannel, CCmainDecay, mux1Read);
        break;
      case MUX1_mainSustain:
        myControlChange(midiChannel, CCmainSustain, mux1Read);
        break;
      case MUX1_mainRelease:
        myControlChange(midiChannel, CCmainRelease, mux1Read);
        break;
      case MUX1_PitchBend:
        myControlChange(midiChannel, CCPitchBend, mux1Read);
        break;
      case MUX1_modwheel:
        myControlChange(midiChannel, CCmodwheel, mux1Read);
        break;
    }
  }

  if (mux2Read > (mux2ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux2Read < (mux2ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux2ValuesPrev[muxInput] = mux2Read;

    switch (muxInput) {
      //      case MUX2_0:
      //        myControlChange(midiChannel, CCspare, mux2Read);
      //        break;
      case MUX2_LFOWaveform:
        if (effects) {
          myControlChange(midiChannel, CCeffectNumber, mux2Read);
        } else {
          myControlChange(midiChannel, CCLFOWaveform, mux2Read);
        }
        break;
      case MUX2_lfoAlt:
        if (effects) {
          myControlChange(midiChannel, CCinternal, mux2Read);
        } else {
          myControlChange(midiChannel, CClfoAlt, mux2Read);
        }
        break;
      case MUX2_lfoMult:
        myControlChange(midiChannel, CClfoMult, mux2Read);
        break;
      case MUX2_osc1Detune:
        myControlChange(midiChannel, CCosc1Detune, mux2Read);
        break;
      case MUX2_osc2SubWave:
        myControlChange(midiChannel, CCosc2SubWave, mux2Read);
        break;
      case MUX2_osc2MainWave:
        myControlChange(midiChannel, CCosc2MainWave, mux2Read);
        break;
      case MUX2_osc1SubWave:
        myControlChange(midiChannel, CCosc1SubWave, mux2Read);
        break;
      case MUX2_EGlevel:
        myControlChange(midiChannel, CCEGlevel, mux2Read);
        break;
      case MUX2_noiseLevel:
        myControlChange(midiChannel, CCnoiseLevel, mux2Read);
        break;
      case MUX2_GlideTime:
        myControlChange(midiChannel, CCGlideTime, mux2Read);
        break;
      case MUX2_MasterTune:
        myControlChange(midiChannel, CCMasterTune, mux2Read);
        break;
      case MUX2_LfoRate:
        if (effects) {
          myControlChange(midiChannel, CCpot1, mux2Read);
        } else {
          myControlChange(midiChannel, CCLfoRate, mux2Read);
        }
        break;
      case MUX2_LfoSlope:
        if (effects) {
          myControlChange(midiChannel, CCpot2, mux2Read);
        } else {
          myControlChange(midiChannel, CCLfoSlope, mux2Read);
        }
        break;
      case MUX2_LfoDelay:
        if (effects) {
          myControlChange(midiChannel, CCpot3, mux2Read);
        } else {
          myControlChange(midiChannel, CCLfoDelay, mux2Read);
        }
        break;
      case MUX2_LfoDepth:
        if (effects) {
          myControlChange(midiChannel, CCmix, mux2Read);
        } else {
          myControlChange(midiChannel, CCLfoDepth, mux2Read);
        }
        break;
    }
  }

  muxInput++;
  if (muxInput >= MUXCHANNELS)
    muxInput = 0;

  digitalWriteFast(MUX_0, muxInput & B0001);
  digitalWriteFast(MUX_1, muxInput & B0010);
  digitalWriteFast(MUX_2, muxInput & B0100);
  digitalWriteFast(MUX_3, muxInput & B1000);
}

void setVoltage(int dacpin, bool channel, bool gain, unsigned int mV) {
  int command = channel ? 0x9000 : 0x1000;

  command |= gain ? 0x0000 : 0x2000;
  command |= (mV & 0x0FFF);

  SPI.beginTransaction(SPISettings(15000000, MSBFIRST, SPI_MODE0));
  digitalWriteFast(dacpin, LOW);
  SPI.transfer(command >> 8);
  SPI.transfer(command & 0xFF);
  digitalWriteFast(dacpin, HIGH);
  SPI.endTransaction();
}

void writeDACs() {
  switch (chipSelSwitch) {
    case 0:

      //DEMUX 1
      switch (muxOutput) {
        case 0:  // 2Volt
          setVoltage(CHIP_SEL1, 0, 1, int(noiseLevel * 2));
          setVoltage(CHIP_SEL1, 1, 1, int(osc2SubLevel * 1));
          break;

        case 1:  // 2Volt
          setVoltage(CHIP_SEL1, 0, 1, int(osc2MainLevel * 1));
          setVoltage(CHIP_SEL1, 1, 1, int(LfoDepth * 2));
          break;

        case 2:  // 2Volt
          setVoltage(CHIP_SEL1, 0, 1, 0);
          setVoltage(CHIP_SEL1, 1, 1, int(PitchBendLevel * 2));
          break;

        case 3:  // 2Volt
          setVoltage(CHIP_SEL1, 0, 1, int(osc1SubLevel * 1));
          setVoltage(CHIP_SEL1, 1, 1, int(osc1MainLevel * 1));
          break;

        case 4:  // 2Volt
          setVoltage(CHIP_SEL1, 0, 1, int(modWheelLevel * 2));
          setVoltage(CHIP_SEL1, 1, 1, int(volumeControl * 1.25));
          break;

        case 5:  // 2Volt
          setVoltage(CHIP_SEL1, 0, 1, int(mixa * 2));
          setVoltage(CHIP_SEL1, 1, 1, int(mixb * 2));
          break;

        case 6:
          setVoltage(CHIP_SEL1, 0, 1, int(ampEGlevel * 2.43));
          setVoltage(CHIP_SEL1, 1, 1, int(MasterTune * 2.43));
          break;

        case 7:
          setVoltage(CHIP_SEL1, 0, 1, int(osc1Detune * 2.43));
          setVoltage(CHIP_SEL1, 1, 1, int(pitchEGlevel * 2.43));
          break;

        case 8:  // 5Volt
          setVoltage(CHIP_SEL1, 0, 1, int(filterRelease * 2.43));
          setVoltage(CHIP_SEL1, 1, 1, int(filterSustain * 2.43));
          break;

        case 9:  // 5Volt
          setVoltage(CHIP_SEL1, 0, 1, int(filterDecay * 2.43));
          setVoltage(CHIP_SEL1, 1, 1, int(filterAttack * 2.43));
          break;

        case 10:  // 5Volt
          setVoltage(CHIP_SEL1, 0, 1, int(LfoRate * 2.43));
          setVoltage(CHIP_SEL1, 1, 1, int(lfoMult * 2.43));
          break;

        case 11:  // 5Volt
          setVoltage(CHIP_SEL1, 0, 1, int(LFOWaveform * 2.41));
          setVoltage(CHIP_SEL1, 1, 1, int(LfoDelay * 2.43));
          break;

        case 12:  // 5Volt
          setVoltage(CHIP_SEL1, 0, 1, int(osc2MainWave * 2.43));
          setVoltage(CHIP_SEL1, 1, 1, int(osc1MainWave * 2.43));
          break;

        case 13:  // 5Volt
          setVoltage(CHIP_SEL1, 0, 1, int(filterEGlevel * 2.43));
          setVoltage(CHIP_SEL1, 1, 1, int(GlideTime * 2.43));
          break;

        case 14:  // 5Volt
          setVoltage(CHIP_SEL1, 0, 1, 0);
          setVoltage(CHIP_SEL1, 1, 1, int(LfoSlope * 2.43));
          break;

        case 15:  // 10Volt
          setVoltage(CHIP_SEL1, 0, 1, int(filterCutoff * 1.5));
          setVoltage(CHIP_SEL1, 1, 1, int(filterRes * 1.5));
          break;
      }
      muxOutput++;
      if (muxOutput >= DEMUXCHANNELS)
        muxOutput = 0;

      digitalWriteFast(DEMUX_S0, muxOutput & B0001);
      digitalWriteFast(DEMUX_S1, muxOutput & B0010);
      digitalWriteFast(DEMUX_S2, muxOutput & B0100);
      digitalWriteFast(DEMUX_S3, muxOutput & B1000);
      break;

    case 1:

      //DEMUX 2
      switch (muxOutput) {
        case 0:  // 5volt
          setVoltage(CHIP_SEL2, 0, 1, int(osc1SubWave * 2.43));
          setVoltage(CHIP_SEL2, 1, 1, int(osc2SubWave * 2.43));
          break;

        case 1:  // 5Volt
          setVoltage(CHIP_SEL2, 0, 1, int(bitCrush * 2.43));
          setVoltage(CHIP_SEL2, 1, 1, int(0));
          break;

        case 2:  // 5Volt
          setVoltage(CHIP_SEL2, 0, 1, int(pot1 * 1.6));
          setVoltage(CHIP_SEL2, 1, 1, int(pot2 * 1.6));
          break;

        case 3:  // 5Volt
          setVoltage(CHIP_SEL2, 0, 1, int(pot3 * 1.6));
          setVoltage(CHIP_SEL2, 1, 1, int(0));
          break;

        case 4:  // 5Volt
          setVoltage(CHIP_SEL2, 0, 1, int(pitchAttack * 2.43));
          setVoltage(CHIP_SEL2, 1, 1, int(pitchDecay * 2.43));
          break;

        case 5:  // 5Volt
          setVoltage(CHIP_SEL2, 0, 1, int(pitchSustain * 2.43));
          setVoltage(CHIP_SEL2, 1, 1, int(pitchRelease * 2.43));
          break;

        case 6:  // 5Volt
          setVoltage(CHIP_SEL2, 0, 1, int(ampAttack * 2.43));
          setVoltage(CHIP_SEL2, 1, 1, int(ampDecay * 2.43));
          break;

        case 7:  // 5Volt
          setVoltage(CHIP_SEL2, 0, 1, int(ampSustain * 2.43));
          setVoltage(CHIP_SEL2, 1, 1, int(ampRelease * 2.43));
          break;
      }

      muxOutput2++;
      if (muxOutput2 >= DEMUXCHANNELS2)
        muxOutput2 = 0;

      digitalWriteFast(DEMUX_S0, muxOutput & B0001);
      digitalWriteFast(DEMUX_S1, muxOutput & B0010);
      digitalWriteFast(DEMUX_S2, muxOutput & B0100);
      digitalWriteFast(DEMUX_S3, muxOutput & B1000);
      break;
  }

  chipSelSwitch++;
  if (chipSelSwitch >= MUXBANK)
    chipSelSwitch = 0;
}

void onButtonPress(uint16_t btnIndex, uint8_t btnType) {

  if (btnIndex == OCTAVE1_SW && btnType == ROX_PRESSED) {
    octave1 = !octave1;
    myControlChange(midiChannel, CCoctave1, octave1);
  }

  if (btnIndex == OCTAVE2_SW && btnType == ROX_PRESSED) {
    octave2 = !octave2;
    myControlChange(midiChannel, CCoctave2, octave2);
  }

  if (btnIndex == UNISON_SW && btnType == ROX_PRESSED) {
    unisonSW = !unisonSW;
    myControlChange(midiChannel, CCunisonSW, unisonSW);
  }

  if (btnIndex == GLIDE_SW && btnType == ROX_PRESSED) {
    glide = !glide;
    myControlChange(midiChannel, CCglide, glide);
  }

  if (btnIndex == KEYTRK_SW && btnType == ROX_PRESSED) {
    filterKeyTrack = !filterKeyTrack;
    myControlChange(midiChannel, CCfilterKeyTrack, filterKeyTrack);
  }

  if (btnIndex == POLE_SW && btnType == ROX_PRESSED) {
    filterPole = !filterPole;
    myControlChange(midiChannel, CCfilterPole, filterPole);
  }

  if (btnIndex == EG_INV_SW && btnType == ROX_PRESSED) {
    filterEGinv = !filterEGinv;
    myControlChange(midiChannel, CCfilterEGinv, filterEGinv);
  }

  if (btnIndex == LOOP_SW && btnType == ROX_PRESSED) {
    filterLoop = !filterLoop;
    myControlChange(midiChannel, CCfilterLoop, filterLoop);
  }

  if (btnIndex == PITCH_EG_SW && btnType == ROX_PRESSED) {
    pitchEG = !pitchEG;
    myControlChange(midiChannel, CCpitchEG, pitchEG);
  }

  if (btnIndex == VCF_EG_SW && btnType == ROX_PRESSED) {
    filterEG = !filterEG;
    myControlChange(midiChannel, CCfilterEG, filterEG);
  }

  if (btnIndex == VCA_EG_SW && btnType == ROX_PRESSED) {
    ampEG = !ampEG;
    myControlChange(midiChannel, CCampEG, ampEG);
  }

  if (btnIndex == VELO_SW && btnType == ROX_PRESSED) {
    velocitySW = !velocitySW;
    myControlChange(midiChannel, CCvelocitySW, velocitySW);
  }

  if (btnIndex == VCO_SW && btnType == ROX_PRESSED) {
    lfoDestVCO = !lfoDestVCO;
    myControlChange(midiChannel, CClfoDestVCO, lfoDestVCO);
  }

  if (btnIndex == VCF_SW && btnType == ROX_PRESSED) {
    lfoDestVCF = !lfoDestVCF;
    myControlChange(midiChannel, CClfoDestVCF, lfoDestVCF);
  }

  if (btnIndex == VCA_SW && btnType == ROX_PRESSED) {
    lfoDestVCA = !lfoDestVCA;
    myControlChange(midiChannel, CClfoDestVCA, lfoDestVCA);
  }

  if (btnIndex == MULTI_SW && btnType == ROX_RELEASED) {
    monoMulti = !monoMulti;
    myControlChange(midiChannel, CCmonoMulti, monoMulti);
  } else if (btnIndex == MULTI_SW && btnType == ROX_HELD) {
    effects = !effects;
    if (effects) {
      srpanel.set(EFFECT_LED, HIGH);
      srpanel.set(LFO_LED, LOW);
    } else {
      srpanel.set(EFFECT_LED, LOW);
      srpanel.set(LFO_LED, HIGH);
    }
    myControlChange(midiChannel, CCeffects, effects);
  }

  if (btnIndex == OSC1_EG_SW && btnType == ROX_PRESSED) {
    osc1EGinv = !osc1EGinv;
    myControlChange(midiChannel, CCosc1EGinv, osc1EGinv);
  }

  if (btnIndex == OSC2_EG_SW && btnType == ROX_PRESSED) {
    osc2EGon = !osc2EGon;
    myControlChange(midiChannel, CCosc2EGon, osc2EGon);
  }
}

void checkSwitches() {

  saveButton.update();
  if (saveButton.held()) {
    switch (state) {
      case PARAMETER:
      case PATCH:
        state = DELETE;
        break;
    }
  } else if (saveButton.numClicks() == 1) {
    switch (state) {
      case PARAMETER:
        if (patches.size() < PATCHES_LIMIT) {
          resetPatchesOrdering();  //Reset order of patches from first patch
          patches.push({ patches.size() + 1, INITPATCHNAME });
          state = SAVE;
        }
        break;
      case SAVE:
        //Save as new patch with INITIALPATCH name or overwrite existing keeping name - bypassing patch renaming
        patchName = patches.last().patchName;
        state = PATCH;
        setVolumeZero();
        savePatch(String(patches.last().patchNo).c_str(), getCurrentPatchData());
        showPatchPage(patches.last().patchNo, patches.last().patchName);
        patchNo = patches.last().patchNo;
        loadPatches();  //Get rid of pushed patch if it wasn't saved
        setPatchesOrdering(patchNo);
        renamedPatch = "";
        state = PARAMETER;
        break;
      case PATCHNAMING:
        if (renamedPatch.length() > 0) patchName = renamedPatch;  //Prevent empty strings
        state = PATCH;
        setVolumeZero();
        savePatch(String(patches.last().patchNo).c_str(), getCurrentPatchData());
        showPatchPage(patches.last().patchNo, patchName);
        patchNo = patches.last().patchNo;
        loadPatches();  //Get rid of pushed patch if it wasn't saved
        setPatchesOrdering(patchNo);
        renamedPatch = "";
        state = PARAMETER;
        break;
    }
  }

  settingsButton.update();
  if (settingsButton.held()) {
    //If recall held, set current patch to match current hardware state
    //Reinitialise all hardware values to force them to be re-read if different
    state = REINITIALISE;
    reinitialiseToPanel();
  } else if (settingsButton.numClicks() == 1) {
    switch (state) {
      case PARAMETER:
        state = SETTINGS;
        showSettingsPage();
        break;
      case SETTINGS:
        showSettingsPage();
      case SETTINGSVALUE:
        settings::save_current_value();
        state = SETTINGS;
        showSettingsPage();
        break;
    }
  }

  backButton.update();
  if (backButton.held()) {
    //If Back button held, Panic - all notes off
  } else if (backButton.numClicks() == 1) {
    switch (state) {
      case RECALL:
        setPatchesOrdering(patchNo);
        state = PARAMETER;
        break;
      case SAVE:
        renamedPatch = "";
        state = PARAMETER;
        loadPatches();  //Remove patch that was to be saved
        setPatchesOrdering(patchNo);
        break;
      case PATCHNAMING:
        charIndex = 0;
        renamedPatch = "";
        state = SAVE;
        break;
      case DELETE:
        setPatchesOrdering(patchNo);
        state = PARAMETER;
        break;
      case SETTINGS:
        state = PARAMETER;
        break;
      case SETTINGSVALUE:
        state = SETTINGS;
        showSettingsPage();
        break;
    }
  }

  //Encoder switch
  recallButton.update();
  if (recallButton.held()) {
    //If Recall button held, return to current patch setting
    //which clears any changes made
    state = PATCH;
    //Recall the current patch
    patchNo = patches.first().patchNo;
    recallPatch(patchNo);
    state = PARAMETER;
  } else if (recallButton.numClicks() == 1) {
    switch (state) {
      case PARAMETER:
        state = RECALL;  //show patch list
        break;
      case RECALL:
        state = PATCH;
        //Recall the current patch
        patchNo = patches.first().patchNo;
        recallPatch(patchNo);
        state = PARAMETER;
        break;
      case SAVE:
        showRenamingPage(patches.last().patchName);
        patchName = patches.last().patchName;
        state = PATCHNAMING;
        break;
      case PATCHNAMING:
        if (renamedPatch.length() < 12)  //actually 12 chars
        {
          renamedPatch.concat(String(currentCharacter));
          charIndex = 0;
          currentCharacter = CHARACTERS[charIndex];
          showRenamingPage(renamedPatch);
        }
        break;
      case DELETE:
        //Don't delete final patch
        if (patches.size() > 1) {
          state = DELETEMSG;
          patchNo = patches.first().patchNo;     //PatchNo to delete from SD card
          patches.shift();                       //Remove patch from circular buffer
          deletePatch(String(patchNo).c_str());  //Delete from SD card
          loadPatches();                         //Repopulate circular buffer to start from lowest Patch No
          renumberPatchesOnSD();
          loadPatches();                      //Repopulate circular buffer again after delete
          patchNo = patches.first().patchNo;  //Go back to 1
          recallPatch(patchNo);               //Load first patch
        }
        state = PARAMETER;
        break;
      case SETTINGS:
        state = SETTINGSVALUE;
        showSettingsPage();
        break;
      case SETTINGSVALUE:
        settings::save_current_value();
        state = SETTINGS;
        showSettingsPage();
        break;
    }
  }
}

void reinitialiseToPanel() {
  //This sets the current patch to be the same as the current hardware panel state - all the pots
  //The four button controls stay the same state
  //This reinialises the previous hardware values to force a re-read
  muxInput = 0;
  for (int i = 0; i < MUXCHANNELS; i++) {
    mux1ValuesPrev[i] = RE_READ;
    mux2ValuesPrev[i] = RE_READ;
  }
  patchName = INITPATCHNAME;
  showPatchPage("Initial", "Panel Settings");
}

void checkEncoder() {
  //Encoder works with relative inc and dec values
  //Detent encoder goes up in 4 steps, hence +/-3

  long encRead = encoder.read();
  if ((encCW && encRead > encPrevious + 3) || (!encCW && encRead < encPrevious - 3)) {
    switch (state) {
      case PARAMETER:
        state = PATCH;
        patches.push(patches.shift());
        patchNo = patches.first().patchNo;
        recallPatch(patchNo);
        state = PARAMETER;
        break;
      case RECALL:
        patches.push(patches.shift());
        break;
      case SAVE:
        patches.push(patches.shift());
        break;
      case PATCHNAMING:
        if (charIndex == TOTALCHARS) charIndex = 0;  //Wrap around
        currentCharacter = CHARACTERS[charIndex++];
        showRenamingPage(renamedPatch + currentCharacter);
        break;
      case DELETE:
        patches.push(patches.shift());
        break;
      case SETTINGS:
        settings::increment_setting();
        showSettingsPage();
        break;
      case SETTINGSVALUE:
        settings::increment_setting_value();
        showSettingsPage();
        break;
    }
    encPrevious = encRead;
  } else if ((encCW && encRead < encPrevious - 3) || (!encCW && encRead > encPrevious + 3)) {
    switch (state) {
      case PARAMETER:
        state = PATCH;
        patches.unshift(patches.pop());
        patchNo = patches.first().patchNo;
        recallPatch(patchNo);
        state = PARAMETER;
        break;
      case RECALL:
        patches.unshift(patches.pop());
        break;
      case SAVE:
        patches.unshift(patches.pop());
        break;
      case PATCHNAMING:
        if (charIndex == -1)
          charIndex = TOTALCHARS - 1;
        currentCharacter = CHARACTERS[charIndex--];
        showRenamingPage(renamedPatch + currentCharacter);
        break;
      case DELETE:
        patches.unshift(patches.pop());
        break;
      case SETTINGS:
        settings::decrement_setting();
        showSettingsPage();
        break;
      case SETTINGSVALUE:
        settings::decrement_setting_value();
        showSettingsPage();
        break;
    }
    encPrevious = encRead;
  }
}

void showSettingsPage() {
  showSettingsPage(settings::current_setting(), settings::current_setting_value(), state);
}

void loop() {

  octoswitch.update(500);
  srp.update();

  checkMux();
  writeDACs();

  checkSwitches();
  checkEncoder();

  usbMIDI.read(midiChannel);  //USB Client MIDI
  MIDI.read(midiChannel);     //MIDI 5 Pin DIN

  if (millis() - unmuteDelay >= delayInterval) {
    // Reset the timer for the next iteration
    unmuteDelay = millis();

    srp.writePin(SIGNAL_MUTE, HIGH);
  }
}
