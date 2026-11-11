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
enum Part { PMel = 0, PChords = 1, PBass = 2, PExtra = 3, PDrums = 4, PArp = 5, PPad = 6, NumParts = 7 };
inline bool isMelodicPart (int p) { return p >= 0 && p < NumParts && p != PDrums; }
enum Lane { LKick = 0, LSnare, LClap, LHat, LHat2, LOpen, LRim, LPercA, LPercB, LPercC, LCrash, NumLanes };

static const char* const genreNames[] = { "R&B", "AFRO", "POP", "SYNTHWAVE", "POP WAVE" };
static constexpr int numGenres = 5;
static const char* const keyNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
// gammes / modes (detectes sur les notes de chaque MIDI ; les notes ne sont jamais modifiees)
static const int NumModes = 19;
static const char* const modeShort[NumModes] = { "IONIEN", "DORIEN", "PHRYGIEN", "LYDIEN", "MIXOLYDIEN", "EOLIEN", "LOCRIEN", "MIN. HARM.", "MIN. MELO.",
                                                 "MAJ. HARM.", "NAPOL. MIN.", "NAPOL. MAJ.", "PHRYG. DOM.", "HONGROIS", "DBL HARM.", "PENTA MAJ.", "PENTA MIN.", "BLUES MIN.", "BLUES MAJ." };
inline bool modeIsMajor (int m) { return m == 0 || m == 3 || m == 4 || m == 9 || m == 12 || m == 14 || m == 15 || m == 18; }
// choix : -1 toutes, -2 majeures, -3 mineures, 0..18 une gamme precise
inline bool modeMatches (int song, int choice)
{
    if (choice == -1) return true;
    const int m = songs[song].mode;
    if (choice == -2) return modeIsMajor (m);
    if (choice == -3) return ! modeIsMajor (m);
    return m == choice;
}

// categorie de samples par genre et par piste de drums
inline juce::String drumCategory (int genre, int lane, int variant)
{
    static const char* rb[NumLanes] = { "RB_KICK", "RB_SNARE", "RB_CLAP", "RB_HAT", "RB_SHAKER", "RB_OHAT", "RB_RIM", "RB_PERC", "RB_PERC", "RB_SNAP", "RB_CRASH" };
    static const char* af[NumLanes] = { "AF_KICK", "AF_SNARE", "AF_CLAP", "AF_HAT", "AF_SHAKER", "AF_OHAT", "AF_RIM", "AF_CONGA", "AF_CONGA", "AF_SNAP", "AF_CRASH" };
    static const char* sw[NumLanes] = { "SW_KICK", "SW_SNARE", "SW_CLAP", "SW_HAT", "SW_HAT", "SW_OHAT", "SW_RIM", "SW_PERC", "SW_PERC", "SW_PERC", "SW_CRASH" };
    if (genre == 3)
    {
        // kit synthwave ; si une categorie manque, on prend celle du R&B
        if (SoundBank::get().drums (sw[lane]) != nullptr) return sw[lane];
        return rb[lane];
    }
    if (genre == 2)
    {
        // pop : kits Essential WAV From Mars, un kit = une machine (le kit vient du meme groupe que le groove)
        static const char* pp[NumLanes] = { "PP_KICK", "PP_SNARE", "PP_CLAP", "PP_HAT", "PP_HAT", "PP_OHAT", "PP_RIM", "PP_LTOM", "PP_HTOM", "PP_PERC", "PP_CRASH" };
        if (SoundBank::get().drums (pp[lane]) != nullptr) return pp[lane];
        return rb[lane];
    }
    if (genre == 1)
    {
        juce::ignoreUnused (variant); // kit afro : clap (pas de snap)
        return af[lane];
    }
    if (genre == 4)
    {
        // pop wave : boites a rythmes 80s, un kit = une machine (jamais melange)
        static const char* pw[NumLanes] = { "PW_KICK", "PW_SNARE", "PW_CLAP", "PW_HAT", "PW_RIDE", "PW_OHAT", "PW_RIM", "PW_LTOM", "PW_HTOM", "PW_PERC", "PW_CRASH" };
        return pw[lane];
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
    std::array<int, NumParts> stem {};
    std::array<int, NumLanes> kit {};
    int clapVariant = 0;
    // R&B : un MIDI par element (index dans parts[], -1 = rien) ; bass808 = rythme de la 808
    std::array<int, 5> rnb { -1, -1, -1, -1, -1 }; // kick, snare, hats, open hat, percu
    int bass808 = -1;

    juce::String toString() const
    {
        juce::StringArray a;
        a.add (juce::String (genre)); a.add (juce::String (song)); a.add (juce::String (groove)); a.add (juce::String (transpose));
        for (int i = 0; i < 4; ++i) a.add (juce::String (stem[(size_t) i]));
        for (auto k : kit) a.add (juce::String (k));
        a.add (juce::String (clapVariant));
        for (auto r : rnb) a.add (juce::String (r));
        a.add (juce::String (bass808));
        a.add (juce::String (stem[PArp])); a.add (juce::String (stem[PPad]));
        return a.joinIntoString (",");
    }
    static GenState fromString (const juce::String& s)
    {
        GenState g;
        auto a = juce::StringArray::fromTokens (s, ",", "");
        if (a.size() < 4 + 4 + NumLanes + 1) return g;
        int i = 0;
        g.genre = a[i++].getIntValue(); g.song = a[i++].getIntValue(); g.groove = a[i++].getIntValue(); g.transpose = a[i++].getIntValue();
        for (int k = 0; k < 4; ++k) g.stem[(size_t) k] = a[i++].getIntValue();
        for (auto& x : g.kit) x = a[i++].getIntValue();
        g.clapVariant = a[i++].getIntValue();
        if (a.size() >= i + 6)
        {
            for (auto& x : g.rnb) x = a[i++].getIntValue();
            g.bass808 = a[i++].getIntValue();
        }
        if (a.size() >= i + 2) { g.stem[PArp] = a[i++].getIntValue(); g.stem[PPad] = a[i++].getIntValue(); }
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
    int bpm = 120, key = 0, minor = 1, mode = 5;
    bool hasBassLine = false;
    std::vector<ChordName> chords;
};

class Generator
{
public:
    // nouvelle generation : morceau au hasard du genre, groove au hasard, kit au hasard
    static GenState randomState (int genre, int keyChoice, int previousSong, juce::Random& rng, int modeChoice = -1)
    {
        GenState g;
        g.genre = genre;
        std::vector<int> cand;
        for (int i = 0; i < numSongs; ++i) if (songs[i].genre == genre && i != previousSong && modeMatches (i, modeChoice)) cand.push_back (i);
        if (cand.empty()) for (int i = 0; i < numSongs; ++i) if (songs[i].genre == genre && modeMatches (i, modeChoice)) cand.push_back (i);
        if (cand.empty()) for (int i = 0; i < numSongs; ++i) if (songs[i].genre == genre) cand.push_back (i);
        if (cand.empty()) return g;
        g.song = cand[(size_t) rng.nextInt ((int) cand.size())];
        g.groove = randomGroove (genre, rng);
        const auto& s = songs[g.song];
        for (int r = 0; r < NumParts; ++r)
        {
            if (! isMelodicPart (r)) continue;
            const int n = stemCount (g.song, r);
            g.stem[(size_t) r] = n > 0 ? rng.nextInt (n) : 0;
        }
        randomKit (g, rng);
        if (genre == 0) { randomRnb (g, rng); g.bass808 = random808 (s.bpm, rng); }
        g.transpose = transposeFor (s.key, keyChoice);
        return g;
    }

    // POP WAVE et AFRO : on prend un groove joue a un tempo proche de celui du projet (sinon il tombe mal sur la melodie)
    static int randomGroove (int genre, juce::Random& rng, double hostBpm = 0.0)
    {
        std::vector<int> gc;
        if ((genre == 4 || genre == 1) && hostBpm > 40.0)
        {
            std::vector<std::pair<double, int>> byGap;
            for (int i = 0; i < numGrooves; ++i)
                if (grooves[i].genre == genre)
                {
                    // un groove a 70 bpm sonne aussi en double tempo a 140 : on garde l'ecart le plus petit
                    const double b = grooves[i].bpm;
                    const double gap = juce::jmin (std::abs (b - hostBpm), std::abs (b * 2.0 - hostBpm), std::abs (b * 0.5 - hostBpm));
                    byGap.push_back ({ gap, i });
                }
            std::sort (byGap.begin(), byGap.end());
            for (auto& [gap, i] : byGap) if (gap <= 10.0 || gc.size() < 6) gc.push_back (i);
            if (! gc.empty()) return gc[(size_t) rng.nextInt ((int) gc.size())];
        }
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
        // afro : deux congas differentes pour Percussion et Percussion 2
        if (g.genre == 1)
            if (const auto* c = SoundBank::get().drums ("AF_CONGA"); c != nullptr && c->size() > 1)
                while (g.kit[(size_t) LPercB] == g.kit[(size_t) LPercA]) g.kit[(size_t) LPercB] = rng.nextInt ((int) c->size());
        // pop wave : une seule machine pour tout le kit, et seulement une machine qui a TOUS les elements
        //            que joue le groove (rien de muet) ; on prend ses sons d'origine (n. 01), qui vont ensemble
        if (g.genre == 4)
            pickPopWaveKit (g, rng);
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

    static std::array<bool, NumLanes> lanesUsed (int groove)
    {
        std::array<bool, NumLanes> u {};
        if (groove >= 0 && groove < numGrooves)
            for (int i = 0; i < grooves[groove].count; ++i)
                u[(size_t) juce::jlimit (0, NumLanes - 1, (int) grooveHits[grooves[groove].start + i].lane)] = true;
        return u;
    }

    // index du son d'origine (le plus petit numero) de cette machine pour cette piste, -1 si elle n'en a pas
    static int machineSample (int lane, const juce::String& machine)
    {
        const auto* set = SoundBank::get().drums (drumCategory (4, lane, 0));
        if (set == nullptr) return -1;
        for (size_t i = 0; i < set->size(); ++i)
            if ((*set)[i].name.upToLastOccurrenceOf (" ", false, false) == machine) return (int) i; // trie par nom : 01 en premier
        return -1;
    }

    static bool kitCoversGroove (const GenState& g)
    {
        const auto used = lanesUsed (g.groove);
        for (int l = 0; l < NumLanes; ++l) if (used[(size_t) l] && g.kit[(size_t) l] < 0) return false;
        return true;
    }

    static void pickPopWaveKit (GenState& g, juce::Random& rng)
    {
        std::vector<juce::String> machines;
        if (const auto* kicks = SoundBank::get().drums (drumCategory (4, LKick, 0)))
            for (auto& smp : *kicks)
            {
                const auto m = smp.name.upToLastOccurrenceOf (" ", false, false);
                if (std::find (machines.begin(), machines.end(), m) == machines.end()) machines.push_back (m);
            }
        if (machines.empty()) return;
        const auto used = lanesUsed (g.groove);
        std::vector<juce::String> full;
        for (auto& m : machines)
        {
            bool ok = true;
            for (int l = 0; l < NumLanes; ++l) if (used[(size_t) l] && machineSample (l, m) < 0) ok = false;
            if (ok) full.push_back (m);
        }
        const auto& from = full.empty() ? machines : full;
        const auto m = from[(size_t) rng.nextInt ((int) from.size())];
        for (int l = 0; l < NumLanes; ++l) g.kit[(size_t) l] = machineSample (l, m);
        // toms : le grave et l'aigu d'origine de la machine
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
        p.mode = s.mode;
        for (int i = 0; i < s.count; ++i)
        {
            const auto& n = songNotes[s.start + i];
            if (! isMelodicPart (n.part) || n.stem != g.stem[(size_t) n.part]) continue;
            const double st = n.s, len = juce::jmin ((double) n.l, p.length - n.s);
            if (st >= p.length || len <= 0.0) continue;
            p.parts[(size_t) n.part].push_back ({ st, juce::jmax (0.05, len), juce::jlimit (0, 127, n.p + g.transpose), juce::jlimit (1, 127, (int) n.v) });
        }
        p.hasBassLine = ! p.parts[PBass].empty();

        // POP WAVE : la basse reste dans le grave. Toute la ligne descend d'octave(s) pour que son centre tombe
        // dans le registre des basses de tes kits (MIDI 31-42 : meme hauteur entendue que la version de 12 h 42, maintenant que les basses des kits sont etiquetees a leur vraie note) ; les notes au-dessus de Do3 (48) descendent d'une octave.
        // Rythme et notes inchanges, seule l'octave bouge.
        if (g.genre == 4 && p.hasBassLine)
        {
            auto& b = p.parts[PBass];
            std::vector<int> ps;
            for (auto& n : b) ps.push_back (n.pitch);
            std::nth_element (ps.begin(), ps.begin() + (long) ps.size() / 2, ps.end());
            int med = ps[ps.size() / 2], shift = 0;
            while (med + shift > 42) shift -= 12;
            while (med + shift < 31) shift += 12;
            for (auto& n : b)
            {
                int q = n.pitch + shift;
                while (q > 48) q -= 12;
                while (q < 24) q += 12;
                n.pitch = q;
            }
        }

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
// -----------------------------------------------------------------------------
//  Couleur POP WAVE (mesuree sur les demos de reference) : synthes tres larges
//  (chorus d'ensemble), mediums en retrait, aigus doux. Uniquement en POP WAVE.
// -----------------------------------------------------------------------------
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1[2] {}, z2[2] {};
    void peak (double sr, double f, double q, double dB)
    {
        const double A = std::pow (10.0, dB / 40.0), w = 2.0 * juce::MathConstants<double>::pi * f / sr, al = std::sin (w) / (2.0 * q), c = std::cos (w);
        const double a0 = 1 + al / A;
        b0 = (1 + al * A) / a0; b1 = -2 * c / a0; b2 = (1 - al * A) / a0; a1 = -2 * c / a0; a2 = (1 - al / A) / a0;
    }
    void highShelf (double sr, double f, double dB)
    {
        const double A = std::pow (10.0, dB / 40.0), w = 2.0 * juce::MathConstants<double>::pi * f / sr, c = std::cos (w), s = std::sin (w);
        const double al = s / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * al;
        const double a0 = (A + 1) - (A - 1) * c + sq;
        b0 = A * ((A + 1) + (A - 1) * c + sq) / a0; b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0; b2 = A * ((A + 1) + (A - 1) * c - sq) / a0;
        a1 = 2 * ((A - 1) - (A + 1) * c) / a0; a2 = ((A + 1) - (A - 1) * c - sq) / a0;
    }
    void lowShelf (double sr, double f, double dB)
    {
        const double A = std::pow (10.0, dB / 40.0), w = 2.0 * juce::MathConstants<double>::pi * f / sr, c = std::cos (w), s = std::sin (w);
        const double al = s / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * al;
        const double a0 = (A + 1) + (A - 1) * c + sq;
        b0 = A * ((A + 1) - (A - 1) * c + sq) / a0; b1 = 2 * A * ((A - 1) - (A + 1) * c) / a0; b2 = A * ((A + 1) - (A - 1) * c - sq) / a0;
        a1 = -2 * ((A - 1) + (A + 1) * c) / a0; a2 = ((A + 1) + (A - 1) * c - sq) / a0;
    }
    void lowPass (double sr, double f, double q)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1 + al;
        b0 = (1 - c) / 2 / a0; b1 = (1 - c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    float process (int ch, float x)
    {
        const double y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return (float) y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
};

// POP WAVE : vrai mixage par piste (coupe-bas, nettoyage, place de chacun dans le spectre, position stereo fixe)
struct PopWaveMixStrip
{
    Biquad f[3]; int n = 0; float panL = 1.0f, panR = 1.0f;
    void process (float* L, float* R, int num)
    {
        for (int k = 0; k < n; ++k)
            for (int i = 0; i < num; ++i) { L[i] = f[k].process (0, L[i]); R[i] = f[k].process (1, R[i]); }
        if (panL != 1.0f || panR != 1.0f)
            for (int i = 0; i < num; ++i) { L[i] *= panL; R[i] *= panR; }
    }
};
inline void highPass (Biquad& q, double sr, double f)
{
    const double w = 2.0 * juce::MathConstants<double>::pi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * 0.707), a0 = 1 + al;
    q.b0 = (1 + c) / 2 / a0; q.b1 = -(1 + c) / a0; q.b2 = q.b0; q.a1 = -2 * c / a0; q.a2 = (1 - al) / a0; q.reset();
}
static constexpr double SUBDB = -9.0, PRESDB = 4.0, TOPDB = 0.0, BODYDB = -11.0;
struct PopWaveColour
{
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int n = (int) (sr * 0.03) + 4;
        for (auto& d : dl) { d.assign ((size_t) n, 0.0f); }
        w = 0; ph[0] = 0.0; ph[1] = 0.37;
        mid.peak (sr, 4300.0, 1.1, PRESDB);   // presence (haut-medium) au niveau des demos
        top.highShelf (sr, 6500.0, TOPDB);
        body.peak (sr, 1400.0, 0.7, BODYDB); body.reset();   // mediums en retrait comme dans les demos
        mid.reset(); top.reset();
        exLp[0] = exLp[1] = 0.0f;
        exK = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 3000.0 / sr));
    }
    // chorus d'ensemble stereo (deux lignes modulees en opposition) + egalisation
    void process (float* L, float* R, int num)
    {
        if (dl[0].empty()) return;
        const int n = (int) dl[0].size();
        const double inc[2] = { 0.45 / sr, 0.62 / sr };
        for (int i = 0; i < num; ++i)
        {
            const float inL = L[i], inR = R != nullptr ? R[i] : L[i];
            dl[0][(size_t) w] = inL; dl[1][(size_t) w] = inR;
            float wet[2];
            for (int c = 0; c < 2; ++c)
            {
                ph[c] += inc[c]; if (ph[c] >= 1.0) ph[c] -= 1.0;
                const double lfo = std::sin (2.0 * juce::MathConstants<double>::pi * (ph[c] + (c == 1 ? 0.5 : 0.0)));
                const double dsm = (0.0075 + 0.0 * lfo) * sr;   // largeur fixe, sans modulation : le son ne bouge plus de gauche a droite, phase stable
                double rp = w - dsm; while (rp < 0) rp += n;
                const int i0 = (int) rp; const float fr = (float) (rp - i0);
                const auto& d = dl[(size_t) (1 - c)];   // croise : la gauche lit la droite retardee (elargit)
                wet[c] = d[(size_t) i0] * (1.0f - fr) + d[(size_t) ((i0 + 1) % n)] * fr;
            }
            float l = inL + wet[0] * 0.0f;   // plus de copie retardee : pas de filtre en peigne, phase intacte
            float r = inR + wet[1] * 0.0f;
            l = top.process (0, mid.process (0, body.process (0, l)));
            r = top.process (1, mid.process (1, body.process (1, r)));
            // brillance 80s : le haut du spectre est legerement sature (excitateur) -> de l'air au-dessus de 5 kHz
            exLp[0] += exK * (l - exLp[0]); exLp[1] += exK * (r - exLp[1]);
            l += exAmt * std::tanh (4.0f * (l - exLp[0]));
            r += exAmt * std::tanh (4.0f * (r - exLp[1]));
            L[i] = l; if (R != nullptr) R[i] = r;
            if (++w >= n) w = 0;
        }
    }
    double sr = 44100.0, ph[2] {};
    std::vector<float> dl[2];
    int w = 0;
    Biquad mid, top, body;
    float exLp[2] {}, exK = 0.35f, exAmt = 0.0f;
};

// POP WAVE : basse comme dans tes kits = sombre (passe-bas) et large (le grave reste au centre,
// le haut de la basse est elargi par un chorus), pas une basse mono qui claque
struct PopWaveBassColour
{
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int n = (int) (sr * 0.03) + 4;
        for (auto& d : dl) d.assign ((size_t) n, 0.0f);
        w = 0; ph = 0.0; low[0] = low[1] = 0.0f;
        lp.lowPass (sr, 1400.0, 0.7); lp.reset();   // filtre ouvert : la basse garde sa couleur
        sub.lowShelf (sr, 90.0, SUBDB); sub.reset();
        k = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 160.0 / sr));
    }
    void process (float* L, float* R, int num)
    {
        if (dl[0].empty() || R == nullptr) return;
        const int n = (int) dl[0].size();
        for (int i = 0; i < num; ++i)
        {
            float x[2] = { sub.process (0, lp.process (0, L[i])), sub.process (1, lp.process (1, R[i])) };
            float o[2];
            ph += 0.33 / sr; if (ph >= 1.0) ph -= 1.0;
            for (int c = 0; c < 2; ++c)
            {
                low[c] += k * (x[c] - low[c]);
                const float hi = x[c] - low[c];
                dl[c][(size_t) w] = hi;
                const double lfo = std::sin (2.0 * juce::MathConstants<double>::pi * (ph + c * 0.5));
                double rp = w - (0.009 + 0.004 * c + 0.0 * lfo) * sr; while (rp < 0) rp += n;
                const int i0 = (int) rp; const float fr = (float) (rp - i0);
                const auto& d = dl[(size_t) (1 - c)];
                const float wet = d[(size_t) i0] * (1.0f - fr) + d[(size_t) ((i0 + 1) % n)] * fr;
                o[c] = low[c] + hi + wet * 0.0f;   // basse en phase (pas de copie retardee)
            }
            L[i] = o[0]; R[i] = o[1];
            if (++w >= n) w = 0;
        }
    }
    double sr = 44100.0, ph = 0.0;
    std::vector<float> dl[2];
    int w = 0;
    float low[2] {}, k = 0.02f;
    Biquad lp, sub;
};

class Engine
{
public:
    struct Settings
    {
        int inst[NumParts] { 1, 0, 0, 0, 0, 0, 0 };     // par piste (basse : menu basse)
        float vol[NumParts] { 0.8f, 0.6f, 0.8f, 0.5f, 0.85f, 0.6f, 0.5f };
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
        levVerb.assign ((size_t) juce::jmax (8192, maxBlock), 1.0f); levEnv = 0.0009f; levGain = 1.0f; levLp = 0.0f;
        reverb.setSampleRate (sr);
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.55f; rp.damping = 0.5f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 1.0f;
        reverb.setParameters (rp);
        reverb.reset();
        colour.prepare (sr);
        {
            auto pan = [] (PopWaveMixStrip& m, float p) { m.panL = std::sqrt (0.5f * (1.0f - p)) * 1.414f; m.panR = std::sqrt (0.5f * (1.0f + p)) * 1.414f; };
            auto& me = mix[PMel];   me.n = 3;   highPass (me.f[0], sr, 180.0); me.f[1].peak (sr, 3000.0, 0.9, 2.0); me.f[2].peak (sr, 1200.0, 0.8, -5.0);
            auto& ch = mix[PChords]; ch.n = 3;  highPass (ch.f[0], sr, 160.0); ch.f[1].peak (sr, 1500.0, 0.7, -6.0); ch.f[2].lowPass (sr, 5000.0, 0.7);   // accords doux : mediums et attaque claquante adoucis
            auto& ex = mix[PExtra]; ex.n = 2;   highPass (ex.f[0], sr, 250.0); ex.f[1].peak (sr, 1000.0, 1.0, -2.5); pan (ex, 0.6f);
            auto& ar = mix[PArp];   ar.n = 2;   highPass (ar.f[0], sr, 300.0); ar.f[1].peak (sr, 700.0, 1.0, -3.0); pan (ar, -0.6f);
            auto& pd = mix[PPad];   pd.n = 3;   pan (pd, 0.0f);   highPass (pd.f[0], sr, 200.0); pd.f[1].peak (sr, 450.0, 1.0, -3.0); pd.f[2].peak (sr, 2500.0, 0.8, -4.0);
            for (auto& m : mix) for (auto& q : m.f) q.reset();
        }
        colBuf.setSize (2, juce::jmax (16, maxBlock));
        bassColour.prepare (sr);
        bassBuf.setSize (2, juce::jmax (16, maxBlock));
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
            if (state.kit[(size_t) pitch] < 0) return; // pas de son pour cette piste dans ce kit
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
            if (state.genre == 4)
            {
                // POP WAVE : chaque nouvel accord / nouvelle note coupe proprement ce qui sonnait avant sur la piste
                // (rien ne deborde sur l'harmonie suivante) ; les notes d'un meme accord (< 30 ms) restent ensemble
                if (now - lastOn[part] > (long long) (0.03 * sr)) sampler.cutBus (part, 0.06f);
                lastOn[part] = now;
                sampler.noteOn (part, n, v, in->zoneFor (n), true, 0.12f);
                return;
            }
            sampler.noteOn (part, n, v, in->zoneFor (n), true, (in->group == "Pads" || in->group == "Cordes") ? 0.6f : 0.3f);
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
        if (buses.getNumSamples() < num) { buses.setSize (Sampler::numBuses * 2, num, false, false, true); verbBuf.setSize (2, num, false, false, true); colBuf.setSize (2, num, false, false, true); bassBuf.setSize (2, num, false, false, true); }
        buses.clear();
        int pos = 0;
        for (auto& e : evs)
        {
            const int t = juce::jlimit (0, num, e.first);
            renderRange (pos, t);
            now = clock + t;
            if (e.second[0]) noteOn (e.second[1], e.second[2], e.second[3]);
            else             noteOff (e.second[1], e.second[2]);
            pos = t;
        }
        renderRange (pos, num);
        clock += num;

        // gains par partie (niveaux de base calibres pour que tout soit equilibre a 100 %)
        static const float base[NumParts] = { 0.85f, 0.65f, 0.5f, 0.55f, 0.62f, 0.55f, 0.5f };
        verbBuf.clear();
        colBuf.clear();
        bassBuf.clear();
        out.clear();
        auto* L = out.getWritePointer (0);
        auto* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
        const bool popWave = state.genre == 4;
        for (int b = 0; b < NumParts; ++b)
        {
            const float g = base[b] * set.vol[b];
            const bool melodic = (b == PMel || b == PChords || b == PExtra || b == PArp || b == PPad);
            const float send = melodic ? set.reverb : 0.0f;
            // POP WAVE : les synthes passent par la couleur du genre avant le mixage
            float* dstL = (popWave && melodic) ? colBuf.getWritePointer (0) : (popWave && b == PBass) ? bassBuf.getWritePointer (0) : L;
            float* dstR = (popWave && melodic) ? colBuf.getWritePointer (1) : (popWave && b == PBass) ? bassBuf.getWritePointer (1) : R;
            if (popWave && melodic) mix[b].process (buses.getWritePointer (b * 2), buses.getWritePointer (b * 2 + 1), num);
            const float* bl = buses.getReadPointer (b * 2);
            const float* br = buses.getReadPointer (b * 2 + 1);
            for (int i = 0; i < num; ++i)
            {
                const float l = bl[i] * g, r = br[i] * g;
                dstL[i] += l;
                if (dstR != nullptr) dstR[i] += r; else dstL[i] += r;
                if (send > 0.0f) { verbBuf.getWritePointer (0)[i] += l * send; verbBuf.getWritePointer (1)[i] += r * send; }
            }
        }
        if (popWave)
        {
            if (R != nullptr)
            {
                bassColour.process (bassBuf.getWritePointer (0), bassBuf.getWritePointer (1), num);
                for (int i = 0; i < num; ++i) { L[i] += bassBuf.getReadPointer (0)[i]; R[i] += bassBuf.getReadPointer (1)[i]; }
            }
            else for (int i = 0; i < num; ++i) L[i] += bassBuf.getReadPointer (0)[i] + bassBuf.getReadPointer (1)[i];
            colour.process (colBuf.getWritePointer (0), colBuf.getWritePointer (1), num);
            for (int i = 0; i < num; ++i)
            {
                L[i] += colBuf.getReadPointer (0)[i];
                if (R != nullptr) R[i] += colBuf.getReadPointer (1)[i]; else L[i] += colBuf.getReadPointer (1)[i];
            }
        }
        // POP WAVE : reverb propre au genre, grande et feutree (sombre)
        if (popWave != verbIsPopWave)
        {
            verbIsPopWave = popWave;
            juce::Reverb::Parameters rp;
            if (popWave) { rp.roomSize = 0.9f; rp.damping = 0.78f; }
            else         { rp.roomSize = 0.55f; rp.damping = 0.5f; }
            rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 1.0f;
            reverb.setParameters (rp);
        }
        if (popWave)   // on assombrit encore ce qui part dans la reverb
            for (int c = 0; c < 2; ++c)
            {
                auto* v = verbBuf.getWritePointer (c);
                for (int i = 0; i < num; ++i) { verbLp[c] += 0.4f * (v[i] - verbLp[c]); v[i] = verbLp[c]; }
            }
        reverb.processStereo (verbBuf.getWritePointer (0), verbBuf.getWritePointer (1), num);
        const float verbOut = popWave ? 0.8f : 0.6f;
        float peak = 0.0f;
        // POP WAVE et POP : niveleur de volume lent -> chaque morceau sort au meme niveau (corrige 70 % de l'ecart,
        // au plus +/-6 dB, reaction en ~0,4 s quand ca monte et ~1,5 s quand ca baisse : les nuances restent).
        // Ne touche ni aux sons, ni aux notes, ni a l'egalisation : seulement le volume general du morceau.
        const bool level = popWave || state.genre == 2;   // POP WAVE et POP
        if (level && (int) levVerb.size() < num) levVerb.resize ((size_t) num, 1.0f);
        if (level)
        {
            const float aUp = (float) std::exp (-1.0 / (0.4 * sr)), aDn = (float) std::exp (-1.0 / (1.5 * sr));
            const float gs = (float) std::exp (-1.0 / (0.05 * sr));
            for (int i = 0; i < num; ++i)
            {
                const float x = 0.5f * ((L[i] + verbBuf.getReadPointer (0)[i] * verbOut) + (R != nullptr ? R[i] + verbBuf.getReadPointer (1)[i] * verbOut : L[i]));
                const float hp = x - levLp; levLp += 0.0085f * (x - levLp);      // mesure sans l'extreme grave, comme l'oreille
                const float e = hp * hp;
                const float a = e > levEnv ? aUp : aDn;
                levEnv = a * levEnv + (1.0f - a) * e;
                const float rms = std::sqrt (levEnv) + 1.0e-6f;
                const float want = juce::jlimit (0.5f, 2.0f, std::pow (levTarget / rms, 0.7f));
                levGain = gs * levGain + (1.0f - gs) * want;
                L[i] *= levGain; if (R != nullptr) R[i] *= levGain;
                levVerb[(size_t) i] = levGain;
            }
        }
        for (int i = 0; i < num; ++i)
        {
            const float vg = level ? levVerb[(size_t) i] : 1.0f;
            float l = (L[i] + verbBuf.getReadPointer (0)[i] * verbOut * vg) * set.master;
            l = std::tanh (l * 1.1f) * 0.92f;
            L[i] = l;
            peak = juce::jmax (peak, std::abs (l));
            if (R != nullptr)
            {
                float r = (R[i] + verbBuf.getReadPointer (1)[i] * verbOut * vg) * set.master;
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
        if (part == PChords || part == PPad) while (n > 76) n -= 12;
        return n;
    }

    void renderRange (int a, int b)
    {
        if (b <= a) return;
        sampler.render (buses, a, b);
        piano.render (buses, a, b);
    }

    double sr = 44100.0;
    long long clock = 0, now = 0;
    float levEnv = 0.0009f, levGain = 1.0f, levLp = 0.0f, levTarget = 0.03f;   // niveleur de volume POP WAVE
    std::vector<float> levVerb = std::vector<float> (8192, 1.0f);                       // temps en samples (coupure des notes POP WAVE)
    long long lastOn[NumParts] { -1000000, -1000000, -1000000, -1000000, -1000000, -1000000, -1000000 };
    Sampler sampler;
    PianoSynth piano;
    juce::AudioBuffer<float> buses, verbBuf, colBuf, bassBuf;
    juce::Reverb reverb;
    PopWaveColour colour;
    PopWaveMixStrip mix[NumParts];
    PopWaveBassColour bassColour;
    bool verbIsPopWave = false;
    float verbLp[2] {};
    Settings set;
    GenState state;
    float meter = 0.0f;
};
} // namespace ranti
