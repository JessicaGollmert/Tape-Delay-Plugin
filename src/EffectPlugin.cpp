//
//  EffectPlugin.cpp
//  MyEffect Plugin Source Code
//
//  Used to define the bodies of functions used by the plugin, as declared in EffectPlugin.h.
//

#include "EffectPlugin.h"

////////////////////////////////////////////////////////////////////////////
// EFFECT - represents the whole effect plugin
////////////////////////////////////////////////////////////////////////////

// Called to create the effect (used to add your effect to the host plugin)
extern "C" {
    CREATE_FUNCTION createEffect(float sampleRate) {
        ::stk::Stk::setSampleRate(sampleRate);
        
        //==========================================================================
        // CONTROLS - Use this array to completely specify your UI
        // - tells the system what parameters you want, and how they are controlled
        // - add or remove parameters by adding or removing entries from the list
        // - each control should have an expressive label / caption
        // - controls can be of different types: ROTARY, BUTTON, TOGGLE, SLIDER, or MENU (see definitions)
        // - for rotary and linear sliders, you can set the range of values (make sure the initial value is inside the range)
        // - for menus, replace the three numeric values with a single array of option strings: e.g. { "one", "two", "three" }
        // - by default, the controls are laid out in a grid, but you can also move and size them manually
        //   i.e. replace AUTO_SIZE with Bounds(50,50,100,100) to place a 100x100 control at (50,50)
        
        const Parameters CONTROLS = {
            //  name,       type,              min, max, initial, size
            {   "Delay Time L",  Parameter::ROTARY, 0.0, 1.0, 0.0, AUTO_SIZE  },
            {   "Delay Time R",  Parameter::ROTARY, 0.0, 1.0, 0.0, AUTO_SIZE  },
            {   "Delay Level",  Parameter::ROTARY, 0.0, 1.0, 0.0, AUTO_SIZE  },
            {   "Feedback",  Parameter::ROTARY, 0.0, 1.0, 0.5, AUTO_SIZE  },
            {   "Repeat Brightness",  Parameter::ROTARY, 0.0, 1.0, 0.5, AUTO_SIZE  },
            {   "Mod Rate",  Parameter::ROTARY, 0.0, 1.0, 0.5, AUTO_SIZE  },
            {   "Mod Level",  Parameter::ROTARY, 0.0, 1.0, 0.5, AUTO_SIZE  },
            {   "Tape Degradation",  Parameter::ROTARY, 0.0, 1.0, 0.0, AUTO_SIZE  },
            {   "Wet/Dry Mix",  Parameter::ROTARY, 0.0, 1.0, 0.5, AUTO_SIZE  },
//            {   "",  Parameter::ROTARY, 0.0, 1.0, 0.5, AUTO_SIZE  },
        };

        const Presets PRESETS = {
            { "Mono Slapback Echo", { 0.100, 0.100, 0.500, 0.232, 0.500, 0.000, 0.000, 0.000, 0.500 } },
            { "Saturated Ping-Pong", { 0.350, 0.689, 0.655, 0.684, 0.492, 0.489, 0.490, 0.202, 0.500 } },
           { "Heavy Modulation", { 0.423, 0.628, 1.000, 0.140, 0.200, 1.000, 1.000, 1.000, 0.373 } },
        };

        return (APDI::Effect*)new MyEffect(CONTROLS, PRESETS);
    }
}

// Constructor: called when the effect is first created / loaded
MyEffect::MyEffect(const Parameters& parameters, const Presets& presets)
: Effect(parameters, presets)
{
    iVibBufferSize = (int)(2.0 * 192000);
     pfVibCircularBuffer = new float[iVibBufferSize];

    for(int iPos = 0; iPos < iVibBufferSize; iPos++) {
         pfVibCircularBuffer[iPos] = 0;
    }

    iVibBufferWritePos = 0;
}

// Destructor: called when the effect is terminated / unloaded
MyEffect::~MyEffect()
{
    delete[] pfVibCircularBuffer;
}

// EVENT HANDLERS: handle different user input (button presses, preset selection, drop menus)

void MyEffect::presetLoaded(int iPresetNum, const char *sPresetName)
{
    // A preset has been loaded, so you could perform setup, such as retrieving parameter values
    // using getParameter and use them to set state variables in the plugin
}

void MyEffect::optionChanged(int iOptionMenu, int iItem)
{
    // An option menu, with index iOptionMenu, has been changed to the entry, iItem
}

void MyEffect::buttonPressed(int iButton)
{
      // A button, with index iButton, has been pressed
}

float MyEffect::InterpolatedRead(float fVibBufferReadPos) // interpolated read for modulation
 {
     int iPos1, iPos2;
     float fPdiff, fVdiff, fResult;
     
     iPos1 = (int)fVibBufferReadPos;
     iPos2 = iPos1 + 1;
     if(iPos2 == iVibBufferSize) {
         iPos2 = 0;
     }

     fPdiff = fVibBufferReadPos - iPos1;
     fVdiff = pfVibCircularBuffer[iPos2] - pfVibCircularBuffer[iPos1];
     fResult = pfVibCircularBuffer[iPos1] + (fVdiff * fPdiff);

     return fResult;
 }


// Applies audio processing to a buffer of audio
// (inputBuffer contains the input audio, and processed samples should be stored in outputBuffer)

void MyEffect::process(const float** inputBuffers, float** outputBuffers, int numSamples)
{
    float fIn0, fIn1, fOut0 = 0, fOut1 = 0;
    const float *pfInBuffer0 = inputBuffers[0], *pfInBuffer1 = inputBuffers[1];
    float *pfOutBuffer0 = outputBuffers[0], *pfOutBuffer1 = outputBuffers[1];
    
    float fDelayTime0 = parameters[0]; // left
    float fDelayTime1 = parameters[1]; // right
    float fDelayLevel = parameters[2] * 0.30; // 0 to 0.3
    float fFeedback = parameters[3];
    float fCutoff = parameters[4] * 4000 + 1000; // 1000Hz to 5000Hz
    float fModRate = parameters[5] * 2.5 + 0.5; //  0.5Hz to 3Hz
    float fModLevel = parameters[6] * 2.0; // 0 to 2
    float fInGain = parameters[7] * 2.0 + 1.0; // 1 to 3
    float fDryAmount = parameters[8];
    float fWetAmount = 1 - fDryAmount; // inverse of dry amount
    float fCurvature = 10000; // curvature of distortion function
    float fModDepth = 0.0009;
    
    int iDelay0 = fDelayTime0 * getSampleRate(); // left samples to delay
    int iDelay1 = fDelayTime1 * getSampleRate(); // right samples to delay
    
    generator.setFrequency(fModRate); // set freuency of sine generator
    lpfLeft.setCutoff(fCutoff);
    lpfRight.setCutoff(fCutoff);
    fSR = getSampleRate();

    while(numSamples--)
    {
        // Get sample from input
        fIn0 = *pfInBuffer0++;
        fIn1 = *pfInBuffer1++;
        
        float fWet0 = fIn0; // wet signal for modulation
        float fWet1 = fIn1;
        
        float fDry0 = fIn0; // dry signal for output
        float fDry1 = fIn1;
        
        float fDelSig0 = delay0.read(iDelay0); // left delayed signal
        float fDelSig1 = delay1.read(iDelay1); // right delayed signal
        
        // MODULATION CODE
        float fMod = generator.tick(); // generate sine wave
        fMod = fMod * fModDepth + fModDepth; // set sine depth, scale & offset
        fMod *= fModLevel; // set modulation amount

        iVibBufferWritePos++;
        if(iVibBufferWritePos == iVibBufferSize) {
            iVibBufferWritePos = 0;
        }
        pfVibCircularBuffer[iVibBufferWritePos] = ((fWet0 + fWet1) * 0.5f); // write to modulation buffer
        
        float fVibBufferReadPos = iVibBufferWritePos - (fMod * fSR); // delay read position based on sine wave
        if(fVibBufferReadPos < 0) {
            fVibBufferReadPos += iVibBufferSize;
        }
    
        float fVibrato = InterpolatedRead(fVibBufferReadPos); // read from modulation buffer
      
        
        // DISTORTION CODE
        if (fDelayTime0 >= 0) { // only apply distortion when delays are audible
            fDelSig0 *= (fInGain * 0.3); // pre-gain
            fDelSig0 = SoftClipDistortion(fDelSig0, fCurvature); // shape singal
        }
        if (fDelayTime1 >= 0) {
            fDelSig1 *= (fInGain * 0.3);
            fDelSig1 = SoftClipDistortion(fDelSig1, fCurvature);
        }
        
        float fDelMix0 = lpfLeft.tick(fDelSig0 * fDelayLevel); // apply LPF to repeats & set their level
        float fDelMix1 = lpfRight.tick(fDelSig1 * fDelayLevel);
    
        delay0.write((fDelMix0 * fFeedback) + fVibrato); // set feedback level & apply vibrato to repeats
        delay1.write((fDelMix1 * fFeedback) + fVibrato);
        
        fOut0 = (fDry0 * fDryAmount) + (fDelMix0 * fWetAmount); // Wet/Dry signals attenuated & combined
        fOut1 = (fDry1 * fDryAmount) + (fDelMix1 * fWetAmount);
        
        // Copy result to output
        *pfOutBuffer0++ = fOut0;
        *pfOutBuffer1++ = fOut1;
    }
}
