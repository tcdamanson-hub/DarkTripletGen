#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <cmath>
#include "DrumSamples.h"

// =============================================================================
//  Bibliotheque de sons (tes samples, integres au plugin) + sampler de pre-ecoute
//  Canaux : 1 kick, 2 snare (+ clap), 3 hats, 4 808, 5 open hat, 6 relances,
//           7 percs, 8 FX (60 = impact, 62 = riser)
// =============================================================================
class SampleLibrary
{
public:
    enum Cat { KICK = 0, SNARE, HAT, OPENHAT, BASS, RIM, CLAP, PERC, IMPACT, RISER, NUM_CATS };

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

    // fichiers : CATEGORIE__Nom__racine.wav
    static int catOf (const juce::String& c)
    {
        static const char* names[] = { "KICK", "SNARE", "HAT", "OPENHAT", "BASS", "RIM", "CLAP", "PERC", "IMPACT", "RISER" };
        for (int i = 0; i < NUM_CATS; ++i) if (c == names[i]) return i;
        return -1;
    }

    SampleLibrary()
    {
        juce::WavAudioFormat wav;
        for (int i = 0; i < DrumSamples::namedResourceListSize; ++i)
        {
            const juce::String file = juce::String (DrumSamples::originalFilenames[i]).upToLastOccurrenceOf (".", false, false);
            const int cat = catOf (file.upToFirstOccurrenceOf ("__", false, false));
            if (cat < 0) continue;
            const juce::String rest = file.fromFirstOccurrenceOf ("__", false, false);
            const juce::String nm = rest.upToLastOccurrenceOf ("__", false, false).replaceCharacter ('_', ' ');
            const int root = rest.fromLastOccurrenceOf ("__", false, false).getIntValue();
            int size = 0;
            const char* data = DrumSamples::getNamedResource (DrumSamples::namedResourceList[i], size);
            if (data == nullptr || size <= 0) continue;
            std::unique_ptr<juce::AudioFormatReader> reader (
                wav.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
            if (reader == nullptr) continue;
            Sample s;
            s.name = nm.isNotEmpty() ? nm : file;
            s.sampleRate = reader->sampleRate;
            s.data.setSize (1, (int) reader->lengthInSamples);
            reader->read (&s.data, 0, (int) reader->lengthInSamples, 0, true, false);
            s.rootNote = root > 0 ? root : 60;
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

    // index du son choisi pour chaque categorie
    // fill : 0 = meme snare, puis rims, puis claps ; clap : 0 = aucun (couche sur le snare)
    void setParams (float gain, int kick, int snare, int hat, int open, int bass, int fill,
                    int clap, int perc, int impact, int riser)
    {
        outGain = gain;
        sel[0] = kick; sel[1] = snare; sel[2] = hat; sel[3] = open; sel[4] = bass; sel[5] = fill;
        sel[6] = clap; sel[7] = perc; sel[8] = impact; sel[9] = riser;
    }

    // volume par piste (index = canal 1..8), 0 = coupe
    void setLaneGain (int ch, float g) { if (ch >= 1 && ch <= 8) laneGain[ch] = juce::jmax (0.0f, g); }

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
    std::array<Voice, 32> voices;
    float outGain = 0.7f;
    int sel[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    float laneGain[9] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
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
            case 1: s = pick (SampleLibrary::KICK, sel[0]);    level = 0.85f; break;
            case 2: s = pick (SampleLibrary::SNARE, sel[1]);   level = 0.8f;  break;
            case 3: s = pick (SampleLibrary::HAT, sel[2]);     level = 0.6f;  break;
            case 4: s = pick (SampleLibrary::BASS, sel[4]);    level = 0.68f; break;
            case 5: s = pick (SampleLibrary::OPENHAT, sel[3]); level = 0.55f; break;
            case 6:
            {
                const int nr = (int) SampleLibrary::get().list (SampleLibrary::RIM).size();
                if (sel[5] == 0)       s = pick (SampleLibrary::SNARE, sel[1]);
                else if (sel[5] <= nr) s = pick (SampleLibrary::RIM, sel[5] - 1);
                else                   s = pick (SampleLibrary::CLAP, sel[5] - 1 - nr);
                level = 0.65f; break;
            }
            case 7: s = pick (SampleLibrary::PERC, sel[7]); level = 0.55f; break;
            case 8: s = note >= 62 ? pick (SampleLibrary::RISER, sel[9]) : pick (SampleLibrary::IMPACT, sel[8]); level = 0.5f; break;
            default: return;
        }
        if (s == nullptr) return;

        const double baseInc = s->sampleRate / sr;
        const double ratio = ch == 8 ? 1.0 : std::pow (2.0, (note - (ch == 4 ? s->rootNote : 60)) / 12.0);

        if (ch == 2 && sel[6] > 0)
            if (auto* c = pick (SampleLibrary::CLAP, sel[6] - 1))
            {
                Voice& cv = freeVoice();
                cv = Voice();
                cv.active = true; cv.s = c; cv.ch = 2; cv.note = note;
                cv.inc = cv.targetInc = (c->sampleRate / sr) * ratio;
                cv.gain = 0.6f * (0.3f + 0.7f * vel);
                cv.age = ++counter;
            }

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
                float x = (d[i0] + (d[i0 + 1] - d[i0]) * fr) * v.gain * laneGain[v.ch];
                if (v.releasing) { v.rel *= v.relMul; x *= v.rel; if (v.rel < 1.0e-4f) { v.active = false; break; } }
                L[i] += x;
                if (R != nullptr) R[i] += x;
                v.inc += (v.targetInc - v.inc) * 0.0015;  // glide 808
                v.pos += v.inc;
            }
        }
    }
};
