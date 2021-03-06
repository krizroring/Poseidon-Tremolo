/*
 * WaveGenerator.cpp - Wave generator for Arduino
 * 2016 Christian Roring
 *
 * Code is based on the Harmless tremolo with modifications for adding a modulation LFO
 * http://www.diystompboxes.com/smfforum/index.php?topic=95586.0
 */

#include "Arduino.h"
#include "WaveGenerator.h"
#include "MedusaDisplay.h"

static const float multiplier_table[] PROGMEM =  { 0.25, 0.5, 1, 2, 4};
static const float LFO_OFFSET = PI / 8; //  tweak for best sound

WaveGenerator::WaveGenerator() {
    lfo = 8; // param or config?

    // firstPeriod = millis();

    waveFn[0] = &WaveGenerator::waveSin;
    waveFn[1] = &WaveGenerator::waveSquare;
    waveFn[2] = &WaveGenerator::waveTri;
    waveFn[3] = &WaveGenerator::waveSaw;
    waveFn[4] = &WaveGenerator::waveReverseSaw;
    waveFn[5] = &WaveGenerator::waveTriSaw;
};

void WaveGenerator::setParams(byte _bpm, byte _depth, byte _wave, byte _multi, byte _mod, byte _pedalMode) {
    bpm = _bpm;
    depth = _depth;
    wave = _wave;
    multi = _multi;
    mod = _mod;
    pedalMode = _pedalMode;

    multiplier = pgm_read_float(multiplier_table+multi);

    setDepth(depth);
    restart();
    setTappedBPM(bpm);
}

int WaveGenerator::generate() {
    if (pedalMode == 0) {
        if (elapsedMillisPeriod >= periodMultiplied) {
            elapsedMillisPeriod = elapsedMillisPeriod - periodMultiplied;
        }

        float modulation = 0.0;
        if (mod > 0) modulation = generateLFO();

        return constrain((this->*waveFn[wave])(elapsedMillisPeriod) + modulation, MAX_LDR_DEPTH, MIN_LDR_DEPTH);
    } else {
        return ldrDepth;
    }
};

void WaveGenerator::restart() {
    elapsedMillisPeriod = 0; // reset elapsed millis
    elapsedMillisLFO = 0;
}

byte WaveGenerator::setTappedBPM(byte _bpm) {
    bpm = constrain(_bpm, MIN_BPM, MAX_BPM);

    setBPM();
    return bpm;
}

byte WaveGenerator::updateBPM(int _modifier) {
    int _tmp = bpm + _modifier;

    if(_tmp >= MIN_BPM && _tmp <= MAX_BPM) {
        bpm = _tmp;
    }

    setBPM();
    return bpm;
}

byte WaveGenerator::updateDepth(int _modifier) {
    int _tmp = depth + _modifier;

    if(_tmp >= MIN_DEPTH && _tmp <= MAX_DEPTH) {
        depth = _tmp;
    }

    return setDepth(depth);
}
byte WaveGenerator::setDepth(byte _depth) {
    // map exp to 256 ?? - depends on read resolution
    depth = _depth;
    ldrDepth = map(depth, MAX_DEPTH, MIN_DEPTH, MAX_LDR_DEPTH, MIN_LDR_DEPTH);
    return depth;
}

byte WaveGenerator::updateWave(int _modifier) {
    int _tmp = wave + _modifier;

    if(_tmp >= MIN_WAVE && _tmp <= MAX_WAVE) {
        wave = _tmp;
    }

    return wave;
}
byte WaveGenerator::updateMultiplier(int _modifier) {
    int _tmp = multi + _modifier;

    if(_tmp >= MIN_MULTI && _tmp <= MAX_MULTI) {
        multi = _tmp;
    }

    multiplier = pgm_read_float(multiplier_table+multi);

    updatePeriod(period / multiplier);

    return multi;
}
byte WaveGenerator::updateModulation(int _modifier) {
    int _tmp = mod + _modifier;

    if(_tmp >= MIN_MOD && _tmp <= MAX_MOD) {
        mod = _tmp;
    }

    return mod;
}

byte WaveGenerator::setPedalMode(int _modifier) {
    int _tmp = pedalMode + _modifier;

    if(_tmp >= 0 && _tmp <= 1) {
        pedalMode = _tmp;
    }

    return pedalMode;
}

//Private functions
void WaveGenerator::setBPM() {
    period = BPM_2_PERIOD(bpm);

    updatePeriod(period / multiplier);
};

void WaveGenerator::updatePeriod(unsigned int _newPeriodMultiplied) {
    //adjust first period so that we are the same % of the way through the period
    //so there aren't any jarring transitions
    elapsedMillisPeriod = map(elapsedMillisPeriod, 0, periodMultiplied - 1, 0, _newPeriodMultiplied - 1);

    unsigned int newLFOPeriod = period * lfo;
    elapsedMillisLFO = map(elapsedMillisLFO, 0, lfoPeriod - 1, 0, newLFOPeriod - 1);

    lfoPeriod = newLFOPeriod;
    periodMultiplied = _newPeriodMultiplied;
    halfMultipliedPeriod = periodMultiplied / 2;
    threeQuarterMultipliedPeriod = (periodMultiplied / 4) + halfMultipliedPeriod;
}

// actually cos so we're on at period start
int WaveGenerator::waveSin(unsigned int _offset) {
    float rads = ((float)_offset / (float)periodMultiplied) * TWO_PI;
    return constrain((cos(rads) + 1.0) * (MIN_LDR_DEPTH - ldrDepth) / 2.0 + ldrDepth, MAX_LDR_DEPTH, MIN_LDR_DEPTH); //cap at 255 instead of 256
}

int WaveGenerator::waveSquare(unsigned int _offset) {
    if(_offset < halfMultipliedPeriod) {
        return MIN_LDR_DEPTH;
    } else {
        return ldrDepth;
    }
}

int WaveGenerator::waveTri(unsigned int _offset) {
    if(_offset < halfMultipliedPeriod) {
        return map(_offset, 0, halfMultipliedPeriod, MIN_LDR_DEPTH, ldrDepth);
    } else {
        return map(_offset, halfMultipliedPeriod + 1, periodMultiplied - 1, ldrDepth, MIN_LDR_DEPTH);
    }
}

int WaveGenerator::waveSaw(unsigned int _offset) {
    return map(_offset, 0, periodMultiplied - 1, ldrDepth, MIN_LDR_DEPTH);
}

int WaveGenerator::waveReverseSaw(unsigned int _offset) {
  return map(_offset, 0, periodMultiplied - 1, MIN_LDR_DEPTH, ldrDepth);
}

int WaveGenerator::waveTriSaw(unsigned int _offset) {
    if(_offset < threeQuarterMultipliedPeriod) {
        return map(_offset, 0, threeQuarterMultipliedPeriod, MIN_LDR_DEPTH, ldrDepth);
    } else {
        return map(_offset, threeQuarterMultipliedPeriod + 1, periodMultiplied - 1, ldrDepth, MIN_LDR_DEPTH);
    }
}

float WaveGenerator::generateLFO() {
    if (elapsedMillisLFO >= lfoPeriod) {
        elapsedMillisLFO = elapsedMillisLFO - lfoPeriod;
    }

    // generate the mod rad
    float modRads = ((float)elapsedMillisLFO / (float)lfoPeriod ) * TWO_PI;
    return cos(modRads + LFO_OFFSET) * mod; // magic numbers: LFO_OFFSET (1/8 of the 1 PI perdiod)
}
