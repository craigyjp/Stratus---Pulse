// This optional setting causes Encoder to use more optimized code,
// It must be defined before Encoder.h is included.
#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>
#include <Bounce.h>
#include "TButton.h"
#include <ADC.h>
#include <ADC_util.h>

ADC *adc = new ADC();

//Teensy 3.6 - Mux Pins
#define MUX_0 28
#define MUX_1 32
#define MUX_2 30
#define MUX_3 31

#define MUX1_S A0 // ADC1
#define MUX2_S A1 // ADC1

//Mux 1 Connections
#define MUX1_osc1MainWave 0
#define MUX1_filterType 1
#define MUX1_filterRes 2
#define MUX1_filterCutoff 3
#define MUX1_osc1MainLevel 4
#define MUX1_osc1SubLevel 5
#define MUX1_osc2MainLevel 6
#define MUX1_osc2SubLevel 7
#define MUX1_volumeControl 8
#define MUX1_bitCrush 9
#define MUX1_mainAttack 10
#define MUX1_mainDecay 11
#define MUX1_mainSustain 12
#define MUX1_mainRelease 13
#define MUX1_PitchBend 14
#define MUX1_modwheel 15

//Mux 2 Connections
#define MUX2_0 0
#define MUX2_LFOWaveform 1
#define MUX2_lfoAlt 2
#define MUX2_lfoMult 3
#define MUX2_osc1Detune 4
#define MUX2_osc2SubWave 5 
#define MUX2_osc2MainWave 6
#define MUX2_osc1SubWave 7 
#define MUX2_EGlevel 8
#define MUX2_noiseLevel 9
#define MUX2_GlideTime 10
#define MUX2_MasterTune 11
#define MUX2_LfoRate 12 
#define MUX2_LfoSlope 13
#define MUX2_LfoDelay 14
#define MUX2_LfoDepth 15

//74HC165 MUX pins

#define EG_INV_SW 0
#define LOOP_SW 1
#define KEYTRK_SW 2
#define POLE_SW 3

#define VCF_EG_SW 8
#define PITCH_EG_SW 9
#define VELO_SW 10
#define VCA_EG_SW 11

#define OCTAVE1_SW 16
#define OSC1_EG_SW 17
#define GLIDE_SW 18
#define OCTAVE2_SW 19
#define OSC2_EG_SW 20
#define UNISON_SW 21

#define VCA_SW 24
#define MULTI_SW 25
#define VCO_SW 26
#define VCF_SW 27

//74HC595 LEDs

#define EG_INV_LED 0
#define LOOP_LED 1
#define KEYTRK_LED 2
#define POLE_LED 3
#define VCF_EG_LED 4
#define PITCH_EG_LED 5
#define VELO_LED 6
#define VCA_EG_LED 7

#define VCA_SW_LED 8
#define MULTI_SW_LED 9
#define VCF_SW_LED 10
#define VCO_SW_LED 11

#define OCTAVE1_LED 12
#define OSC1_EG_LED 13
#define GLIDE_LED 14
#define OCTAVE2_LED 15
#define OSC2_EG_LED 16
#define UNISON_LED 17

//74HC595 +5v OUTPUTS

#define GLIDE_OUT 0
#define OCTAVE1_OUT 1
#define OCTAVE2_OUT 2
#define UNISON_OUT 3
#define LOOP_OUT 4
#define EG_INV_OUT 5
#define KEYTRK_OUT 6
#define POLE_OUT 7

#define FILTER_A_OUT 8
#define FILTER_B_OUT 9
#define FILTER_C_OUT 10
//#define OCTAVE_SELECT_0 11
//#define OCTAVE_SELECT_2 12
#define LFOALT_OUT 13
#define OSC1_EG_OUT 14
#define OSC2_EG_OUT 15

#define PITCH_VELO_OUT 16
#define FILTER_VELO_OUT 17
#define VCO_OUT 18
#define VCF_OUT 19
#define VCA_OUT 20
#define MULTI_OUT 21
#define AMP_VELO_OUT 22
//#define OCTAVE_SELECT_1 23

#define OCTAVE 24
#define PLUS 26
#define MINUS 25
#define FILTER_ENV_OUT 27

// System controls

#define RECALL_SW 18
#define SAVE_SW 24
#define SETTINGS_SW 12
#define BACK_SW 10

//#define OCTAVEUP_SW 26
//#define OCTAVEDOWN_SW 27

#define ENCODER_PINA 4
#define ENCODER_PINB 5

#define DEMUX_S0 37
#define DEMUX_S1 36
#define DEMUX_S2 23
#define DEMUX_S3 22
#define CHIP_SEL1 19
#define CHIP_SEL2 25

#define MUXCHANNELS 16
#define DEMUXCHANNELS 16
#define DEMUXCHANNELS2 8
#define MUXBANK 2
#define QUANTISE_FACTOR 7

#define DEBOUNCE 30

static byte muxInput = 0;
static byte muxOutput = 0;
static byte muxOutput2 = 0;
static byte chipSelSwitch = 0;
static int mux1ValuesPrev[MUXCHANNELS] = {};
static int mux2ValuesPrev[MUXCHANNELS] = {};

static int mux1Read = 0;
static int mux2Read = 0;

static long encPrevious = 0;


TButton saveButton{ SAVE_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION };
TButton settingsButton{ SETTINGS_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION };
TButton backButton{ BACK_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION };
TButton recallButton{ RECALL_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION }; // on encoder

Encoder encoder(ENCODER_PINB, ENCODER_PINA);//This often needs the pins swapping depending on the encoder

void setupHardware()
{
  adc->adc0->setAveraging(32);                                          // set number of averages 0, 4, 8, 16 or 32.
  adc->adc0->setResolution(10);                                         // set bits of resolution  8, 10, 12 or 16 bits.
  adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_LOW_SPEED);  // change the conversion speed
  adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED);           // change the sampling speed

  //MUXs on ADC1
  adc->adc1->setAveraging(32);                                          // set number of averages 0, 4, 8, 16 or 32.
  adc->adc1->setResolution(10);                                         // set bits of resolution  8, 10, 12 or 16 bits.
  adc->adc1->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_LOW_SPEED);  // change the conversion speed
  adc->adc1->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED);           // change the sampling speed


    //Mux ADC
  pinMode(MUX1_S, INPUT_DISABLE);
  pinMode(MUX2_S, INPUT_DISABLE);

  //Mux address pins

  pinMode(MUX_0, OUTPUT);
  pinMode(MUX_1, OUTPUT);
  pinMode(MUX_2, OUTPUT);
  pinMode(MUX_3, OUTPUT);

  digitalWrite(MUX_0, LOW);
  digitalWrite(MUX_1, LOW);
  digitalWrite(MUX_2, LOW);
  digitalWrite(MUX_3, LOW);
  
  pinMode (DEMUX_S0, OUTPUT);
  pinMode (DEMUX_S1, OUTPUT);
  pinMode (DEMUX_S2, OUTPUT);
  pinMode (DEMUX_S3, OUTPUT);
  pinMode (CHIP_SEL1, OUTPUT);
  pinMode (CHIP_SEL2, OUTPUT);
  
  digitalWrite(DEMUX_S0,LOW);
  digitalWrite(DEMUX_S1,LOW);
  digitalWrite(DEMUX_S2,LOW);
  digitalWrite(DEMUX_S3,LOW);
  digitalWrite(CHIP_SEL1,HIGH);
  digitalWrite(CHIP_SEL2,HIGH);
  
  //Switches
  
  pinMode(RECALL_SW, INPUT_PULLUP); //On encoder
  pinMode(SAVE_SW, INPUT_PULLUP);
  pinMode(SETTINGS_SW, INPUT_PULLUP);
  pinMode(BACK_SW, INPUT_PULLUP);

}
