#include "MelodyGenerator.h"
#include "Scales.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// =============================================================================
//  Moteur "producteur" :
//   1. une progression d'accords (style rap FR melancolique / dark trap)
//   2. un MOTIF rythmique + melodique d'une mesure, ecrit en notes d'accord
//      (tonique, tierce, quinte, octave...) -> il se transpose sur chaque accord
//   3. le motif se repete ; seule la fin de phrase varie et retombe sur la tonique
//   4. une piste d'accords (+ basse) voice-leadee
// =============================================================================

namespace
{
struct Rng
{
    std::uint64_t s;
    explicit Rng (int seed) : s ((std::uint64_t) (std::uint32_t) seed * 0x9E3779B97F4A7C15ull + 0x1234567ull) {}
    std::uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (std::uint32_t) (s >> 16); }
    float nextFloat()      { return (float) (next() & 0xFFFFFF) / (float) 0x1000000; }
    int   nextInt (int n)  { return n <= 0 ? 0 : (int) (next() % (std::uint32_t) n); }
    bool  chance (float p) { return nextFloat() < p; }
    template <typename T> int pickWeighted (const T& w)
    {
        float total = 0; for (float x : w) total += std::max (0.0f, x);
        if (total <= 0) return 0;
        float r = nextFloat() * total;
        for (int i = 0; i < (int) w.size(); ++i) { r -= std::max (0.0f, w[(size_t) i]); if (r <= 0) return i; }
        return (int) w.size() - 1;
    }
};

// ---------- Rythmes de motif (1 mesure = 96 ticks, 24 par noire) ----------
// triolet de croches = 8, triolet de noires = 16, double = 6, croche = 12
struct RNote { int on, dur; };
struct RTemplate { std::vector<RNote> n; int feel; float density; };

const std::vector<RTemplate>& rhythms()
{
    static const std::vector<RTemplate> r = {
        // ---- TRIOLETS ----
        { { {0,8},{8,8},{16,8},{24,8},{32,8},{40,8},{48,8},{56,8},{64,8},{72,8},{80,8},{88,8} }, 0, 1.00f }, // arpege continu
        { { {0,8},{8,8},{16,8},{24,24},{48,8},{56,8},{64,8},{72,24} },                           0, 0.70f },
        { { {0,8},{8,8},{16,8},{24,8},{32,8},{40,8},{48,48} },                                   0, 0.55f },
        { { {0,16},{16,16},{32,16},{48,16},{64,16},{80,16} },                                    0, 0.50f }, // triolets de noires
        { { {0,24},{24,8},{32,8},{40,8},{48,24},{72,8},{80,8},{88,8} },                          0, 0.65f },
        { { {0,16},{16,8},{24,24},{48,16},{64,8},{72,24} },                                      0, 0.45f },
        { { {0,32},{32,16},{48,8},{56,8},{64,32} },                                              0, 0.35f },
        { { {0,8},{8,8},{16,32},{48,8},{56,8},{64,32} },                                         0, 0.45f },
        // ---- BINAIRE (piano / guitare) ----
        { { {0,18},{18,18},{36,12},{48,18},{66,18},{84,12} },                                    1, 0.50f }, // 3-3-2
        { { {0,12},{12,12},{24,12},{36,12},{48,12},{60,12},{72,12},{84,12} },                    1, 0.80f }, // croches
        { { {0,6},{6,6},{12,6},{18,6},{24,24},{48,6},{54,6},{60,6},{66,6},{72,24} },             1, 0.90f },
        { { {0,12},{12,6},{18,6},{24,24},{48,12},{60,12},{72,24} },                              1, 0.60f },
        { { {0,36},{36,12},{48,24},{72,12},{84,12} },                                            1, 0.35f },
        { { {0,18},{18,6},{24,12},{36,12},{48,18},{66,6},{72,24} },                              1, 0.60f },
        { { {0,24},{36,12},{48,24},{84,12} },                                                    1, 0.25f },
        { { {0,12},{12,12},{24,12},{36,12},{48,48} },                                            1, 0.45f },
        { { {12,12},{24,12},{36,12},{60,12},{72,12},{84,12} },                                   1, 0.55f },
    };
    return r;
}

// ---------- Progressions (degres de la gamme, 0 = I) ----------
struct Prog { const char* name; int deg[4]; };
const Prog progs[] = {
    { "i - VI - III - VII",  { 0, 5, 2, 6 } }, // le classique melancolique (Am F C G)
    { "i - VII - VI - V",    { 0, 6, 5, 4 } }, // andalouse
    { "VI - VII - i - i",    { 5, 6, 0, 0 } },
    { "i - iv - VI - V",     { 0, 3, 5, 4 } },
    { "i - VI - VII - i",    { 0, 5, 6, 0 } },
    { "i - VI - iv - V",     { 0, 5, 3, 4 } },
    { "i - v - VI - iv",     { 0, 4, 5, 3 } },
    { "i - III - VII - VI",  { 0, 2, 6, 5 } },
    { "i - iv - i - V",      { 0, 3, 0, 4 } },
    { "i - i - VI - VI",     { 0, 0, 5, 5 } },
    { "i - bII - i - bII",   { 0, 1, 0, 1 } }, // tres dark (phrygien)
    { "i - VI - i - VII",    { 0, 5, 0, 6 } },
};
constexpr int numProgs = (int) (sizeof (progs) / sizeof (progs[0]));
// progressions privilegiees en mode Auto (les plus "rap FR")
const int autoWeights[numProgs] = { 6, 4, 4, 4, 3, 3, 2, 2, 2, 2, 1, 2 };

// ---------- Tokens = notes relatives a l'accord ----------
enum Tok { ROOT = 0, THIRD, FIFTH, OCT, THIRD_OCT, FIFTH_LOW, UP, DOWN, NUM_TOK };

const std::vector<std::vector<int>>& arpShapes()
{
    static const std::vector<std::vector<int>> a = {
        { ROOT, FIFTH, OCT },          { ROOT, THIRD, FIFTH },        { OCT, FIFTH, THIRD },
        { ROOT, FIFTH, OCT, FIFTH },   { FIFTH, THIRD, ROOT },        { ROOT, OCT, FIFTH, THIRD },
        { THIRD, FIFTH, OCT },         { ROOT, THIRD, FIFTH, THIRD }, { FIFTH_LOW, ROOT, THIRD },
        { OCT, THIRD_OCT, FIFTH },     { ROOT, FIFTH, THIRD_OCT, OCT },
    };
    return a;
}

const char* noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

struct MNote { int on, dur, tok; bool oct; int vel; };
using Motif = std::vector<MNote>;

class Gen
{
public:
    explicit Gen (const GenParams& params)
        : p (params), rng (params.seed),
          iv (MelodyGenerator::getScaleIntervals (params.scaleIndex, params.rotation))
    {
        n    = (int) iv.size();
        base = 12 * (p.octave + 1) + p.root;
        lo   = -n;
        hi   = std::max (1, p.rangeOctaves) * n + n / 2;
    }

    GenResult run()
    {
        GenResult res;
        const int totalTicks = p.bars * 96;

        // ----- 1. progression -----
        int progIdx = p.progression - 1;
        if (progIdx < 0 || progIdx >= numProgs)
            progIdx = rng.pickWeighted (std::vector<float> (autoWeights, autoWeights + numProgs));
        const Prog& pr = progs[progIdx];

        // rythme harmonique : 1 accord par mesure (4/8 mesures), 2 par mesure (1/2 mesures)
        struct Slot { int start, end; Chord c; };
        std::vector<Slot> slots;
        if (p.bars >= 4)
        {
            for (int b = 0; b < p.bars; ++b)
                slots.push_back ({ b * 96, (b + 1) * 96, makeChord (pr.deg[b % 4]) });
        }
        else
        {
            const int count = p.bars * 2;
            for (int i = 0; i < count; ++i)
                slots.push_back ({ i * 48, (i + 1) * 48, makeChord (pr.deg[i % 4]) });
        }
        auto chordAt = [&] (int tick) -> const Chord&
        {
            for (auto& s : slots) if (tick >= s.start && tick < s.end) return s.c;
            return slots.back().c;
        };

        for (size_t i = 0; i < slots.size() && i < 4; ++i)
            res.chordNames.push_back (chordName (slots[i].c));

        // ----- 2. motif + variations -----
        Motif A  = makeMotif();
        Motif V1 = variation (A, true);
        Motif V2 = variation (A, true);

        std::vector<const Motif*> bars;
        for (int b = 0; b < p.bars; ++b)
        {
            const bool lastOfPhrase = (b % 4 == 3) || (b == p.bars - 1);
            if (! lastOfPhrase)                     bars.push_back (&A);
            else if (b == p.bars - 1)               bars.push_back (&V1);
            else                                    bars.push_back (&V2);
        }

        // ----- 3. rendu melodie -----
        int prevDeg = 0;
        for (int b = 0; b < p.bars; ++b)
        {
            for (const auto& m : *bars[(size_t) b])
            {
                const int tick = b * 96 + m.on;
                int d = tokDeg (m.tok, chordAt (tick), prevDeg);
                if (m.oct) d += n;
                while (d > hi) d -= n;
                while (d < lo) d += n;
                while (midiOf (d) > 127) d -= n;
                while (midiOf (d) < 0)   d += n;
                prevDeg = d;

                MelodyNote note;
                note.start    = tick / 24.0;
                note.length   = std::max (1.0 / 24.0, (m.dur / 24.0) * (double) p.gate);
                note.length   = std::min (note.length, totalTicks / 24.0 - note.start - 1.0e-3);
                note.pitch    = std::clamp (midiOf (d), 0, 127);
                note.velocity = std::clamp (m.vel, 1, 127);
                res.melody.push_back (note);
            }
        }

        // ----- 4. accords -----
        if (p.chordMode > 0)
            renderChords (res, slots);

        return res;
    }

private:
    struct Chord { int r, t3, t5; };

    const GenParams& p;
    Rng rng;
    std::vector<int> iv;
    int n = 7, base = 57, lo = -7, hi = 17;

    static int floorDiv (int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
    int semiOf (int d) const { int o = floorDiv (d, n); return o * 12 + iv[(size_t) (d - o * n)]; }
    int midiOf (int d) const { return base + semiOf (d); }

    // degre de progression (pense en mineur heptatonique) -> degre de la gamme choisie
    int mapDegree (int deg) const
    {
        if (n == 7) return deg;
        static const int minorSemis[7] = { 0, 2, 3, 5, 7, 8, 10 };
        int best = 0, bestDist = 99;
        for (int i = 0; i < n; ++i)
        {
            int dist = std::abs (iv[(size_t) i] - minorSemis[deg]);
            if (dist < bestDist) { bestDist = dist; best = i; }
        }
        return best;
    }

    Chord makeChord (int progDeg)
    {
        Chord c;
        c.r = mapDegree (progDeg);
        if (c.r > n / 2) c.r -= n; // accords graves sous la tonique (VI, VII, V...)
        float best3 = 99, best5 = 99;
        c.t3 = 1; c.t5 = std::min (2, n - 1);
        for (int s = 1; s < n; ++s)
        {
            const int dist = semiOf (c.r + s) - semiOf (c.r);
            const float s3 = std::abs ((float) dist - 3.4f);
            if (s3 < best3) { best3 = s3; c.t3 = s; }
        }
        for (int s = 1; s < n; ++s)
        {
            if (s == c.t3) continue;
            const int dist = semiOf (c.r + s) - semiOf (c.r);
            const float s5 = std::abs ((float) dist - 7.0f) + (s < c.t3 ? 2.0f : 0.0f);
            if (s5 < best5) { best5 = s5; c.t5 = s; }
        }
        return c;
    }

    std::string chordName (const Chord& c) const
    {
        const int pc = ((p.root + semiOf (c.r)) % 12 + 12) % 12;
        const int t = semiOf (c.r + c.t3) - semiOf (c.r);
        const int f = semiOf (c.r + c.t5) - semiOf (c.r);
        std::string q;
        if      (t == 3 && f == 7) q = "m";
        else if (t == 4 && f == 7) q = "";
        else if (t == 3 && f == 6) q = "dim";
        else if (t == 4 && f == 8) q = "aug";
        else if (t == 2)           q = "sus2";
        else if (t == 5)           q = "sus4";
        else q = (t == 3 ? "m" : "") + std::string (f == 6 ? "b5" : f == 8 ? "#5" : "");
        return std::string (noteNames[pc]) + q;
    }

    int tokDeg (int tok, const Chord& c, int prevDeg) const
    {
        switch (tok)
        {
            case ROOT:      return c.r;
            case THIRD:     return c.r + c.t3;
            case FIFTH:     return c.r + c.t5;
            case OCT:       return c.r + n;
            case THIRD_OCT: return c.r + n + c.t3;
            case FIFTH_LOW: return c.r + c.t5 - n;
            case UP:        return prevDeg + 1;
            case DOWN:      return prevDeg - 1;
            default:        return c.r;
        }
    }

    int velFor (int on)
    {
        if (on % 24 == 0) return 98 + rng.nextInt (14);
        if (on % 12 == 0 || on % 8 == 0) return 82 + rng.nextInt (14);
        return 70 + rng.nextInt (14);
    }

    int pickTok (int prevTok, bool isLong, bool isLast)
    {
        std::vector<float> w = { 3.0f, 2.2f, 2.6f, 1.6f, 0.7f, 0.9f,
                                 0.6f + 0.9f * p.darkness, 0.6f + 0.9f * p.darkness };
        if (isLong || isLast || prevTok == UP || prevTok == DOWN) { w[UP] = 0; w[DOWN] = 0; }
        if (isLast) { w[ROOT] *= 2.5f; w[FIFTH] *= 1.5f; w[THIRD_OCT] *= 0.3f; }
        if (p.rangeOctaves == 1) { w[OCT] *= 0.3f; w[THIRD_OCT] *= 0.2f; }
        if (prevTok >= 0 && prevTok < NUM_TOK) w[(size_t) prevTok] *= 0.4f;
        return rng.pickWeighted (w);
    }

    Motif makeMotif()
    {
        // choix du rythme selon le style et la densite
        const auto& rs = rhythms();
        std::vector<float> w (rs.size(), 0.0f);
        for (size_t i = 0; i < rs.size(); ++i)
        {
            if (p.feel != 2 && rs[i].feel != p.feel) continue;
            const float d = rs[i].density - p.density;
            w[i] = std::exp (-(d * d) / 0.06f) + 0.02f;
        }
        const auto& t = rs[(size_t) rng.pickWeighted (w)];

        Motif m;
        for (size_t i = 0; i < t.n.size(); ++i)
        {
            if (i > 0 && t.n.size() > 3 && rng.chance (p.rests * 0.45f))
                continue; // silence
            m.push_back ({ t.n[i].on, t.n[i].dur, ROOT, false, 100 });
        }

        // notes : arpege (le plus "trap") ou ligne melodique sur les notes de l'accord
        const bool arp = rng.chance (0.45f + 0.3f * p.density);
        const auto& shapes = arpShapes();
        const auto& shape = shapes[(size_t) rng.nextInt ((int) shapes.size())];
        int prevTok = -1;
        for (size_t i = 0; i < m.size(); ++i)
        {
            auto& x = m[i];
            const bool isLong = x.dur >= 24;
            const bool isLast = i + 1 == m.size();
            if (arp)
            {
                x.tok = shape[i % shape.size()];
                // notes de passage sombres (b2 / b6...) entre deux notes d'accord
                if (i > 0 && ! isLong && ! isLast && prevTok != UP && prevTok != DOWN
                    && rng.chance (0.08f + 0.2f * p.darkness))
                    x.tok = rng.chance (0.5f) ? UP : DOWN;
            }
            else
            {
                x.tok = (i == 0) ? (rng.chance (0.6f) ? ROOT : FIFTH) : pickTok (prevTok, isLong, isLast);
            }
            x.oct = i > 0 && x.tok != UP && x.tok != DOWN && rng.chance (p.octaveJumps * 0.35f);
            x.vel = velFor (x.on);
            prevTok = x.tok;
        }
        return m;
    }

    // fin de phrase : on garde le debut, la 2e moitie change et retombe sur la tonique
    Motif variation (const Motif& src, bool resolve)
    {
        Motif v;
        int prevTok = -1;
        for (auto& x : src) if (x.on < 48) { v.push_back (x); prevTok = x.tok; }

        const bool roll = rng.chance (p.rolls * 0.6f);
        if (roll)
        {
            // garde le 3e temps, roll sur le 4e
            for (auto& x : src) if (x.on >= 48 && x.on < 72) v.push_back ({ x.on, std::min (x.dur, 72 - x.on), x.tok, x.oct, x.vel });
            const bool trip = (p.feel == 0) || (p.feel == 2 && rng.chance (0.5f));
            const int step = trip ? 4 : 3;
            int k = 0;
            for (int t = 72; t < 96; t += step, ++k)
                v.push_back ({ t, step, (k % 2 == 0 || rng.chance (0.6f)) ? ROOT : FIFTH, false, 70 + k * 5 });
            return v;
        }

        Motif tail = makeMotif();
        for (auto& x : tail)
            if (x.on >= 48) v.push_back (x);
        if (v.empty()) v.push_back ({ 0, 48, ROOT, false, 100 });

        if (resolve)
        {
            auto& last = v.back();
            last.tok = ROOT;
            last.oct = false;
            if (96 - last.on <= 48) last.dur = 96 - last.on;
            last.vel = std::max (last.vel, 96);
            (void) prevTok;
        }
        return v;
    }

    template <typename SlotVec>
    void renderChords (GenResult& res, const SlotVec& slots)
    {
        int prevCenter = base - 12 + 4;
        for (auto& s : slots)
        {
            const auto& c = s.c;
            // triade autour d'une octave sous la melodie, inversion la plus proche (voice leading)
            int tones[3] = { midiOf (c.r) - 12, midiOf (c.r + c.t3) - 12, midiOf (c.r + c.t5) - 12 };
            int bestInv[3] = { tones[0], tones[1], tones[2] };
            int bestScore = 1 << 30;
            for (int inv = -1; inv <= 2; ++inv)
            {
                int v[3] = { tones[0], tones[1], tones[2] };
                if (inv < 0) v[2] -= 12;
                for (int k = 0; k < inv; ++k) v[k] += 12;
                const int center = (v[0] + v[1] + v[2]) / 3;
                const int score = std::abs (center - prevCenter);
                if (score < bestScore) { bestScore = score; std::copy (v, v + 3, bestInv); }
            }
            prevCenter = (bestInv[0] + bestInv[1] + bestInv[2]) / 3;
            const int bass = midiOf (c.r) - 24;

            const double st = s.start / 24.0, len = (s.end - s.start) / 24.0;
            auto add = [&] (double start, double l, int pitch, int vel)
            {
                if (pitch < 0 || pitch > 127) return;
                res.chords.push_back ({ start, l, pitch, vel });
            };

            add (st, len * 0.98, bass, 90);
            if (p.chordMode == 1)
            {
                for (int k = 0; k < 3; ++k) add (st, len * 0.98, bestInv[k], 72);
            }
            else
            {
                // accords rythmes : un coup long + un rappel, par demi-mesure
                const bool trip = p.feel != 1;
                for (int h = s.start; h < s.end; h += 48)
                {
                    const double a = h / 24.0;
                    const double second = trip ? (32 / 24.0) : (36 / 24.0);
                    for (int k = 0; k < 3; ++k)
                    {
                        add (a, second * 0.9, bestInv[k], 78);
                        add (a + second, (2.0 - second) * 0.85, bestInv[k], 64);
                    }
                }
            }
        }
        std::sort (res.chords.begin(), res.chords.end(),
                   [] (const MelodyNote& a, const MelodyNote& b) { return a.start < b.start; });
    }
};
} // namespace

std::vector<int> MelodyGenerator::getScaleIntervals (int scaleIndex, int rotation)
{
    const auto& scales = getScales();
    scaleIndex = std::clamp (scaleIndex, 0, (int) scales.size() - 1);
    const auto& src = scales[(size_t) scaleIndex].intervals;
    const int n = (int) src.size();
    const int r = ((rotation % n) + n) % n;
    std::vector<int> out;
    out.reserve ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        int v = src[(size_t) ((i + r) % n)] - src[(size_t) r];
        if (v < 0) v += 12;
        out.push_back (v);
    }
    return out;
}

std::vector<std::string> MelodyGenerator::getProgressionNames()
{
    std::vector<std::string> v { "Auto" };
    for (auto& pr : progs) v.push_back (pr.name);
    return v;
}

GenResult MelodyGenerator::generate (const GenParams& p)
{
    Gen g (p);
    return g.run();
}
