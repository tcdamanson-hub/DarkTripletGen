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
    static const char* rb[NumLanes] = { "RB_KICK", "RB_SNARE", "RB_CLAP", "RB_HAT", "RB_SHAKER", "RB_OHAT", "RB_RIM", "RB_PERC", "RB_PERC", "RB_SNAP", "RB_CRASH" };
    static const char* af[NumLanes] = { "AF_KICK", "AF_SNARE", "AF_CLAP", "AF_HAT", "AF_SHAKER", "AF_OHAT", "AF_RIM", "AF_HAND", "AF_DRUM", "AF_WOOD", "-" };
    static const char* sw[NumLanes] = { "SW_KICK", "SW_SNARE", "SW_CLAP", "SW_HAT", "SW_HAT", "SW_OHAT", "SW_RIM", "SW_PERC", "SW_PERC", "SW_PERC", "SW_CRASH" };
    if (genre == 3)
    {
        // kit synthwave ; si une categorie manque, on prend celle du R&B
        if (SoundBank::get().drums (sw[lane]) != nullptr) return sw[lane];
        return rb[lane];
    }
    if (genre == 2)
    {
        // pop : kits Prism (le kit vient du meme groupe que le groove)
        static const char* pp[NumLanes] = { "PP_KICK", "PP_SNARE", "PP_CLAP", "PP_HAT", "PP_HAT", "PP_OHAT", "RB_RIM", "PP_LTOM", "PP_HTOM", "RB_PERC", "PP_CRASH" };
        if (SoundBank::get().drums (pp[lane]) != nullptr) return pp[lane];
        return rb[lane];
    }
    if (genre == 1)
    {
        juce::ignoreUnused (variant); // kit afro : clap (pas de snap)
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
    // R&B : un MIDI par element (index dans parts[], -1 = rien) ; bass808 = rythme de la 808
    std::array<int, 5> rnb { -1, -1, -1, -1, -1 }; // kick, snare, hats, open hat, percu
    int bass808 = -1;

    juce::String toString() const
    {
        juce::StringArray a;
        a.add (juce::String (genre)); a.add (juce::String (song)); a.add (juce::String (groove)); a.add (juce::String (transpose));
        for (auto s : stem) a.add (juce::String (s));
        for (auto k : kit) a.add (juce::String (k));
        a.add (juce::String (clapVariant));
        for (auto r : rnb) a.add (juce::String (r));
        a.add (juce::String (bass808));
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
        if (a.size() >= i + 6)
        {
            for (auto& x : g.rnb) x = a[i++].getIntValue();
            g.bass808 = a[i++].getIntValue();
        }
        return g;
    }
};

struct PNote { double start, length; int pitch, vel; };
struct ChordName { double start; juce::String name; };

inline int bits (int x) { int c = 0; while (x) { c += x & 1; x >>= 1; } return c; }

// nom d'accord a partir des notes jouees ensemble (Cm7, Ab, Fsus4, G/B ...)
inline juce::String nameChord (const std::vector<int>& pitches)
{
    if (pitches.empty()) return {};
    int bass = 127; int mask = 0;
    for (auto p : pitches) { bass = juce::jmin (bass, p); mask |= 1 << (((p % 12) + 12) % 12); }
    struct T { int m; const char* s; };
    static const T tpl[] = { { 0x891, "maj7" }, { 0x489, "m7" }, { 0x491, "7" }, { 0x449, "m7b5" }, { 0x249, "dim7" },
                             { 0x291, "6" }, { 0x289, "m6" }, { 0x095, "add9" }, { 0x08d, "m(add9)" },
                             { 0x091, "" }, { 0x089, "m" }, { 0x049, "dim" }, { 0x111, "aug" }, { 0x0a1, "sus4" }, { 0x085, "sus2" }, { 0x081, "5" } };
    int bestRoot = -1, bestScore = -100; const char* bestSuf = "";
    for (int r = 0; r < 12; ++r)
    {
        const int rot = ((mask >> r) | (mask << (12 - r))) & 0xfff;
        for (auto& t : tpl)
        {
            if ((rot & t.m) != t.m) continue;
            const int extra = bits (rot & ~t.m);
            const int score = bits (t.m) * 3 - extra * 2 + (r == bass % 12 ? 2 : 0);
            if (score > bestScore) { bestScore = score; bestRoot = r; bestSuf = t.s; }
        }
    }
    if (bestRoot < 0) return keyNames[bass % 12];
    juce::String n = juce::String (keyNames[bestRoot]) + bestSuf;
    if (bass % 12 != bestRoot) n << "/" << keyNames[bass % 12];
    return n;
}

inline std::vector<ChordName> chordNames (const std::vector<PNote>& notes)
{
    std::vector<ChordName> out;
    auto sorted = notes;
    std::sort (sorted.begin(), sorted.end(), [] (const PNote& a, const PNote& b) { return a.start < b.start; });
    size_t i = 0;
    while (i < sorted.size())
    {
        std::vector<int> ps;
        const double t = sorted[i].start;
        size_t j = i;
        while (j < sorted.size() && sorted[j].start - t < 0.06) ps.push_back (sorted[j++].pitch);
        // notes encore tenues a ce moment (accord joue en arpege)
        for (auto& n : sorted) if (n.start < t - 0.06 && n.start + n.length > t + 0.1) ps.push_back (n.pitch);
        if (ps.size() >= 2)
        {
            auto nm = nameChord (ps);
            if (out.empty() || out.back().name != nm) out.push_back ({ t, nm });
        }
        i = j;
    }
    return out;
}

struct Pattern
{
    double length = 32.0;
    std::array<std::vector<PNote>, NumParts> parts; // drums : pitch = piste (Lane)
    juce::String songName, grooveName;
    int bpm = 120, key = 0, minor = 1;
    bool hasBassLine = false;
    std::vector<ChordName> chords;
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
        if (genre == 0) { randomRnb (g, rng); g.bass808 = random808 (s.bpm, rng); }
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
        // pop : un kit complet du groupe du groove (moderne, californie, retro, indie), jamais melange
        if (g.genre == 2 && g.groove >= 0 && g.groove < numGrooves)
        {
            static const char* groupName[] = { "", "Moderne", "Californie", "Retro", "Indie" };
            const int grp = juce::jlimit (0, 4, grooves[g.groove].group);
            if (const auto* kicks = SoundBank::get().drums ("PP_KICK"); kicks != nullptr && grp > 0)
            {
                std::vector<juce::String> names;
                for (auto& k : *kicks) if (k.name.startsWith (groupName[grp])) names.push_back (k.name);
                if (! names.empty())
                {
                    const auto kitName = names[(size_t) rng.nextInt ((int) names.size())];
                    for (int l = 0; l < NumLanes; ++l)
                        if (const auto* set = SoundBank::get().drums (drumCategory (2, l, 0)))
                        {
                            int found = -1, sameGroup = -1;
                            for (size_t i = 0; i < set->size(); ++i)
                            {
                                if ((*set)[i].name == kitName) found = (int) i;
                                if (sameGroup < 0 && (*set)[i].name.startsWith (groupName[grp])) sameGroup = (int) i;
                            }
                            if (found >= 0) g.kit[(size_t) l] = found;
                            else if (sameGroup >= 0) g.kit[(size_t) l] = sameGroup;
                        }
                }
            }
        }
    }

    // R&B : un MIDI kick, un snare, un hats (+ open hat, percu de temps en temps), tempos compatibles
    static int pickPart (std::initializer_list<int> kinds, int lo, int hi, juce::Random& rng)
    {
        std::vector<int> c, any;
        for (int i = 0; i < numParts; ++i)
            for (int k : kinds)
                if (parts[i].kind == k)
                {
                    any.push_back (i);
                    if (parts[i].lo <= hi && parts[i].hi >= lo) c.push_back (i);
                }
        const auto& v = c.empty() ? any : c;
        return v.empty() ? -1 : v[(size_t) rng.nextInt ((int) v.size())];
    }

    static void randomRnb (GenState& g, juce::Random& rng, int lo = 60, int hi = 200)
    {
        g.rnb[0] = pickPart ({ 0 }, lo, hi, rng);
        if (g.rnb[0] >= 0) { lo = juce::jmax (lo, parts[g.rnb[0]].lo); hi = juce::jmin (hi, parts[g.rnb[0]].hi); }
        g.rnb[1] = pickPart ({ 1, 2 }, lo, hi, rng);
        g.rnb[2] = pickPart ({ 3 }, lo, hi, rng);
        g.rnb[3] = rng.nextFloat() < 0.6f ? pickPart ({ 5 }, lo, hi, rng) : -1;
        g.rnb[4] = rng.nextFloat() < 0.4f ? pickPart ({ 7, 4 }, lo, hi, rng) : -1;
        g.groove = -1;
    }

    static int random808 (int songBpm, juce::Random& rng) { return pickPart ({ 99 }, songBpm - 15, songBpm + 15, rng); }

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

        // 808 : si le MIDI n'a pas de ligne de basse, elle suit les changements d'accords
        //       (note la plus grave de chaque accord, tenue jusqu'au suivant)
        const auto& src = ! p.parts[PChords].empty() ? p.parts[PChords] : p.parts[PMel];
        if (! p.hasBassLine && ! src.empty() && g.genre == 0 && g.bass808 >= 0 && g.bass808 < numParts)
        {
            const auto& bp = parts[g.bass808];
            std::vector<std::pair<double, int>> hits;
            for (double off = 0.0; off < p.length - 1.0e-6; off += bp.len)
                for (int i = 0; i < bp.count; ++i)
                {
                    const double t = off + partHits[bp.start + i].s;
                    if (t < p.length && (hits.empty() || t - hits.back().first > 0.05)) hits.push_back ({ t, (int) partHits[bp.start + i].v });
                }
            for (size_t k = 0; k < hits.size(); ++k)
            {
                const double t = hits[k].first;
                int low = 999;
                for (auto& n : src) if (n.start <= t + 0.06 && n.start + n.length > t + 0.06) low = juce::jmin (low, n.pitch);
                if (low == 999)
                {
                    double best = -1.0;
                    for (auto& n : src) if (n.start <= t + 0.06 && n.start > best) { best = n.start; low = n.pitch; }
                    for (auto& n : src) if (std::abs (n.start - best) < 0.06) low = juce::jmin (low, n.pitch);
                }
                if (low == 999) for (auto& n : src) low = juce::jmin (low, n.pitch);
                int note = 24 + ((low % 12) + 12) % 12;
                if (note < 26) note += 12;
                const double next = k + 1 < hits.size() ? hits[k + 1].first : p.length;
                p.parts[PBass].push_back ({ t, juce::jlimit (0.15, 2.0, next - t - 0.02), note, juce::jlimit (60, 127, hits[k].second + 20) });
            }
        }
        else if (! p.hasBassLine && ! src.empty())
        {
            auto sorted = src;
            std::sort (sorted.begin(), sorted.end(), [] (const PNote& a, const PNote& b) { return a.start < b.start; });
            std::vector<std::pair<double, int>> groups; // debut, note la plus grave
            for (auto& n : sorted)
            {
                if (! groups.empty() && n.start - groups.back().first < 0.06) groups.back().second = juce::jmin (groups.back().second, n.pitch);
                else groups.push_back ({ n.start, n.pitch });
            }
            for (size_t k = 0; k < groups.size(); ++k)
            {
                const double t = groups[k].first;
                int note = 24 + ((groups[k].second % 12) + 12) % 12;
                if (note < 26) note += 12;
                const double next = k + 1 < groups.size() ? groups[k + 1].first : p.length;
                p.parts[PBass].push_back ({ t, juce::jlimit (0.2, 4.0, next - t - 0.02), note, 110 });
            }
        }
        p.chords = chordNames (p.parts[PChords].empty() ? p.parts[PMel] : p.parts[PChords]);
        for (auto& v : p.parts) std::sort (v.begin(), v.end(), [] (const PNote& a, const PNote& b) { return a.start < b.start; });
        return p;
    }
};

// R&B : les MIDI d'elements joues ensemble (chacun boucle sur sa longueur)
inline std::vector<PNote> buildRnbDrums (const GenState& g, double& length, juce::String* name = nullptr)
{
    static const int laneOf[5] = { LKick, LSnare, LHat, LOpen, LPercA };
    static const char* tag[5] = { "K", "S", "H", "OH", "P" };
    std::vector<PNote> out;
    length = 16.0;
    juce::StringArray nm;
    for (int r = 0; r < 5; ++r)
        if (g.rnb[(size_t) r] >= 0 && g.rnb[(size_t) r] < numParts)
        {
            length = juce::jmax (length, (double) parts[g.rnb[(size_t) r]].len);
            nm.add (juce::String (tag[r]) + juce::String (parts[g.rnb[(size_t) r]].name).fromLastOccurrenceOf ("_", false, false));
        }
    for (int r = 0; r < 5; ++r)
    {
        const int pi = g.rnb[(size_t) r];
        if (pi < 0 || pi >= numParts) continue;
        const auto& pt = parts[pi];
        int lane = laneOf[r];
        if (pt.kind == 2) lane = LClap;
        if (pt.kind == 4) lane = LHat2;
        for (double off = 0.0; off < length - 1.0e-6; off += pt.len)
            for (int i = 0; i < pt.count; ++i)
            {
                const double t = off + partHits[pt.start + i].s;
                if (t < length) out.push_back ({ t, 0.1, lane, juce::jlimit (1, 127, (int) partHits[pt.start + i].v) });
            }
    }
    if (name != nullptr) *name = nm.joinIntoString (" ");
    std::sort (out.begin(), out.end(), [] (const PNote& a, const PNote& b) { return a.start < b.start; });
    return out;
}

// drums seuls : le groove, boucle sur 'length' temps
inline std::vector<PNote> buildDrums (int groove, double length, juce::String* name = nullptr)
{
    std::vector<PNote> out;
    if (groove < 0 || groove >= numGrooves) return out;
    const auto& gr = grooves[groove];
    if (name != nullptr) *name = gr.name;
    for (double off = 0.0; off < length - 1.0e-6; off += gr.len)
        for (int i = 0; i < gr.count; ++i)
        {
            const auto& h = grooveHits[gr.start + i];
            const double t = off + h.s;
            if (t < length) out.push_back ({ t, 0.1, (int) h.lane, juce::jlimit (1, 127, (int) h.v) });
        }
    std::sort (out.begin(), out.end(), [] (const PNote& a, const PNote& b) { return a.start < b.start; });
    return out;
}

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
        float lane[NumLanes] { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
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
            sampler.noteOn (PDrums, pitch, v, &smp, false, 0.05f, choke, set.lane[pitch]);
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
    int lift (int part, int n) const
    {
        if (state.genre == 1) return n; // afro : les pistes jouent a leur vraie hauteur
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
