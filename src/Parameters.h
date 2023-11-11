//Values below are just for initialising and will be changed when synth is initialised to current panel controls & EEPROM settings
byte midiChannel = MIDI_CHANNEL_OMNI;//(EEPROM)
String patchName = INITPATCHNAME;
boolean encCW = true;//This is to set the encoder to increment when turned CW - Settings Option
int resolutionFrig = 3;
int bankselect = 0;
unsigned long unmuteDelay = 0;
const unsigned long delayInterval = 1000;  // 50ms delay

int pot1 = 0;
int pot2 = 0;
int pot3 = 0;
int effectNumber = 0;
int mix = 0;
int mixa = 0;
int mixb = 0;
int pot1str = 0;
int pot2str = 0;
int pot3str = 0;
int effectNumberstr = 0;
int mixstr = 0;
int internal = 0;
String FV1effect = "                ";

int noiseLevel = 0;
int noiseLevelstr = 0; // for display
int volumeControl = 0;
int volumeControlstr = 0; // for display

int octave1 = 0;
int octave2 = 0;
int octaveup = 0;
int octavedown = 0;
int unisonSW =0;
int glide = 0;
int velocitySW = 0;
int osc1EGinv = 0;
int osc2EGon = 0;

int mainAttack = 0;
int mainAttackstr = 0;
int mainDecay = 0;
int mainDecaystr = 0;
int mainSustain = 0;
int mainSustainstr = 0;
int mainRelease = 0;
int mainReleasestr = 0;

int lfoVCO = 0;
int lfoVCF = 0;
int lfoVCA = 0;
int monoMulti = 0;
boolean effects = false;

int pitchBendRange = 8;
int PitchBendLevel = 0;
int PitchBendLevelstr = 0; // for display
int modWheelDepth = 0.2f;
int modWheelLevel = 0;
int modWheelLevelstr = 0;

int GlideTimestr = 0; // for display
int GlideTime = 0;
int octave_select0 = 0;
int octave_select1 = 0;
int octave_select2 = 0;
int returnvalue = 0;
int fixbitCrush = 0;

String StratusLFOWaveform = "                ";
String StratusSUBWaveform = "                ";
String StratusOSCWaveform = "                    ";
String LfoMultiplierstr = "    ";
int LfoSlope = 0;
int LfoSlopestr = 0;
int LfoDepth = 0;
int LfoDepthstr = 0; // for display
int lfoAlt = 0;
int LfoRate = 0;
int LfoRatestr = 0; //for display
int bitCrush = 0;
int bitCrushstr = 0; // for display
int LFOWaveform = 0;
int LFOWaveformstr = 0;
int lfoDestVCO = 0;
int lfoDestVCF = 0;
int lfoDestVCA = 0;
int lfoMult = 0;
int lfoMultstr = 0;
int LfoDelay = 1;
int LfoDelaystr = 0; // for display

int osc1SubLevel = 0;
int osc1SubLevelstr = 0; // for display
int osc1MainLevel = 0; // for display
int osc1MainLevelstr = 0;
int osc2SubLevel = 0;
int osc2SubLevelstr = 0; // for display
int osc2MainLevelstr = 0; //for display
int osc2MainLevel = 0;

int osc1MainWave = 0;
int osc1MainWavestr = 0;
int osc2MainWave = 0;
int osc2MainWavestr = 0;
int osc2SubWave = 0;
int osc2SubWavestr = 0;
int osc1SubWave = 0;
int osc1SubWavestr = 0;

int ampAttack = 0;
int ampDecay = 0;
int ampSustain = 0;
int ampRelease = 0;
int ampEGlevel = 0;
int ampVelo = 0;
int ampEG = 0;

int pitchAttack = 0;
int pitchDecay = 0;
int pitchSustain = 0;
int pitchRelease = 0;
int pitchEGlevel = 0;
int pitchVelo = 0;
int pitchEG = 0;

int EGlevel = 0;
int EGlevelstr = 0;

int MasterTune = 0;
int MasterTunestr = 0;
int detune = 0;
int osc1Detune = 0;
int osc1Detunestr = 0;

int filterAttack = 0;
int filterDecay = 0;
int filterSustain = 0;
int filterRelease = 0;
int filterEGlevel = 0;
int filterVelo = 0;
int filterEG = 0;
int filterRes = 0;
int filterResstr = 0;
int filterCutoff = 12000;
int filterCutoffstr = 12000; // for display
int filterEnv = 0; // not required
int filterType = 0;
int filterTypestr = 0;
int filterA = 0;
int filterB = 0;
int filterC = 0;
int filterPole = 0;
int filterKeyTrack = 0;
int filterLoop = 0;
int filterEGinv = 0;
int keytrackingAmount = 0.5;//MIDI CC & settings option (EEPROM)
int FilterEnv = 0;
int FilterEnvPatch = 0;
