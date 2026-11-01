#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <cmath>
#include "InstrumentSamples.h"

// =============================================================================
//  Instruments de pre-ecoute : tes one-shots (keys, bells, plucks, pads...)
//  integres au plugin, rejoues a toutes les hauteurs.
//  Fichiers : GROUPE__Nom__racine.wav  (racine = note MIDI du sample)
// =============================================================================
class InstrumentLibrary
{
public:
    struct Sample
    {
        juce::String group, name;
        juce::AudioBuffer<float> data;
        double sampleRate = 44100.0;
        int rootNote = 60;
    };

    static InstrumentLibrary& get()
    {
        static InstrumentLibrary lib;
        return lib;
    }

    int size() const { return (int) samples.size(); }
    const Sample* at (int i) const { return i >= 0 && i < size() ? &samples[(size_t) i] : nullptr; }

    // liste du menu : [0] = piano de synthese, puis "Groupe - Nom"
    static juce::StringArray menuNames()
    {
        juce::StringArray out { "Piano (synthe)" };
        for (auto& s : get().samples) out.add (s.group + " - " + s.name);
        return out;
    }

private:
    std::vector<Sample> samples;

    InstrumentLibrary()
    {
        juce::WavAudioFormat wav;
        for (int i = 0; i < InstrumentSamples::namedResourceListSize; ++i)
        {
            const juce::String file = juce::String (InstrumentSamples::originalFilenames[i]).upToLastOccurrenceOf (".", false, false);
            if (! file.contains ("__")) continue;
            int size = 0;
            const char* data = InstrumentSamples::getNamedResource (InstrumentSamples::namedResourceList[i], size);
            if (data == nullptr || size <= 0) continue;
            std::unique_ptr<juce::AudioFormatReader> reader (
                wav.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
            if (reader == nullptr) continue;
            Sample s;
            s.group = file.upToFirstOccurrenceOf ("__", false, false);
            const auto rest = file.fromFirstOccurrenceOf ("__", false, false);
            s.name = rest.upToLastOccurrenceOf ("__", false, false).replaceCharacter ('_', ' ');
            s.rootNote = juce::jlimit (12, 120, rest.fromLastOccurrenceOf ("__", false, false).getIntValue());
            s.sampleRate = reader->sampleRate;
            const int nch = reader->numChannels > 1 ? 2 : 1;
            s.data.setSize (nch, (int) reader->lengthInSamples);
            reader->read (&s.data, 0, (int) reader->lengthInSamples, 0, true, nch > 1);
            samples.push_back (std::move (s));
        }
        std::sort (samples.begin(), samples.end(), [] (const Sample& a, const Sample& b)
                   { return a.group != b.group ? a.group < b.group : a.name < b.name; });
    }
};

// Lecteur polyphonique des one-shots : stereo, interpolation Hermite (plus propre
// que lineaire quand on transpose), une sortie stereo par couche (1 melodie, 2 accords, 3 couche 2)
class InstrumentPlayer
{
public:
    void prepare (double sampleRate) { sr = sampleRate; for (auto& v : voices) v.active = false; }
    void setRelease (float sec) { release = juce::jmax (0.02f, sec); }
    void setLayerGains (const float* g) { for (int i = 0; i < 4; ++i) layerGain[i] = g[i]; }

    void noteOn (int ch, int note, float vel, int sampleIndex, float level)
    {
        const auto* s = InstrumentLibrary::get().at (sampleIndex);
        if (s == nullptr) return;
        // decoupe : une nouvelle note coupe les notes deja en cours sur la meme couche
        // (les notes jouees ensemble, a moins de 25 ms, restent : accords, doubles notes)
        const juce::int64 window = (juce::int64) (0.025 * sr);
        for (auto& v : voices)
            if (v.active && v.ch == ch && (v.note == note || now - v.start > window))
                startRelease (v, ch == 2 ? 0.12f : 0.06f);
        Voice* v = nullptr;
        for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (v == nullptr)
        {
            v = &voices[0];
            for (auto& x : voices) if (x.age < v->age) v = &x;
        }
        *v = Voice();
        v->active = true; v->held = true; v->s = s; v->ch = ch; v->note = note;
        v->inc = (s->sampleRate / sr) * std::pow (2.0, (note - s->rootNote) / 12.0);
        v->gain = level * (0.3f + 0.7f * vel);
        v->attMul = (float) (1.0 / (0.003 * sr));
        v->age = ++counter;
        v->start = now;
    }

    void noteOff (int ch, int note)
    {
        for (auto& v : voices)
            if (v.active && v.held && v.ch == ch && v.note == note) startRelease (v, release);
    }

    void allOff() { for (auto& v : voices) if (v.active) startRelease (v, 0.05f); }

    // L[c] / R[c] : buffers de la couche c (1..3)
    void renderRange (float* const* L, float* const* R, int start, int end)
    {
        if (end > start) now += end - start;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            const int c = juce::jlimit (1, 3, v.ch);
            float* outL = L[c];
            float* outR = R[c];
            const float* dL = v.s->data.getReadPointer (0);
            const float* dR = v.s->data.getReadPointer (v.s->data.getNumChannels() > 1 ? 1 : 0);
            const int len = v.s->data.getNumSamples();
            const float lg = layerGain[c];
            for (int i = start; i < end; ++i)
            {
                const int i0 = (int) v.pos;
                if (i0 + 2 >= len) { v.active = false; break; }
                const float t = (float) (v.pos - i0);
                const int im = i0 > 0 ? i0 - 1 : 0;
                float xl = hermite (dL[im], dL[i0], dL[i0 + 1], dL[i0 + 2], t);
                float xr = hermite (dR[im], dR[i0], dR[i0 + 1], dR[i0 + 2], t);
                if (v.att < 1.0f) v.att = juce::jmin (1.0f, v.att + v.attMul);
                float g = v.gain * v.att * lg;
                if (! v.held) { v.rel *= v.relMul; g *= v.rel; if (v.rel < 1.0e-4f) { v.active = false; break; } }
                outL[i] += xl * g;
                outR[i] += xr * g;
                v.pos += v.inc;
            }
        }
    }

private:
    struct Voice
    {
        bool active = false, held = false;
        const InstrumentLibrary::Sample* s = nullptr;
        double pos = 0, inc = 1;
        float gain = 1, att = 0, attMul = 0.01f, rel = 1, relMul = 1;
        int ch = 0, note = 0;
        juce::uint32 age = 0;
        juce::int64 start = 0;
    };

    static float hermite (float xm1, float x0, float x1, float x2, float t)
    {
        const float c = (x1 - xm1) * 0.5f;
        const float v = x0 - x1;
        const float w = c + v;
        const float a = w + v + (x2 - x0) * 0.5f;
        const float b = w + a;
        return ((((a * t) - b) * t + c) * t + x0);
    }

    void startRelease (Voice& v, float sec)
    {
        v.held = false;
        v.relMul = (float) std::exp (-1.0 / (sec * sr));
    }

    double sr = 44100.0;
    float release = 0.3f;
    float layerGain[4] = { 1, 1, 1, 1 };
    juce::uint32 counter = 0;
    std::array<Voice, 64> voices;
    juce::int64 now = 0;
};
