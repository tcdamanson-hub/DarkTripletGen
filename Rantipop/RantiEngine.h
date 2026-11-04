#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <vector>
#include "RantiData.h"
#include "RantiSounds.h"
#include "RantiPiano.h"

// =============================================================================
//  RANTIPOP MELODIE : moteur
//  Tes MIDI sont joues TELS QUELS (timing et notes d'origine), juste transposes.
//  Parties : 0 melodie, 1 accords, 2 basse/808, 3 contre-melo, 4 drums
// =============================================================================
namespace ranti
{
enum Part { PMel = 0, PChords = 1, PBass = 2, PExtra = 3, PDrums = 4, NumParts = 5 };
enum Lane { LKick = 0, LSnare, LClap, LHat, LHat2, LOpen, LRim, LPercA, LPercB, LPercC, LCrash, NumLanes };

static const char* const genreNames[] = { "R&B", "AFRO", "POP", "SYNTHWAVE" };
static constexpr int numGenres = 4;
static const char* const keyNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// categorie de samples par genre et par piste de drums
inline juce::String drumCategory (int genre, int lane, int variant)
{
    static const char* rb[NumLanes] = { "RB_KICK", "RB_SNARE", "RB_CLAP", "RB_HAT", "RB_HAT", "AF_OHAT", "RB_RIM", "RB_PERC", "RB_PERC", "RB_SNAP", "-" };
    static const char* af[NumLanes] = { "AF_KICK", "AF_SNARE", "AF_CLAP", "AF_HAT", "AF_SHAKER", "AF_OHAT", "AF_RIM", "AF_HAND", "AF_DRUM", "AF_WOOD", "-" };
    static const char* sw[NumLanes] = { "SW_KICK", "SW_SNARE", "SW_CLAP", "SW_HAT", "SW_HAT", "SW_OHAT", "SW_RIM", "SW_PERC", "SW_PERC", "SW_PERC", "SW_CRASH" };
    if (genre == 3)
    {
        // kit synthwave ; si une categorie manque, on prend celle du R&B
        if (SoundBank::get().drums (sw[lane]) != nullptr) return sw[lane];
        return rb[lane];
    }
    if (genre == 1)
    {
        if (lane == LClap && variant == 1) return "AF_SNAP";
        return af[lane];
    }
    return rb[lane];
}

inline bool genreAvailable (int g)
{
    for (int i = 0; i < numSongs; ++i) if (songs[i].genre == g) return true;
    return false;
}

// tout ce qui decrit une generation (sauvegarde dans le projet, historique)
struct GenState
{
    int genre = 0, song = -1, groove = -1, transpose = 0;
    std::array<int, 4> stem { 0, 0, 0, 0 };
    std::array<int, NumLanes> kit {};
    int clapVariant = 0;

    juce::String toString() const
    {
        juce::StringArray a;
        a.add (juce::String (genre)); a.add (juce::String (song)); a.add (juce::String (groove)); a.add (juce::String (transpose));
        for (auto s : stem) a.add (juce::String (s));
        for (auto k : kit) a.add (juce::String (k));
        a.add (juce::String (clapVariant));
        return a.joinIntoString (",");
    }
    static GenState fromString (const juce::String& s)
    {
        GenState g;
        auto a = juce::StringArray::fromTokens (s, ",", "");
        if (a.size() < 4 + 4 + NumLanes + 1) return g;
        int i = 0;
        g.genre = a[i++].getIntValue(); g.song = a[i++].getIntValue(); g.groove = a[i++].getIntValue(); g.transpose = a[i++].getIntValue();
        for (auto& x : g.stem) x = a[i++].getIntValue();
        for (auto& x : g.kit) x = a[i++].getIntValue();
        g.clapVariant = a[i++].getIntValue();
        return g;
    }
};

struct PNote { double start, length; int pitch, vel; };

struct Pattern
{
    double length = 32.0;
    std::array<std::vector<PNote>, NumParts> parts; // drums : pitch = piste (Lane)
    juce::String songName, grooveName;
    int bpm = 120, key = 0, minor = 1;
    bool hasBassLine = false;
};

class Generator
{
public:
    // nouvelle generation : morceau au hasard du genre, groove au hasard, kit au hasard
    static GenState randomState (int genre, int keyChoice, int previousSong, juce::Random& rng)
    {
        GenState g;
        g.genre = genre;
        std::vector<int> cand;
        for (int i = 0; i < numSongs; ++i) if (songs[i].genre == genre && i != previousSong) cand.push_back (i);
        if (cand.empty()) for (int i = 0; i < numSongs; ++i) if (songs[i].genre == genre) cand.push_back (i);
        if (cand.empty()) return g;
        g.song = cand[(size_t) rng.nextInt ((int) cand.size())];
        g.groove = randomGroove (genre, rng);
        const auto& s = songs[g.song];
        for (int r = 0; r < 4; ++r)
        {
            const int n = stemCount (g.song, r);
            g.stem[(size_t) r] = n > 0 ? rng.nextInt (n) : 0;
        }
        randomKit (g, rng);
        g.transpose = transposeFor (s.key, keyChoice);
        return g;
    }

    static int randomGroove (int genre, juce::Random& rng)
    {
        std::vector<int> gc;
        for (int i = 0; i < numGrooves; ++i) if (grooves[i].genre == genre) gc.push_back (i);
        if (gc.empty()) for (int i = 0; i < numGrooves; ++i) if (grooves[i].genre == 0) gc.push_back (i);
        return gc.empty() ? -1 : gc[(size_t) rng.nextInt ((int) gc.size())];
    }

    static void randomKit (GenState& g, juce::Random& rng)
    {
        g.clapVariant = rng.nextInt (2);
        for (int l = 0; l < NumLanes; ++l)
        {
            const auto* set = SoundBank::get().drums (drumCategory (g.genre, l, g.clapVariant));
            g.kit[(size_t) l] = (set != nullptr && ! set->empty()) ? rng.nextInt ((int) set->size()) : 0;
        }
    }

    static int transposeFor (int songKey, int keyChoice) // keyChoice : 0 = auto, 1..12 = C..B
    {
        if (keyChoice <= 0) return 0;
        int t = ((keyChoice - 1) - songKey) % 12;
        if (t < 0) t += 12;
        if (t > 6) t -= 12;
        return t;
    }

    static int stemCount (int song, int role)
    {
        if (song < 0) return 0;
        const auto& s = songs[song];
        int mx = -1;
        for (int i = 0; i < s.count; ++i)
        {
            const auto& n = songNotes[s.start + i];
            if (n.part == role) mx = juce::jmax (mx, (int) n.stem);
        }
        return mx + 1;
    }

    static Pattern build (const GenState& g)
    {
        Pattern p;
        if (g.song < 0 || g.song >= numSongs) return p;
        const auto& s = songs[g.song];
        p.length = s.len;
        p.songName = s.name;
        p.bpm = s.bpm;
        p.key = ((s.key + g.transpose) % 12 + 12) % 12;
        p.minor = s.minor;
        for (int i = 0; i < s.count; ++i)
        {
            const auto& n = songNotes[s.start + i];
            if (n.part > 3 || n.stem != g.stem[(size_t) n.part]) continue;
            const double st = n.s, len = juce::jmin ((double) n.l, p.length - n.s);
            if (st >= p.length || len <= 0.0) continue;
            p.parts[(size_t) n.part].push_back ({ st, juce::jmax (0.05, len), juce::jlimit (0, 127, n.p + g.transpose), juce::jlimit (1, 127, (int) n.v) });
        }
        p.hasBassLine = ! p.parts[PBass].empty();

        // drums : le groove boucle sur toute la longueur
        if (g.groove >= 0 && g.groove < numGrooves)
        {
            const auto& gr = grooves[g.groove];
            p.grooveName = gr.name;
            for (double off = 0.0; off < p.length - 1.0e-6; off += gr.len)
                for (int i = 0; i < gr.count; ++i)
                {
                    const auto& h = grooveHits[gr.start + i];
                    const double t = off + h.s;
                    if (t >= p.length) continue;
                    p.parts[PDrums].push_back ({ t, 0.1, (int) h.lane, juce::jlimit (1, 127, (int) h.v) });
                }
        }

        // 808 : si le MIDI n'a pas de ligne de basse, elle suit les kicks
        //       et prend la note la plus grave des accords qui sonnent a ce moment
        if (! p.hasBassLine)
        {
            std::vector<double> kicks;
            for (auto& d : p.parts[PDrums]) if (d.pitch == LKick) kicks.push_back (d.start);
            std::sort (kicks.begin(), kicks.end());
            kicks.erase (std::unique (kicks.begin(), kicks.end(), [] (double a, double b) { return std::abs (a - b) < 0.05; }), kicks.end());
            const auto& src = ! p.parts[PChords].empty() ? p.parts[PChords] : p.parts[PMel];
            for (size_t k = 0; k < kicks.size(); ++k)
            {
                const double t = kicks[k];
                int low = 999;
                for (auto& n : src) if (n.start <= t + 0.06 && n.start + n.length > t + 0.06) low = juce::jmin (low, n.pitch);
                if (low == 999) // rien ne sonne : derniere note jouee avant
                {
                    double best = -1.0;
                    for (auto& n : src) if (n.start <= t + 0.06 && n.start > best) { best = n.start; low = n.pitch; }
                    for (auto& n : src) if (std::abs (n.start - best) < 0.06) low = juce::jmin (low, n.pitch);
                }
                if (low == 999) continue;
                int note = 24 + ((low % 12) + 12) % 12;
                if (note < 26) note += 12;
                const double next = k + 1 < kicks.size() ? kicks[k + 1] : p.length;
                const double len = juce::jlimit (0.2, 4.0, next - t - 0.02);
                p.parts[PBass].push_back ({ t, len, note, 110 });
            }
        }
        for (auto& v : p.parts) std::sort (v.begin(), v.end(), [] (const PNote& a, const PNote& b) { return a.start < b.start; });
        return p;
    }
};

// -----------------------------------------------------------------------------
//  Rendu audio : pianos / samples / 808 / drums -> bus -> mix
// -----------------------------------------------------------------------------
class Engine
{
public:
    struct Settings
    {
        int inst[4] { 1, 0, 0, 0 };     // melodie, accords, basse (menu basse), contre-melo
        float vol[NumParts] { 0.8f, 0.6f, 0.8f, 0.5f, 0.85f };
        float reverb = 0.25f, master = 0.8f;
    };

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        sampler.prepare (sr);
        piano.prepare (sr);
        buses.setSize (Sampler::numBuses * 2, juce::jmax (16, maxBlock));
        verbBuf.setSize (2, juce::jmax (16, maxBlock));
        reverb.setSampleRate (sr);
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.55f; rp.damping = 0.5f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 1.0f;
        reverb.setParameters (rp);
        reverb.reset();
    }

    void setSettings (const Settings& s) { set = s; }
    void setState (const GenState& g) { state = g; }

    void noteOn (int part, int pitch, int vel)
    {
        const float v = vel / 127.0f;
        if (part == PDrums)
        {
            const auto* set2 = SoundBank::get().drums (drumCategory (state.genre, pitch, state.clapVariant));
            if (set2 == nullptr || set2->empty()) return;
            const auto& smp = (*set2)[(size_t) juce::jlimit (0, (int) set2->size() - 1, state.kit[(size_t) pitch])];
            const int choke = (pitch == LHat || pitch == LOpen) ? 1 : -1;
            sampler.noteOn (PDrums, pitch, v, &smp, false, 0.05f, choke);
            return;
        }
        if (part == PBass)
        {
            if (auto* in = SoundBank::get().bass (set.inst[PBass]))
                sampler.noteOn (PBass, pitch, juce::jmax (0.75f, v), in->zoneFor (pitch), true, 0.08f, 2); // 808 monophonique
            return;
        }
        const int sel = set.inst[part];
        if (sel <= 0) { piano.noteOn (part, pitch, v); return; }
        if (auto* in = SoundBank::get().melodic (sel))
        {
            const int n = lift (part, pitch);
            sampler.noteOn (part, n, v, in->zoneFor (n), true, in->group == "Pads" ? 0.6f : 0.3f);
        }
    }

    void noteOff (int part, int pitch)
    {
        if (part == PDrums) return;
        if (part == PBass) { sampler.noteOff (PBass, pitch); return; }
        piano.noteOff (part, pitch);
        sampler.noteOff (part, lift (part, pitch));
    }

    void allOff() { sampler.allOff(); piano.allOff(); }

    void render (juce::AudioBuffer<float>& out, const std::vector<std::pair<int, std::array<int, 4>>>& evs) // (sample, {on, part, pitch, vel})
    {
        const int num = out.getNumSamples();
        if (buses.getNumSamples() < num) { buses.setSize (Sampler::numBuses * 2, num, false, false, true); verbBuf.setSize (2, num, false, false, true); }
        buses.clear();
        int pos = 0;
        for (auto& e : evs)
        {
            const int t = juce::jlimit (0, num, e.first);
            renderRange (pos, t);
            if (e.second[0]) noteOn (e.second[1], e.second[2], e.second[3]);
            else             noteOff (e.second[1], e.second[2]);
            pos = t;
        }
        renderRange (pos, num);

        // gains par partie (niveaux de base calibres pour que tout soit equilibre a 100 %)
        static const float base[NumParts] = { 0.85f, 0.65f, 0.5f, 0.55f, 0.62f };
        verbBuf.clear();
        out.clear();
        auto* L = out.getWritePointer (0);
        auto* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
        for (int b = 0; b < NumParts; ++b)
        {
            const float g = base[b] * set.vol[b];
            const float send = (b == PMel || b == PChords || b == PExtra) ? set.reverb : 0.0f;
            const float* bl = buses.getReadPointer (b * 2);
            const float* br = buses.getReadPointer (b * 2 + 1);
            for (int i = 0; i < num; ++i)
            {
                const float l = bl[i] * g, r = br[i] * g;
                L[i] += l;
                if (R != nullptr) R[i] += r; else L[i] += r;
                if (send > 0.0f) { verbBuf.getWritePointer (0)[i] += l * send; verbBuf.getWritePointer (1)[i] += r * send; }
            }
        }
        reverb.processStereo (verbBuf.getWritePointer (0), verbBuf.getWritePointer (1), num);
        float peak = 0.0f;
        for (int i = 0; i < num; ++i)
        {
            float l = (L[i] + verbBuf.getReadPointer (0)[i] * 0.6f) * set.master;
            l = std::tanh (l * 1.1f) * 0.92f;
            L[i] = l;
            peak = juce::jmax (peak, std::abs (l));
            if (R != nullptr)
            {
                float r = (R[i] + verbBuf.getReadPointer (1)[i] * 0.6f) * set.master;
                r = std::tanh (r * 1.1f) * 0.92f;
                R[i] = r;
                peak = juce::jmax (peak, std::abs (r));
            }
        }
        meter = juce::jmax (peak, meter * 0.9f);
    }

    float getMeter() const { return meter; }

private:
    // registre des sons samples : rien de trop grave (la 808 garde le grave)
    static int lift (int part, int n)
    {
        while (n < 48) n += 12;
        if (part == PChords) while (n > 76) n -= 12;
        return n;
    }

    void renderRange (int a, int b)
    {
        if (b <= a) return;
        sampler.render (buses, a, b);
        piano.render (buses, a, b);
    }

    double sr = 44100.0;
    Sampler sampler;
    PianoSynth piano;
    juce::AudioBuffer<float> buses, verbBuf;
    juce::Reverb reverb;
    Settings set;
    GenState state;
    float meter = 0.0f;
};
} // namespace ranti
