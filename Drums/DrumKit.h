#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <cmath>
#include "DrumSamples.h"

// =============================================================================
//  Bibliotheque de sons (tes samples, integres au plugin) + sampler de pre-ecoute
//  Canaux : 1 kick, 2 snare, 3 hats, 4 808, 5 open hat, 6 relances
// =============================================================================
class SampleLibrary
{
public:
    enum Cat { KICK = 0, SNARE, HAT, OPENHAT, BASS, RIM, NUM_CATS };

    struct Sample
    {
        juce::String name;
        juce::AudioBuffer<float> data;
        double sampleRate = 44100.0;
        int rootNote = 60; // note qui joue le sample a sa hauteur d'origine
    };

    static SampleLibrary& get()
    {
        static SampleLibrary lib;
        return lib;
    }

    const std::vector<Sample>& list (int cat) const { return samples[(size_t) cat]; }

    static juce::StringArray namesFor (int cat)
    {
        juce::StringArray out;
        for (auto& s : get().list (cat)) out.add (s.name);
        if (out.isEmpty()) out.add ("-");
        return out;
    }

private:
    std::array<std::vector<Sample>, NUM_CATS> samples;

    static int catOf (const juce::String& f)
    {
        if (f.startsWith ("KICK_"))     return KICK;
        if (f.startsWith ("SNARE_"))    return SNARE;
        if (f.startsWith ("HI_HAT_"))   return HAT;
        if (f.startsWith ("OPEN_HAT_")) return OPENHAT;
        if (f.startsWith ("808_"))      return BASS;
        if (f.startsWith ("RIM_"))      return RIM;
        return -1;
    }

    // hauteur detectee de tes 808 (analyse : quasi toutes en Do, MOJITO en Sol#)
    static int bassRoot (const juce::String& n)
    {
        if (n == "MOJITO") return 56;   // G#
        return 60;                      // C
    }

    SampleLibrary()
    {
        juce::WavAudioFormat wav;
        for (int i = 0; i < DrumSamples::namedResourceListSize; ++i)
        {
            const juce::String file (DrumSamples::originalFilenames[i]);
            const int cat = catOf (file);
            if (cat < 0) continue;
            int size = 0;
            const char* data = DrumSamples::getNamedResource (DrumSamples::namedResourceList[i], size);
            if (data == nullptr || size <= 0) continue;
            std::unique_ptr<juce::AudioFormatReader> reader (
                wav.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
            if (reader == nullptr) continue;
            Sample s;
            s.name = file.fromFirstOccurrenceOf ("_", false, false).upToLastOccurrenceOf (".", false, false);
            if (cat == HAT || cat == OPENHAT) s.name = s.name.fromFirstOccurrenceOf ("HAT_", false, false);
            s.sampleRate = reader->sampleRate;
            s.data.setSize (1, (int) reader->lengthInSamples);
            reader->read (&s.data, 0, (int) reader->lengthInSamples, 0, true, false);
            if (cat == BASS) s.rootNote = bassRoot (s.name);
            samples[(size_t) cat].push_back (std::move (s));
        }
        for (auto& v : samples)
            std::sort (v.begin(), v.end(), [] (const Sample& a, const Sample& b) { return a.name < b.name; });
    }
};

class DrumKit
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& v : voices) v.active = false;
    }

    // index du son choisi pour chaque categorie ; fillCat : RIM ou SNARE
    void setParams (float gain, int kick, int snare, int hat, int open, int bass, int fill)
    {
        outGain = gain;
        sel[0] = kick; sel[1] = snare; sel[2] = hat; sel[3] = open; sel[4] = bass; sel[5] = fill;
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
        for (int c = 0; c < buf.getNumChannels(); ++c)
        {
            auto* d = buf.getWritePointer (c);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                d[i] = std::tanh (d[i] * outGain * 1.1f) * 0.9f;
        }
    }

private:
    struct Voice
    {
        bool active = false, releasing = false;
        const SampleLibrary::Sample* s = nullptr;
        double pos = 0, inc = 1, targetInc = 1;
        float gain = 1, rel = 1, relMul = 1;
        int ch = 0, note = 0;
        juce::uint32 age = 0;
    };

    double sr = 44100.0;
    std::array<Voice, 24> voices;
    float outGain = 0.7f;
    int sel[6] = { 0, 0, 0, 0, 0, 0 };
    juce::uint32 counter = 0;

    const SampleLibrary::Sample* pick (int cat, int idx) const
    {
        const auto& l = SampleLibrary::get().list (cat);
        if (l.empty()) return nullptr;
        return &l[(size_t) juce::jlimit (0, (int) l.size() - 1, idx)];
    }

    Voice& freeVoice()
    {
        for (auto& v : voices) if (! v.active) return v;
        Voice* o = &voices[0];
        for (auto& v : voices) if (v.age < o->age) o = &v;
        return *o;
    }

    void release (Voice& v, double seconds)
    {
        v.releasing = true;
        v.relMul = (float) std::exp (-1.0 / (seconds * sr));
    }

    void handle (const juce::MidiMessage& m)
    {
        const int ch = m.getChannel();
        if (m.isNoteOff())
        {
            if (ch == 4)
                for (auto& v : voices)
                    if (v.active && v.ch == 4 && ! v.releasing && v.note == m.getNoteNumber()) release (v, 0.06);
            return;
        }
        if (! m.isNoteOn()) return;

        const int note = m.getNoteNumber();
        const float vel = m.getFloatVelocity();
        const SampleLibrary::Sample* s = nullptr;
        float level = 1.0f;
        switch (ch)
        {
            case 1: s = pick (SampleLibrary::KICK, sel[0]);    level = 0.95f; break;
            case 2: s = pick (SampleLibrary::SNARE, sel[1]);   level = 0.8f;  break;
            case 3: s = pick (SampleLibrary::HAT, sel[2]);     level = 0.6f;  break;
            case 4: s = pick (SampleLibrary::BASS, sel[4]);    level = 0.85f; break;
            case 5: s = pick (SampleLibrary::OPENHAT, sel[3]); level = 0.55f; break;
            case 6: s = sel[5] == 0 ? pick (SampleLibrary::SNARE, sel[1]) : pick (SampleLibrary::RIM, sel[5] - 1); level = 0.65f; break;
            default: return;
        }
        if (s == nullptr) return;

        const double baseInc = s->sampleRate / sr;
        const double ratio = std::pow (2.0, (note - (ch == 4 ? s->rootNote : 60)) / 12.0);

        // hats : le nouveau coup coupe le precedent (choke), open hat compris
        if (ch == 3 || ch == 5)
            for (auto& v : voices)
                if (v.active && (v.ch == 3 || v.ch == 5) && ! v.releasing) release (v, 0.01);

        // 808 monophonique : si une note sonne encore -> glide, sinon nouvelle note
        if (ch == 4)
        {
            for (auto& v : voices)
                if (v.active && v.ch == 4 && ! v.releasing)
                {
                    v.targetInc = baseInc * ratio;
                    v.note = note;
                    v.gain = std::max (v.gain, level * (0.4f + 0.6f * vel));
                    return;
                }
        }

        Voice& v = freeVoice();
        v = Voice();
        v.active = true; v.s = s; v.ch = ch; v.note = note;
        v.inc = v.targetInc = baseInc * ratio;
        v.gain = level * (0.3f + 0.7f * vel);
        v.age = ++counter;
    }

    void renderRange (juce::AudioBuffer<float>& buf, int start, int end)
    {
        if (end <= start) return;
        auto* L = buf.getWritePointer (0);
        auto* R = buf.getNumChannels() > 1 ? buf.getWritePointer (1) : nullptr;
        for (auto& v : voices)
        {
            if (! v.active || v.s == nullptr) continue;
            const float* d = v.s->data.getReadPointer (0);
            const int len = v.s->data.getNumSamples();
            for (int i = start; i < end; ++i)
            {
                const int i0 = (int) v.pos;
                if (i0 + 1 >= len) { v.active = false; break; }
                const float fr = (float) (v.pos - i0);
                float x = (d[i0] + (d[i0 + 1] - d[i0]) * fr) * v.gain;
                if (v.releasing) { v.rel *= v.relMul; x *= v.rel; if (v.rel < 1.0e-4f) { v.active = false; break; } }
                L[i] += x;
                if (R != nullptr) R[i] += x;
                v.inc += (v.targetInc - v.inc) * 0.0015;  // glide 808
                v.pos += v.inc;
            }
        }
    }
};
