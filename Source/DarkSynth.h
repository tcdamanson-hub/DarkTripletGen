#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

// Synthe de pre-ecoute polyphonique (FM simple).
//  canal 1 = melodie : cloche / piano sombre
//  canal 2 = accords + basse : nappe douce
class DarkSynth
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        filter.prepare ({ sr, (juce::uint32) juce::jmax (1, maxBlock), 1 });
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setResonance (0.7f);
        for (auto& v : voices) { v.adsr.setSampleRate (sr); v.active = false; }
    }

    void setParams (float cutoffHz, float releaseSec, float gain)
    {
        cutoff = cutoffHz; release = releaseSec; outGain = gain;
        filter.setCutoffFrequency (juce::jlimit (40.0f, 18000.0f, cutoff));
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

private:
    struct Voice
    {
        bool active = false;
        int note = -1, channel = 1;
        double freq = 220.0, pc = 0.0, pm = 0.0;
        float vel = 1.0f, modEnv = 1.0f, modDecay = 0.999f, index = 2.0f, ratio = 2.0f, level = 1.0f;
        juce::ADSR adsr;
        juce::uint32 age = 0;
    };

    double sr = 44100.0;
    std::array<Voice, 16> voices;
    juce::dsp::StateVariableTPTFilter<float> filter;
    float cutoff = 3000.0f, release = 0.6f, outGain = 0.7f;
    juce::uint32 counter = 0;

    static double mtof (int n) { return 440.0 * std::pow (2.0, (n - 69) / 12.0); }

    void handle (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
        {
            Voice* v = nullptr;
            for (auto& x : voices) if (! x.active) { v = &x; break; }
            if (v == nullptr)
            {
                v = &voices[0];
                for (auto& x : voices) if (x.age < v->age) v = &x; // vole la plus ancienne
            }
            const bool mel = m.getChannel() == 1;
            v->active  = true;
            v->note    = m.getNoteNumber();
            v->channel = m.getChannel();
            v->freq    = mtof (v->note);
            v->pc = v->pm = 0.0;
            v->vel     = 0.3f + 0.7f * m.getFloatVelocity();
            v->age     = ++counter;
            v->ratio   = mel ? 2.0f : 1.0f;
            v->index   = mel ? 2.6f : 0.8f;
            v->modEnv  = 1.0f;
            v->modDecay = (float) std::exp (-1.0 / ((mel ? 0.18 : 0.6) * sr));
            v->level   = mel ? 0.55f : (v->note < 48 ? 0.45f : 0.22f);
            v->adsr.setParameters (mel ? juce::ADSR::Parameters { 0.002f, 1.1f, 0.25f, release }
                                       : juce::ADSR::Parameters { 0.03f, 0.8f, 0.6f, release * 1.2f });
            v->adsr.noteOn();
        }
        else if (m.isNoteOff())
        {
            for (auto& v : voices)
                if (v.active && v.note == m.getNoteNumber() && v.channel == m.getChannel())
                    v.adsr.noteOff();
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            for (auto& v : voices) v.adsr.noteOff();
        }
    }

    void renderRange (juce::AudioBuffer<float>& buf, int start, int end)
    {
        if (end <= start) return;
        auto* L = buf.getWritePointer (0);
        auto* R = buf.getNumChannels() > 1 ? buf.getWritePointer (1) : nullptr;
        const double twoPi = juce::MathConstants<double>::twoPi;

        for (int i = start; i < end; ++i)
        {
            float s = 0.0f;
            for (auto& v : voices)
            {
                if (! v.active) continue;
                const float env = v.adsr.getNextSample();
                if (! v.adsr.isActive()) { v.active = false; continue; }
                const double mod = std::sin (twoPi * v.pm) * v.index * v.modEnv;
                s += (float) std::sin (twoPi * v.pc + mod) * env * v.vel * v.level;
                v.pc += v.freq / sr;              if (v.pc >= 1.0) v.pc -= 1.0;
                v.pm += v.freq * v.ratio / sr;    if (v.pm >= 1.0) v.pm -= 1.0;
                v.modEnv *= v.modDecay;
            }
            s = filter.processSample (0, s);
            s = std::tanh (s * 1.2f) * outGain * 0.5f;
            L[i] += s;
            if (R != nullptr) R[i] += s;
        }
    }
};
