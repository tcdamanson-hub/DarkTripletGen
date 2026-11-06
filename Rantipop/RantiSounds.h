#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <cmath>
#include <map>
#include <atomic>
#include <memory>
#include "RantiSamples.h"

// =============================================================================
//  RANTIPOP : sons integres au plugin
//   Inst/  GROUPE__Nom__racine.wav  (plusieurs racines = multi-samples, ex. Rhodes)
//   Drums/ GENRE_CAT__Nom.wav       (RB_KICK, AF_SHAKER, ...)
// =============================================================================
namespace ranti
{
struct Sample
{
    juce::AudioBuffer<float> data;
    double sampleRate = 44100.0;
    int root = 60;
    juce::String name;
    // decode en arriere-plan au demarrage : FL n'attend plus le chargement des sons
    const char* src = nullptr;
    int srcSize = 0;
    std::shared_ptr<std::atomic<bool>> ready = std::make_shared<std::atomic<bool>> (false);
    bool isReady() const { return ready->load (std::memory_order_acquire); }
};

struct Instrument
{
    juce::String group, name;
    std::vector<Sample> zones; // triees par racine

    const Sample* zoneFor (int note) const
    {
        const Sample* best = nullptr;
        int bestD = 1000;
        for (auto& z : zones)
        {
            const int d = std::abs (z.root - note);
            if (d < bestD) { bestD = d; best = &z; }
        }
        return best;
    }
};

class SoundBank
{
public:
    static SoundBank& get()
    {
        static SoundBank b;
        return b;
    }

    // menu melodique : [0] = piano de synthese, puis les instruments (hors basses)
    const juce::StringArray& melodicMenu() const { return melNames; }
    const juce::StringArray& bassMenu() const    { return bassNames; }
    const Instrument* melodic (int menuIndex) const
    {
        return menuIndex >= 1 && menuIndex - 1 < (int) melIdx.size() ? &instruments[(size_t) melIdx[(size_t) menuIndex - 1]] : nullptr;
    }
    const Instrument* bass (int menuIndex) const
    {
        return menuIndex >= 0 && menuIndex < (int) bassIdx.size() ? &instruments[(size_t) bassIdx[(size_t) menuIndex]] : nullptr;
    }
    int findMelodic (const juce::String& n) const { return juce::jmax (0, melNames.indexOf (n)); }
    int findBass (const juce::String& n) const    { return juce::jmax (0, bassNames.indexOf (n)); }

    const std::vector<Sample>* drums (const juce::String& cat) const
    {
        auto it = drumCats.find (cat);
        return it == drumCats.end() ? nullptr : &it->second;
    }

private:
    std::vector<Instrument> instruments;
    std::vector<int> melIdx, bassIdx;
    juce::StringArray melNames, bassNames;
    std::map<juce::String, std::vector<Sample>> drumCats;

    static int groupOrder (const juce::String& g)
    {
        static const juce::StringArray order { "Piano", "Rhodes", "Keys", "Guitares", "Mallets", "Plucks", "Pads", "Bells", "Leads", "Synths", "Cuivres", "Bass" };
        const int i = order.indexOf (g);
        return i < 0 ? 50 : i;
    }

    SoundBank()
    {
        std::map<juce::String, size_t> instIndex;
        for (int i = 0; i < RantiSamples::namedResourceListSize; ++i)
        {
            const juce::String file = juce::String (RantiSamples::originalFilenames[i]).upToLastOccurrenceOf (".", false, false);
            const juce::String fn (RantiSamples::originalFilenames[i]);
            if (! (fn.endsWithIgnoreCase (".wav") || fn.endsWithIgnoreCase (".ogg") || fn.endsWithIgnoreCase (".flac")) || ! file.contains ("__"))
                continue;
            int size = 0;
            const char* data = RantiSamples::getNamedResource (RantiSamples::namedResourceList[i], size);
            if (data == nullptr || size <= 0) continue;
            Sample s;
            s.src = data; s.srcSize = size;

            const auto head = file.upToFirstOccurrenceOf ("__", false, false);
            const auto rest = file.fromFirstOccurrenceOf ("__", false, false);
            if (head.contains ("_")) // tambour : GENRE_CAT__Nom
            {
                s.name = rest;
                drumCats[head].push_back (std::move (s));
                continue;
            }
            s.root = juce::jlimit (0, 127, rest.fromLastOccurrenceOf ("__", false, false).getIntValue());
            const auto name = rest.upToLastOccurrenceOf ("__", false, false);
            const auto key = head + "|" + name;
            auto it = instIndex.find (key);
            if (it == instIndex.end())
            {
                Instrument in;
                in.group = head; in.name = name;
                instruments.push_back (std::move (in));
                it = instIndex.emplace (key, instruments.size() - 1).first;
            }
            instruments[it->second].zones.push_back (std::move (s));
        }
        for (auto& c : drumCats)
            std::sort (c.second.begin(), c.second.end(), [] (const Sample& a, const Sample& b) { return a.name < b.name; });

        std::sort (instruments.begin(), instruments.end(), [] (const Instrument& a, const Instrument& b)
        {
            const int ga = groupOrder (a.group), gb = groupOrder (b.group);
            return ga != gb ? ga < gb : a.name < b.name;
        });
        melNames.add ("Piano (synthe)");
        for (size_t i = 0; i < instruments.size(); ++i)
        {
            auto& in = instruments[i];
            std::sort (in.zones.begin(), in.zones.end(), [] (const Sample& a, const Sample& b) { return a.root < b.root; });
            if (in.group == "Bass") { bassIdx.push_back ((int) i); bassNames.add (in.name); }
            else                    { melIdx.push_back ((int) i);  melNames.add (in.name); }
        }
        if (bassNames.isEmpty()) bassNames.add ("-");
        // ordre de chargement : instruments puis tambours ; tout est pret en une ou deux secondes
        for (auto& in : instruments) for (auto& z : in.zones) all.push_back (&z);
        for (auto& c : drumCats) for (auto& z : c.second) all.push_back (&z);
        loader = std::make_unique<Loader> (*this);
        loader->startThread (juce::Thread::Priority::low);
    }

    struct Loader : juce::Thread
    {
        SoundBank& bank;
        explicit Loader (SoundBank& b) : juce::Thread ("RantipopSons"), bank (b) {}
        void run() override
        {
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            for (auto* s : bank.all)
            {
                if (threadShouldExit()) return;
                std::unique_ptr<juce::AudioFormatReader> reader (
                    formats.createReaderFor (std::make_unique<juce::MemoryInputStream> (s->src, (size_t) s->srcSize, false)));
                if (reader != nullptr)
                {
                    const int nch = reader->numChannels > 1 ? 2 : 1;
                    s->data.setSize (nch, (int) reader->lengthInSamples);
                    reader->read (&s->data, 0, (int) reader->lengthInSamples, 0, true, nch > 1);
                    s->sampleRate = reader->sampleRate;
                }
                s->ready->store (true, std::memory_order_release);
            }
        }
    };
    std::vector<Sample*> all;
    std::unique_ptr<Loader> loader;
public:
    ~SoundBank() { if (loader != nullptr) loader->stopThread (4000); }
private:
};

// -----------------------------------------------------------------------------
//  Lecteur de samples polyphonique : transpose (Hermite), une sortie stereo par bus
// -----------------------------------------------------------------------------
class Sampler
{
public:
    static constexpr int numBuses = 5; // 0 melodie, 1 accords, 2 basse, 3 contre-melo, 4 drums

    void prepare (double sampleRate) { sr = sampleRate; for (auto& v : voices) v.active = false; }

    // pitched = transpose le sample vers note ; sinon lecture a hauteur d'origine (drums)
    void noteOn (int bus, int note, float vel, const Sample* s, bool pitched, float release, int chokeGroup = -1, float level = 1.0f)
    {
        if (s == nullptr || ! s->isReady()) return;
        if (chokeGroup >= 0)
            for (auto& v : voices)
                if (v.active && v.choke == chokeGroup) startRelease (v, 0.03f);
        if (pitched)
            for (auto& v : voices)
                if (v.active && v.bus == bus && v.note == note) startRelease (v, 0.03f);
        Voice* v = nullptr;
        for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (v == nullptr)
        {
            v = &voices[0];
            for (auto& x : voices) if (x.age < v->age) v = &x;
        }
        *v = Voice();
        v->active = true; v->held = true; v->s = s; v->bus = bus; v->note = note; v->choke = chokeGroup;
        v->inc = (s->sampleRate / sr) * (pitched ? std::pow (2.0, (note - s->root) / 12.0) : 1.0);
        v->gain = (0.15f + 0.85f * vel * vel) * level;
        v->attMul = (float) (1.0 / ((pitched ? 0.003 : 0.0005) * sr));
        v->release = release;
        v->age = ++counter;
    }

    void noteOff (int bus, int note)
    {
        for (auto& v : voices)
            if (v.active && v.held && v.bus == bus && v.note == note) startRelease (v, v.release);
    }

    void allOff() { for (auto& v : voices) if (v.active) startRelease (v, 0.04f); }

    void render (juce::AudioBuffer<float>& buses, int start, int end)
    {
        for (auto& v : voices)
        {
            if (! v.active) continue;
            float* outL = buses.getWritePointer (v.bus * 2);
            float* outR = buses.getWritePointer (v.bus * 2 + 1);
            const float* dL = v.s->data.getReadPointer (0);
            const float* dR = v.s->data.getReadPointer (v.s->data.getNumChannels() > 1 ? 1 : 0);
            const int len = v.s->data.getNumSamples();
            for (int i = start; i < end; ++i)
            {
                const int i0 = (int) v.pos;
                if (i0 + 2 >= len) { v.active = false; break; }
                const float t = (float) (v.pos - i0);
                const int im = i0 > 0 ? i0 - 1 : 0;
                const float xl = hermite (dL[im], dL[i0], dL[i0 + 1], dL[i0 + 2], t);
                const float xr = hermite (dR[im], dR[i0], dR[i0 + 1], dR[i0 + 2], t);
                if (v.att < 1.0f) v.att = juce::jmin (1.0f, v.att + v.attMul);
                float g = v.gain * v.att;
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
        const Sample* s = nullptr;
        double pos = 0, inc = 1;
        float gain = 1, att = 0, attMul = 0.01f, rel = 1, relMul = 1, release = 0.3f;
        int bus = 0, note = 0, choke = -1;
        juce::uint32 age = 0;
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
        v.relMul = (float) std::exp (-1.0 / (juce::jmax (0.01f, sec) * sr));
    }

    double sr = 44100.0;
    juce::uint32 counter = 0;
    std::array<Voice, 96> voices;
};
} // namespace ranti
