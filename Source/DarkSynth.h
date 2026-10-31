#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

// Petit synthe mono pour pre-ecouter les melodies : 2 saws desaccordees + sub, filtre passe-bas, glide.
class DarkSynth
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        filter.prepare ({ sr, (juce::uint32) juce::jmax (1, maxBlock), 1 });
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setResonance (0.9f);
        adsr.setSampleRate (sr);
        adsr.setParameters ({ 0.003f, 0.35f, 0.55f, 0.25f });
        reset();
    }

    void reset()
    {
        filter.reset();
        adsr.reset();
        heldNote = -1;
        p1 = p2 = p3 = 0.0;
    }

    void setParams (float cutoffHz, float glideMs, float gain)
    {
        cutoff = cutoffHz;
        glideCoef = glideMs <= 0.5f ? 1.0
                                    : 1.0 - std::exp (-1.0 / (0.001 * (double) glideMs * sr));
        outGain = gain;
    }

    void render (juce::AudioBuffer<float>& buf, const juce::MidiBuffer& midi)
    {
        int pos = 0;
        for (const auto meta : midi)
        {
            const int t = juce::jlimit (0, buf.getNumSamples(), meta.samplePosition);
            renderRange (buf, pos, t);
            handle (meta.getMessage());
            pos = t;
        }
        renderRange (buf, pos, buf.getNumSamples());
    }

    void allNotesOff() { adsr.noteOff(); heldNote = -1; }

private:
    double sr = 44100.0;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::ADSR adsr;
    int heldNote = -1;
    double curFreq = 220.0, targetFreq = 220.0, glideCoef = 1.0;
    double p1 = 0, p2 = 0, p3 = 0;
    float vel = 1.0f, cutoff = 2200.0f, outGain = 0.7f;
    int ctr = 0;

    static double mtof (int n) { return 440.0 * std::pow (2.0, (n - 69) / 12.0); }

    static float blep (double t, double dt)
    {
        if (t < dt)       { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    void handle (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
        {
            targetFreq = mtof (m.getNoteNumber());
            if (heldNote < 0 && ! adsr.isActive())
                curFreq = targetFreq;
            heldNote = m.getNoteNumber();
            vel = 0.35f + 0.65f * m.getFloatVelocity();
            adsr.noteOn();
        }
        else if (m.isNoteOff() && m.getNoteNumber() == heldNote)
        {
            adsr.noteOff();
            heldNote = -1;
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            allNotesOff();
        }
    }

    void renderRange (juce::AudioBuffer<float>& buf, int start, int end)
    {
        if (end <= start) return;
        auto* L = buf.getWritePointer (0);
        auto* R = buf.getNumChannels() > 1 ? buf.getWritePointer (1) : nullptr;

        for (int i = start; i < end; ++i)
        {
            if (! adsr.isActive()) continue;

            curFreq += (targetFreq - curFreq) * glideCoef;
            const double f1 = curFreq * 1.004, f2 = curFreq * 0.996, f3 = curFreq * 0.5;
            const double d1 = f1 / sr, d2 = f2 / sr, d3 = f3 / sr;

            float s1 = (float) (2.0 * p1 - 1.0) - blep (p1, d1);
            float s2 = (float) (2.0 * p2 - 1.0) - blep (p2, d2);
            float sub = (float) std::sin (juce::MathConstants<double>::twoPi * p3);
            p1 += d1; if (p1 >= 1.0) p1 -= 1.0;
            p2 += d2; if (p2 >= 1.0) p2 -= 1.0;
            p3 += d3; if (p3 >= 1.0) p3 -= 1.0;

            const float env = adsr.getNextSample();
            if ((ctr++ & 31) == 0)
                filter.setCutoffFrequency (juce::jlimit (40.0f, 18000.0f, cutoff * (0.35f + 1.3f * env)));

            float s = (s1 + s2) * 0.28f + sub * 0.45f;
            s = filter.processSample (0, s);
            s = std::tanh (s * 1.6f) * env * vel * outGain * 0.6f;

            L[i] += s;
            if (R != nullptr) R[i] += s;
        }
    }
};
