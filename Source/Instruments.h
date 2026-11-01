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
            s.data.setSize (1, (int) reader->lengthInSamples);
            reader->read (&s.data, 0, (int) reader->lengthInSamples, 0, true, false);
            samples.push_back (std::move (s));
        }
        std::sort (samples.begin(), samples.end(), [] (const Sample& a, const Sample& b)
                   { return a.group != b.group ? a.group < b.group : a.name < b.name; });
    }
};

// Lecteur polyphonique des one-shots (mono -> centre, enveloppe attaque/relache)
class InstrumentPlayer
{
public:
    void prepare (double sampleRate) { sr = sampleRate; for (auto& v : voices) v.active = false; }
    void setRelease (float sec) { release = juce::jmax (0.02f, sec); }

    void noteOn (int ch, int note, float vel, int sampleIndex, float level)
    {
        const auto* s = InstrumentLibrary::get().at (sampleIndex);
        if (s == nullptr) return;
        // meme note sur le meme canal : on relache l'ancienne
        for (auto& v : voices)
            if (v.active && v.ch == ch && v.note == note && v.held) startRelease (v, 0.03f);
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
        v->gain = level * (0.25f + 0.75f * vel);
        v->attMul = (float) (1.0 / (0.004 * sr));
        v->age = ++counter;
    }

    void noteOff (int ch, int note)
    {
        for (auto& v : voices)
            if (v.active && v.held && v.ch == ch && v.note == note) startRelease (v, release);
    }

    void allOff() { for (auto& v : voices) if (v.active) startRelease (v, 0.05f); }

    void renderRange (float* L, float* R, int start, int end)
    {
        for (auto& v : voices)
        {
            if (! v.active) continue;
            const float* d = v.s->data.getReadPointer (0);
            const int len = v.s->data.getNumSamples();
            for (int i = start; i < end; ++i)
            {
                const int i0 = (int) v.pos;
                if (i0 + 1 >= len) { v.active = false; break; }
                const float fr = (float) (v.pos - i0);
                float x = d[i0] + (d[i0 + 1] - d[i0]) * fr;
                if (v.att < 1.0f) v.att = juce::jmin (1.0f, v.att + v.attMul);
                x *= v.gain * v.att;
                if (! v.held) { v.rel *= v.relMul; x *= v.rel; if (v.rel < 1.0e-4f) { v.active = false; break; } }
                L[i] += x;
                if (R != nullptr) R[i] += x;
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
    };

    void startRelease (Voice& v, float sec)
    {
        v.held = false;
        v.relMul = (float) std::exp (-1.0 / (sec * sr));
    }

    double sr = 44100.0;
    float release = 0.3f;
    juce::uint32 counter = 0;
    std::array<Voice, 48> voices;
};
