# Stratus Pulse

A VCDO based 6 voice Polysynth using Electrid Druid VCDO1 chips.

This is my first attempt at a polysynth and it took me several years as its a learning curve with all of these things.

It started out with 12 Electric Druid VCDO1 chips as the osillators with the intention of just replacing the Crumar Stratus organ voice section with something a little more synth based. But eventually I realized that the whole synth should be replaced with new oscillators, filters, LFO, evnvelopes etc. I tried switching to AS3340 CEM based oscillators but I couldn't keep the tuning stable for 12 voices and I am not competent enough to write an autotune routine, so I went back to more "stable" VCDOs.

The synth programs are controlled by a Teensy 3.6 which takes care of all program parameters, screen etc. The oscillators are controlled by my own 6 voice poly MIDI to CV converter and that takes care of the MIDI in and usbMIDI.

# Final specs are as follows:

* 6 voice polyphonic
* 2 DCO's per voice with 16 waveforms for the main oscillator and 32 waveforms for the sub oscillator althouh its just 8 waveforms with 4 octave variations.
* 16 waveform LFO with delay and slope as per the original Stratus and mono/multi trigger using the Electric Druid Taplfo3d
* Noise source of White/Pink using the Electric Druid Noise2
* Polyphonic glide
* AS3320 pole mixing filters based on the Matrix 12 filters.
* 3 sets of envelope generators using the Electric Druid EnvGen8c, looping on the Filter evnvelope and velocity available to all.
* Bit crush to destroy the oscillators.
* 999 Memories.
* MIDI and usbMIDI connections.

I have no schematics for this as such as it was built over many years and mostly written down in a notepad which needs translating to real schematics, but I generally followed the datasheets for the Electric Druid chips and I will link all the sites I used to develop the synth.

https://fitzgreyve2.blogspot.com
https://github.com/craigyjp/16bit-MIDI-to-CV-polyphonic-converter
https://electricdruid.net/multimode-filters-part-2-pole-mixing-filters/
https://electricdruid.net/product/vcdo-wavetable-oscillator/
https://electricdruid.net/product/noise2-white-pink-generator/
https://electricdruid.net/product/taplfo3/
https://electricdruid.net/product/envgen8/


