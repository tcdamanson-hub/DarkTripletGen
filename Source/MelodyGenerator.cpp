#include "MelodyGenerator.h"
#include "Scales.h"
#include "LearnedData.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// =============================================================================
//  Moteur v3 "topline" (style rap FR : Ninho, Werenoi, dark trap)
//
//  1. Progression d'accords (ponderee selon le style)
//  2. Phrase de 2 mesures planifiee temps par temps :
//       - une NOTE CIBLE (note de l'accord) sur chaque temps, choisie selon un
//         CONTOUR (arche, descente, vague...) et proche de la note precedente
//       - entre deux cibles : FIGURES (arpege, passage en gamme, broderie,
//         repetition, saut d'octave) qui menent vers la cible suivante
//       - appoggiatures (tension sur le temps qui se resout), anticipations
//       - rythme continu : chaque note tient jusqu'a la suivante
//  3. Forme : question (2 mes) -> reponse (meme phrase sur les accords suivants,
//     fin re-ecrite qui se pose) ; en 8 mesures : 2e moitie ornementee + climax
//  4. Accords + basse voice-leades (7emes en style Ninho)
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
        for (int i = 0; i < (int) w.size(); ++i) { r -= std::max (0.0f, w[(size_t) i]); if (r <= 0 && w[(size_t) i] > 0) return i; }
        for (int i = (int) w.size() - 1; i >= 0; --i) if (w[(size_t) i] > 0) return i;
        return (int) w.size() - 1;
    }
};

enum Style { CUSTOM = 0, NINHO, WERENOI, TRAPDARK, TRIOLETS };

// ---------- Progressions (degres de la gamme, 0 = I) ----------
struct Prog { const char* name; int deg[4]; };
const Prog progs[] = {
    { "i - VI - III - VII",  { 0, 5, 2, 6 } },
    { "i - VII - VI - V",    { 0, 6, 5, 4 } },
    { "VI - VII - i - i",    { 5, 6, 0, 0 } },
    { "i - iv - VI - V",     { 0, 3, 5, 4 } },
    { "i - VI - VII - i",    { 0, 5, 6, 0 } },
    { "i - VI - iv - V",     { 0, 5, 3, 4 } },
    { "i - v - VI - iv",     { 0, 4, 5, 3 } },
    { "i - III - VII - VI",  { 0, 2, 6, 5 } },
    { "i - iv - i - V",      { 0, 3, 0, 4 } },
    { "i - i - VI - VI",     { 0, 0, 5, 5 } },
    { "i - bII - i - bII",   { 0, 1, 0, 1 } },
    { "i - VI - i - VII",    { 0, 5, 0, 6 } },
};
constexpr int numProgs = (int) (sizeof (progs) / sizeof (progs[0]));
const float progWeights[5][numProgs] = {
    { 6, 4, 4, 4, 3, 3, 2, 2, 2, 2, 1, 2 },   // custom
    { 8, 2, 6, 2, 5, 2, 3, 5, 1, 2, 0, 3 },   // Ninho : melancolique
    { 3, 7, 2, 6, 3, 6, 3, 2, 4, 3, 2, 3 },   // Werenoi : cadences en V (mineur harmonique)
    { 2, 3, 2, 2, 2, 2, 2, 1, 3, 6, 6, 5 },   // Trap dark : statique / phrygien
    { 6, 5, 3, 4, 3, 4, 2, 2, 3, 3, 2, 3 },   // Triolets
};

// ---------- Cellules rythmiques d'UN temps (24 ticks) ----------
struct Cell { std::vector<int> on; float base, dens, runs; };

const std::vector<Cell>& tripCells()
{
    static const std::vector<Cell> c = {
        { { 0, 8, 16 },             4.0f,  0.5f, 0.0f },
        { { 0, 16 },                1.2f, -1.0f, 0.0f },
        { { 0 },                    0.5f, -1.5f, 0.0f },
        { { 8, 16 },                0.5f,  0.0f, 0.0f }, // syncope : la note d'avant tient
        { { 0, 8, 16, 20 },         0.0f,  0.0f, 2.5f },
        { { 0, 4, 8, 16 },          0.0f,  0.0f, 2.0f },
        { { 0, 8, 12, 16, 20 },     0.0f,  0.0f, 1.5f },
        { { 0, 4, 8, 12, 16, 20 },  0.0f,  0.0f, 1.0f },
    };
    return c;
}

const std::vector<Cell>& straightCells()
{
    static const std::vector<Cell> c = {
        { { 0, 6, 12, 18 },               3.0f,  1.0f, 0.0f },
        { { 0, 12, 18 },                  2.2f,  0.3f, 0.0f },
        { { 0, 6, 12 },                   1.5f,  0.3f, 0.0f },
        { { 0, 12 },                      1.5f, -1.0f, 0.0f },
        { { 0, 18 },                      0.8f, -0.5f, 0.0f },
        { { 6, 12, 18 },                  0.6f,  0.0f, 0.0f }, // syncope
        { { 0 },                          0.4f, -1.5f, 0.0f },
        { { 0, 3, 6, 12, 18 },            0.0f,  0.0f, 2.0f },
        { { 0, 6, 12, 15, 18, 21 },       0.0f,  0.0f, 1.8f },
        { { 0, 3, 6, 9, 12, 15, 18, 21 }, 0.0f,  0.0f, 0.8f },
    };
    return c;
}

// ---------- Figures melodiques ----------
enum Fig { F_ARP = 0, F_PASS, F_TURN, F_REPEAT, F_LEAP, F_NUM };
const float figWeights[5][F_NUM] = {
    { 3.0f, 3.0f, 1.5f, 1.5f, 0.6f },  // custom
    { 4.0f, 2.5f, 1.0f, 2.0f, 0.4f },  // Ninho : arpeges guitare/piano + notes repetees
    { 2.0f, 4.0f, 2.5f, 0.8f, 1.0f },  // Werenoi : lignes en gamme, broderies (violon/piano)
    { 3.0f, 1.5f, 1.0f, 3.0f, 1.5f },  // Trap dark : repetitions, sauts
    { 4.0f, 3.0f, 1.5f, 1.0f, 0.8f },  // Triolets
};

const char* noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

struct BeatPlan
{
    std::vector<int> on;
    int feel = 0;          // 0 triolet, 1 binaire
    int fig = F_ARP;
    float h = 0.5f;        // hauteur voulue (contour) 0..1
    bool appog = false;    // appoggiature sur le temps
    bool antic = false;    // anticipation de l'accord suivant
};
using Plan = std::vector<BeatPlan>;

struct GNote { int tick; int deg; int vel; int midi; };

class Gen
{
public:
    explicit Gen (const GenParams& params)
        : p (params), style (std::clamp (params.style, 0, 4)), rng (params.seed),
          iv (MelodyGenerator::getScaleIntervals (params.scaleIndex, params.rotation))
    {
        n    = (int) iv.size();
        base = 12 * (p.octave + 1) + p.root;
        loM  = -2;
        hiM  = p.rangeOctaves <= 1 ? n + 2 : (p.rangeOctaves == 2 ? n + 5 : 2 * n + 4);
    }

    GenResult run()
    {
        if (p.motif == 0)
            return runLearned();

        GenResult res;
        totalTicks = p.bars * 96;

        // ----- 1. progression -----
        int progIdx = p.progression - 1;
        if (progIdx < 0 || progIdx >= numProgs)
            progIdx = rng.pickWeighted (std::vector<float> (progWeights[style], progWeights[style] + numProgs));
        const Prog& pr = progs[progIdx];

        if (p.bars >= 4)
            for (int b = 0; b < p.bars; ++b)
                slots.push_back ({ b * 96, (b + 1) * 96, makeChord (pr.deg[b % 4]) });
        else
            for (int i = 0; i < p.bars * 2; ++i)
                slots.push_back ({ i * 48, (i + 1) * 48, makeChord (pr.deg[i % 4]) });

        for (size_t i = 0; i < slots.size() && i < 4; ++i)
            res.chordNames.push_back (chordName (slots[i].c));

        // ----- 2. forme -----
        std::vector<GNote> notes;
        int prev = 0;
        if (p.bars == 1)
        {
            Plan P = makePlan (4);
            render (P, 0, 2, prev, notes);
        }
        else if (p.bars == 2)
        {
            Plan P = makePlan (8);
            render (P, 0, 2, prev, notes);
        }
        else
        {
            Plan P = makePlan (8);                 // question
            Plan R = P; reroll (R, 5);             // reponse : meme debut, fin re-ecrite
            render (P, 0, 0, prev, notes);
            render (R, 8, p.bars == 4 ? 2 : 1, prev, notes);
            if (p.bars == 8)
            {
                Plan P2 = ornament (P, 0.12f);     // reprise ornementee, plus haute (climax)
                Plan R2 = P2; reroll (R2, 5);
                render (P2, 16, 0, prev, notes);
                render (R2, 24, 2, prev, notes);
            }
        }

        // ----- 3. notes -> MIDI (legato : chaque note tient jusqu'a la suivante) -----
        std::sort (notes.begin(), notes.end(), [] (const GNote& a, const GNote& b) { return a.tick < b.tick; });
        notes.erase (std::unique (notes.begin(), notes.end(),
                                  [] (const GNote& a, const GNote& b) { return a.tick == b.tick; }),
                     notes.end());
        for (size_t i = 0; i < notes.size(); ++i)
        {
            const int nextTick = (i + 1 < notes.size()) ? notes[i + 1].tick : totalTicks;
            MelodyNote m;
            m.start    = notes[i].tick / 24.0;
            m.length   = std::max (1.0 / 24.0, ((nextTick - notes[i].tick) / 24.0) * (double) p.gate);
            m.length   = std::min (m.length, totalTicks / 24.0 - m.start - 1.0e-3);
            int mp = notes[i].midi;
            while (mp > 127) mp -= 12;
            while (mp < 0)   mp += 12;
            m.pitch    = mp;
            m.velocity = std::clamp (notes[i].vel, 1, 127);
            res.melody.push_back (m);
        }

        // ----- 4. accords -----
        if (p.chordMode > 0)
            renderChords (res);

        return res;
    }

private:
    struct Chord { int r = 0, t3 = 2, t5 = 4, t7 = 6, altIdx = -1; }; // altIdx : degre hausse d'1/2 ton (sensible)
    struct Slot { int start, end; Chord c; };

    const GenParams& p;
    int style;
    Rng rng;
    std::vector<int> iv;
    std::vector<Slot> slots;
    int n = 7, base = 57, loM = -2, hiM = 13, totalTicks = 384;

    // ---------- outils gamme ----------
    static int floorDiv (int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
    static int mod (int a, int b) { return ((a % b) + b) % b; }
    int semiOf (int d) const { int o = floorDiv (d, n); return o * 12 + iv[(size_t) (d - o * n)]; }
    int midiOf (int d) const { return base + semiOf (d); }
    int semiC (int d, const Chord& c) const
    {
        const int o = floorDiv (d, n), idx = d - o * n;
        return o * 12 + iv[(size_t) idx] + (idx == c.altIdx ? 1 : 0);
    }
    int midiC (int d, const Chord& c) const { return base + semiC (d, c); }
    bool isAeolian() const
    {
        static const int a[7] = { 0, 2, 3, 5, 7, 8, 10 };
        if (n != 7) return false;
        for (int i = 0; i < 7; ++i) if (iv[(size_t) i] != a[i]) return false;
        return true;
    }

    const Chord& chordAt (int tick) const
    {
        tick = mod (tick, totalTicks);
        for (auto& s : slots) if (tick >= s.start && tick < s.end) return s.c;
        return slots.back().c;
    }

    bool isChordTone (const Chord& c, int d) const
    {
        const int k = mod (d - c.r, n);
        return k == 0 || k == c.t3 || k == c.t5;
    }

    int nextChordTone (const Chord& c, int from, int dir) const
    {
        for (int d = from + dir, i = 0; i < 2 * n; d += dir, ++i)
            if (isChordTone (c, d)) return d;
        return from + dir;
    }

    int fold (int d) const
    {
        while (d > hiM + 3) d -= n;
        while (d < loM - 3) d += n;
        return d;
    }

    int mapDegree (int deg) const
    {
        if (n == 7) return deg;
        static const int minorSemis[7] = { 0, 2, 3, 5, 7, 8, 10 };
        int best = 0, bestDist = 99;
        for (int i = 0; i < n; ++i)
        {
            const int dist = std::abs (iv[(size_t) i] - minorSemis[deg]);
            if (dist < bestDist) { bestDist = dist; best = i; }
        }
        return best;
    }

    Chord makeChord (int progDeg) const
    {
        Chord c;
        c.r = mapDegree (progDeg);
        if (c.r > n / 2) c.r -= n;
        auto closest = [&] (float target, int exclude1, int exclude2, int minStep)
        {
            float best = 99; int bs = 1;
            for (int s = std::max (1, minStep); s < n; ++s)
            {
                if (s == exclude1 || s == exclude2) continue;
                const float d = std::abs ((float) (semiOf (c.r + s) - semiOf (c.r)) - target);
                if (d < best) { best = d; bs = s; }
            }
            return bs;
        };
        if (n == 7)
        {
            c.t3 = 2; c.t5 = 4; c.t7 = 6; // empilement de tierces de la gamme
            // mineur "a la Werenoi" : accord de V majeur (sensible) sur une gamme mineure naturelle
            if (progDeg == 4 && isAeolian() && (style == WERENOI || style == TRIOLETS))
                c.altIdx = 6;
        }
        else
        {
            c.t3 = closest (3.4f, -1, -1, 1);
            c.t5 = closest (7.0f, c.t3, -1, c.t3 + 1);
            c.t7 = closest (10.6f, c.t3, c.t5, c.t5 + 1);
        }
        return c;
    }

    std::string chordName (const Chord& c) const
    {
        const int pc = mod (p.root + semiC (c.r, c), 12);
        const int t = semiC (c.r + c.t3, c) - semiC (c.r, c);
        const int f = semiC (c.r + c.t5, c) - semiC (c.r, c);
        std::string q;
        if      (t == 3 && f == 7) q = "m";
        else if (t == 4 && f == 7) q = "";
        else if (t == 3 && f == 6) q = "dim";
        else if (t == 4 && f == 8) q = "aug";
        else if (t == 2)           q = "sus2";
        else if (t == 5)           q = "sus4";
        else q = (t == 3 ? "m" : "") + std::string (f == 6 ? "b5" : f == 8 ? "#5" : "");
        if (use7th())
        {
            const int s = semiC (c.r + c.t7, c) - semiC (c.r, c);
            if (s == 10) q += "7";
            else if (s == 11) q += "maj7";
        }
        return std::string (noteNames[pc]) + q;
    }

    bool use7th() const { return style == NINHO && n == 7; }

    // ---------- plan rythmique / melodique ----------
    float contour (int shape, float x) const
    {
        const float pi = 3.14159265f;
        switch (shape)
        {
            case 0:  return 0.25f + 0.65f * std::sin (pi * std::min (1.0f, x * 1.15f));       // arche
            case 1:  return 0.90f - 0.75f * x;                                                 // descente
            case 2:  return 0.50f + 0.25f * std::sin (2.0f * pi * x * 1.5f) - 0.15f * x;       // vague
            case 3:  return 0.30f + 0.60f * std::sin (pi * std::pow (x, 1.6f));                // sommet tardif
            default: return 0.60f + 0.30f * std::sin (pi * x) - 0.35f * x;                    // haut puis pose
        }
    }

    BeatPlan makeBeat (int index, float h)
    {
        BeatPlan b;
        b.feel = (p.feel == 2) ? (rng.chance (0.55f) ? 0 : 1) : p.feel;
        const auto& cells = b.feel == 0 ? tripCells() : straightCells();
        std::vector<float> w (cells.size());
        for (size_t i = 0; i < cells.size(); ++i)
        {
            const auto& c = cells[i];
            float x = c.base * std::max (0.05f, 1.0f + c.dens * (p.density - 0.5f) * 2.0f) + c.runs * p.rolls;
            if (c.on.empty() || c.on[0] != 0)
                x = (index == 0) ? 0.0f : x * (0.4f + p.rests * 2.0f);   // syncopes
            w[i] = x;
        }
        b.on = cells[(size_t) rng.pickWeighted (w)].on;

        std::vector<float> fw (figWeights[style], figWeights[style] + F_NUM);
        fw[F_LEAP] *= 0.3f + 2.0f * p.octaveJumps;
        fw[F_PASS] *= 0.7f + 0.6f * p.darkness;
        b.fig = rng.pickWeighted (fw);

        float appogP = 0.04f + 0.28f * p.darkness;
        if (style == WERENOI) appogP *= 1.5f;
        b.appog = b.on.size() >= 2 && b.on[0] == 0 && rng.chance (appogP);
        b.antic = rng.chance (0.3f);
        b.h = std::clamp (h + (rng.nextFloat() - 0.5f) * 0.12f, 0.0f, 1.0f);
        return b;
    }

    Plan makePlan (int numBeats)
    {
        Plan plan;
        const int shape = rng.nextInt (5);
        for (int i = 0; i < numBeats; ++i)
            plan.push_back (makeBeat (i, contour (shape, (i + 0.5f) / (float) numBeats)));
        return plan;
    }

    void reroll (Plan& plan, int fromBeat)
    {
        for (int i = fromBeat; i < (int) plan.size(); ++i)
            plan[(size_t) i] = makeBeat (i, plan[(size_t) i].h);
    }

    Plan ornament (const Plan& src, float lift)
    {
        Plan out = src;
        for (auto& b : out)
        {
            b.h = std::clamp (b.h + lift, 0.0f, 1.0f);
            if (rng.chance (0.3f + 0.4f * p.rolls))
            {
                const auto& cells = b.feel == 0 ? tripCells() : straightCells();
                std::vector<float> w (cells.size());
                for (size_t i = 0; i < cells.size(); ++i)
                    w[i] = (cells[i].runs > 0 && cells[i].on[0] == 0) ? cells[i].runs : 0.0f;
                b.on = cells[(size_t) rng.pickWeighted (w)].on;
            }
        }
        return out;
    }

    int chooseTarget (const Chord& c, float h, int prev)
    {
        const float desired = (float) loM + h * (float) (hiM - loM);
        int best = c.r; float bestScore = 1e9f;
        for (int o = -2; o <= 3; ++o)
        {
            for (int t : { c.r, c.r + c.t3, c.r + c.t5 })
            {
                const int d = t + o * n;
                if (d < loM - 2 || d > hiM + 2) continue;
                float score = std::abs ((float) d - desired) + 0.45f * (float) std::abs (d - prev)
                            + (d == prev ? 0.6f : 0.0f) + rng.nextFloat() * 0.5f;
                if (score < bestScore) { bestScore = score; best = d; }
            }
        }
        return best;
    }

    int nearestChordTone (const Chord& c, int around) const
    {
        for (int k = 0; k < n; ++k)
        {
            if (isChordTone (c, around - k)) return around - k;
            if (isChordTone (c, around + k)) return around + k;
        }
        return around;
    }

    int velFor (int tick, bool accent)
    {
        const int on = mod (tick, 24);
        int v;
        if (on == 0)                     v = 94 + rng.nextInt (14);
        else if (on % 8 == 0 || on % 6 == 0) v = 80 + rng.nextInt (14);
        else                             v = 66 + rng.nextInt (14);
        return v + (accent ? 8 : 0);
    }

    // endMode : 0 = continue, 1 = demi-cadence (pose sur l'accord), 2 = fin (pose vers la tonique)
    void render (const Plan& plan, int startBeat, int endMode, int& prevDeg, std::vector<GNote>& out)
    {
        const int N = (int) plan.size();
        std::vector<int> tg ((size_t) N + 1);
        int prev = prevDeg;
        for (int i = 0; i < N; ++i)
        {
            tg[(size_t) i] = chooseTarget (chordAt ((startBeat + i) * 24), plan[(size_t) i].h, prev);
            prev = tg[(size_t) i];
        }
        if (endMode > 0)
        {
            const Chord& c = chordAt ((startBeat + N - 1) * 24);
            int anchor = prevDeg;
            if (N >= 2) anchor = tg[(size_t) N - 2];
            if (endMode == 2)
            {
                // note de l'accord la plus proche de la tonique (sensible, tonique...)
                const int tonic = (int) std::lround ((float) anchor / (float) n) * n;
                tg[(size_t) N - 1] = nearestChordTone (c, tonic);
            }
            else
            {
                tg[(size_t) N - 1] = nearestChordTone (c, (int) std::lround ((float) (anchor - c.r) / (float) n) * n + c.r);
            }
        }
        tg[(size_t) N] = chooseTarget (chordAt ((startBeat + N) * 24), plan[0].h, tg[(size_t) N - 1]);

        for (int i = 0; i < N; ++i)
        {
            const auto& b = plan[(size_t) i];
            const int k = (int) b.on.size();
            if (k == 0) continue;
            const int beatTick = (startBeat + i) * 24;
            const Chord& c = chordAt (beatTick);
            const Chord& cNext = chordAt (beatTick + 24);
            const bool chordChanges = cNext.r != c.r;
            const int t = tg[(size_t) i], nextT = tg[(size_t) i + 1];
            std::vector<int> degs ((size_t) k);

            if (endMode > 0 && i == N - 1)
            {
                // descente en gamme qui se pose sur la cible
                for (int j = 0; j < k; ++j) degs[(size_t) j] = t + (k - 1 - j);
                if (k >= 3 && rng.chance (0.4f)) degs[0] = t + n; // envolee puis descente
            }
            else
            {
                degs[0] = b.appog ? t + 1 : t;
                int cur = t;
                for (int j = 1; j < k; ++j)
                {
                    const bool last = j == k - 1;
                    if (b.appog && j == 1) { degs[1] = t; cur = t; continue; }
                    int dir = nextT > cur ? 1 : (nextT < cur ? -1 : ((j % 2) ? 1 : -1));
                    switch (b.fig)
                    {
                        case F_PASS:
                            cur += dir;
                            break;
                        case F_ARP:
                            cur = nextChordTone (c, cur, dir);
                            break;
                        case F_TURN:
                        {
                            static const int offs[4] = { 1, 0, -1, 0 };
                            cur = t + offs[(j - 1) % 4];
                            break;
                        }
                        case F_REPEAT:
                            cur = (last && k > 2) ? t + dir : t;
                            break;
                        case F_LEAP:
                        default:
                            cur = (j == 1) ? t + n : nextChordTone (c, cur, -1);
                            break;
                    }
                    if (last && b.antic && chordChanges && k >= 2)
                        cur = nextT;   // anticipation de l'accord suivant
                    cur = fold (cur);
                    degs[(size_t) j] = cur;
                }
            }

            for (int j = 0; j < k; ++j)
            {
                const int tick = beatTick + b.on[(size_t) j];
                const bool accent = (j == 0 && b.appog) || (endMode > 0 && i == N - 1 && j == k - 1);
                const int d = fold (degs[(size_t) j]);
                out.push_back ({ tick, d, velFor (tick, accent), midiC (d, chordAt (tick)) });
            }
        }
        prevDeg = tg[(size_t) N - 1];
    }


    // =========================================================================
    //  MOTEUR "APPRIS" : dessins d'arpege extraits des MIDI d'exemple
    // =========================================================================
    struct LNote { int on, dur, step, vel; };
    using LBar = std::vector<LNote>;
    struct PNote { int tick, dur, midi, vel; };

    LBar tplNotes (int idx) const
    {
        const auto& t = learned::templates[idx];
        LBar b;
        for (int i = 0; i < t.count; ++i)
        {
            const auto& x = learned::notes[t.start + i];
            b.push_back ({ x.on, x.dur, x.step, x.vel });
        }
        return b;
    }

    int pickTemplate (int wantPos, int src, int excludeSrc, bool strictFeel, bool useBank = true)
    {
        if (useBank && p.bank > 0)
        {
            const int r = pickTemplateImpl (wantPos, src, excludeSrc, strictFeel, true);
            if (r >= 0 || src >= 0) return r;
        }
        return pickTemplateImpl (wantPos, src, excludeSrc, strictFeel, false);
    }

    int pickTemplateImpl (int wantPos, int src, int excludeSrc, bool strictFeel, bool bankOnly)
    {
        std::vector<float> w ((size_t) learned::numTemplates, 0.0f);
        const float target = 4.0f + p.density * 20.0f;
        float tot = 0.0f;
        for (int i = 0; i < learned::numTemplates; ++i)
        {
            const auto& t = learned::templates[i];
            // banque 4 "Melodic" = les variations fabriquees, quel que soit le rythme ; les autres banques ne les utilisent jamais
            if (p.bank == 4) { if (! t.derived) continue; }
            else
            {
                if (t.derived) continue;
                if (strictFeel && p.feel != 2 && t.feel != p.feel) continue;
            }
            if (src >= 0 && t.src != src) continue;
            if (excludeSrc >= 0 && t.src == excludeSrc) continue;
            if (bankOnly && p.bank != 4 && (int) learned::srcBank[t.src] != p.bank) continue;
            const float d = (float) t.count - target;
            float x = std::exp (-(d * d) / 72.0f) + 0.02f;
            if (wantPos >= 0 && t.pos == wantPos) x *= 2.5f;
            w[(size_t) i] = x;
            tot += x;
        }
        if (tot <= 0.0f)
            return strictFeel ? pickTemplateImpl (wantPos, src, excludeSrc, false, bankOnly) : -1;
        return rng.pickWeighted (w);
    }

    static LBar splice (const LBar& a, const LBar& b)
    {
        LBar o;
        for (auto& x : a) if (x.on < 48) o.push_back ({ x.on, std::min (x.dur, 96), x.step, x.vel });
        for (auto& x : b) if (x.on >= 48) o.push_back (x);
        return o;
    }

    static int minStep (const LBar& b)
    {
        int m = 999;
        for (auto& x : b) m = std::min (m, x.step);
        return m;
    }

    void mutate (LBar& bar, float amount)
    {
        if (bar.empty()) return;
        const int low = minStep (bar);
        LBar out;
        for (auto x : bar)
        {
            const bool bass = x.step == low && x.on < 6;
            if (! bass && rng.chance (p.rests * 0.35f * amount) && bar.size() > 3)
                continue;                                      // respiration
            if (! bass && rng.chance (p.octaveJumps * 0.25f * amount))
                x.step += (x.step > 10 ? -7 : 7);              // envolee d'octave
            out.push_back (x);
        }
        bar = out;
    }

    void addRoll (LBar& bar)
    {
        if (bar.empty()) return;
        int top = -999;
        for (auto& x : bar) if (x.on >= 48) top = std::max (top, x.step);
        if (top == -999) for (auto& x : bar) top = std::max (top, x.step);
        const bool trip = (p.feel == 0) || (p.feel == 2 && rng.chance (0.5f));
        const int st = trip ? 4 : 3;
        const int from = rng.chance (0.5f) ? 72 : 84;
        LBar out;
        for (auto& x : bar)
            if (! (x.on >= from && x.step == top)) out.push_back ({ x.on, std::min (x.dur, std::max (2, from - x.on + 24)), x.step, x.vel });
        int k = 0;
        for (int t = from; t < 96; t += st, ++k)
            out.push_back ({ t, st, top, std::min (110, 45 + k * 7) });   // roll en crescendo
        bar = out;
    }

    int stepToDeg (int step) const
    {
        if (n == 7) return step;
        return (int) std::lround ((double) step * n / 7.0);
    }

    void renderBars (const std::vector<const LBar*>& bars, int regShift, float velMul, std::vector<PNote>& out)
    {
        for (int b = 0; b < (int) bars.size(); ++b)
        {
            const Chord& c = slots[(size_t) b].c;
            for (auto& x : *bars[(size_t) b])
            {
                const int tick = b * 96 + x.on;
                if (tick >= totalTicks) continue;
                const int d = c.r + stepToDeg (x.step) + regShift;
                int midi = midiC (d, c);
                while (midi > 127) midi -= 12;
                while (midi < 0)   midi += 12;
                const int dur = std::max (2, std::min ((int) std::lround (x.dur * (0.4 + 0.6 * p.gate)), totalTicks - tick));
                out.push_back ({ tick, dur, midi, std::clamp ((int) std::lround (x.vel * velMul), 1, 127) });
            }
        }
    }

    void toMelody (std::vector<PNote>& v, std::vector<MelodyNote>& out) const
    {
        std::sort (v.begin(), v.end(), [] (const PNote& a, const PNote& b)
                   { return a.tick != b.tick ? a.tick < b.tick : a.midi < b.midi; });
        // meme hauteur qui se chevauche : on coupe la precedente
        for (size_t i = 0; i < v.size(); ++i)
            for (size_t j = i + 1; j < v.size() && v[j].tick < v[i].tick + v[i].dur; ++j)
                if (v[j].midi == v[i].midi)
                {
                    v[i].dur = std::max (1, v[j].tick - v[i].tick);
                    break;
                }
        for (auto& x : v)
        {
            if (x.dur <= 0 || (! out.empty() && false)) continue;
            out.push_back ({ x.tick / 24.0, std::min (x.dur / 24.0, totalTicks / 24.0 - x.tick / 24.0 - 1.0e-3),
                             x.midi, x.vel });
        }
        // supprime les doublons exacts (meme instant, meme hauteur)
        out.erase (std::unique (out.begin(), out.end(), [] (const MelodyNote& a, const MelodyNote& b)
                                { return a.start == b.start && a.pitch == b.pitch; }), out.end());
    }

    float meanStep (const LBar& b) const
    {
        if (b.empty()) return 0.0f;
        float s = 0; for (auto& x : b) s += (float) x.step;
        return s / (float) b.size();
    }

    GenResult runLearned()
    {
        GenResult res;
        totalTicks = p.bars * 96;
        if (p.root > 6) base -= 12;   // meme zone de registre que les exemples (tonique entre C4 et F#4)

        // ----- progression : apprise (Auto) ou choisie -----
        int prog4[4] = { 0, 0, 0, 0 };
        if (p.progression <= 0)
        {
            std::vector<float> w;
            for (int i = 0; i < learned::numProgs; ++i)
            {
                const auto& lp = learned::progs[i];
                float x = (float) lp.w;
                bool moves = false, darkChord = false;
                for (int k = 0; k < 4; ++k)
                {
                    if (lp.r[k] != lp.r[0]) moves = true;
                    if (lp.r[k] == 1 || lp.r[k] == 5) darkChord = true;
                }
                if (! moves) x *= 0.35f;                       // boucle sur 1 seul accord : moins souvent
                bool hasII = false;
                for (int k = 0; k < 4; ++k) if (lp.r[k] == 1) hasII = true;
                if (hasII && semiOf (1) != 1) x *= 0.08f;      // ii diminue : rare (sauf phrygien = bII)
                if (darkChord) x *= 1.0f + p.darkness;
                if (style == NINHO && (lp.r[1] == 5 || lp.r[2] == 6 || lp.r[2] == 5)) x *= 1.6f;
                w.push_back (x);
            }
            const auto& lp = learned::progs[rng.pickWeighted (w)];
            for (int k = 0; k < 4; ++k) prog4[k] = lp.r[k];
        }
        else
        {
            for (int k = 0; k < 4; ++k) prog4[k] = progs[p.progression - 1].deg[k];
        }

        for (int b = 0; b < p.bars; ++b)
            slots.push_back ({ b * 96, (b + 1) * 96, makeChord (prog4[b % 4]) });
        for (int b = 0; b < std::min (4, p.bars); ++b)
            res.chordNames.push_back (chordName (slots[(size_t) b].c));

        // ----- dessin principal -----
        const int a = pickTemplate (0, -1, -1, true);
        const int srcA = learned::templates[a].src;
        LBar A = tplNotes (a);
        if (rng.chance (0.5f))
        {
            const int b2 = pickTemplate (-1, -1, srcA, true);
            if (b2 >= 0) A = splice (A, tplNotes (b2));        // croisement : debut de l'un, fin de l'autre
        }
        mutate (A, 0.5f);

        // ----- fin de phrase -----
        LBar V = A;
        const int v = rng.chance (0.65f) ? pickTemplate (3, srcA, -1, false) : -1;
        if (v >= 0 && v != a) V = tplNotes (v);
        mutate (V, 1.0f);
        if (rng.chance (0.25f + 0.6f * p.rolls)) addRoll (V);

        LBar A2 = A; mutate (A2, 1.0f);
        LBar V2 = V;
        if (rng.chance (0.4f + 0.5f * p.rolls)) addRoll (V2); else mutate (V2, 1.0f);

        std::vector<const LBar*> order;
        for (int b = 0; b < p.bars; ++b)
        {
            const bool end = (b % 4 == 3) || (b == p.bars - 1 && p.bars > 1);
            if (b < 4) order.push_back (end ? &V : &A);
            else       order.push_back (end ? &V2 : &A2);
        }

        std::vector<PNote> mel;
        renderBars (order, 0, 1.0f, mel);
        toMelody (mel, res.melody);

        // ----- 2e couche (contre-chant) -----
        if (p.layer2 > 0)
        {
            int c = pickTemplate (-1, -1, srcA, true);
            if (c >= 0)
            {
                LBar C = tplNotes (c);
                mutate (C, 0.7f);
                // registre : au-dessus de la couche principale
                const float diff = meanStep (A) + 6.0f - meanStep (C);
                const int shift = (int) std::lround (diff / 7.0f) * 7;
                LBar Cv = C;
                if (rng.chance (0.5f)) addRoll (Cv);
                std::vector<const LBar*> o2;
                for (int b = 0; b < p.bars; ++b)
                    o2.push_back (((b % 4 == 3) && p.bars > 1) ? &Cv : &C);
                std::vector<PNote> cn;
                renderBars (o2, stepToDeg (shift), 0.85f, cn);
                toMelody (cn, res.counter);
            }
        }

        if (p.chordMode > 0)
            renderChords (res);
        return res;
    }

    // ---------- accords ----------
    void renderChords (GenResult& res)
    {
        int prevCenter = base - 12 + 4;
        for (auto& s : slots)
        {
            const auto& c = s.c;
            int tones[3] = { midiC (c.r, c) - 12, midiC (c.r + c.t3, c) - 12, midiC (c.r + c.t5, c) - 12 };
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
            std::vector<int> voicing (bestInv, bestInv + 3);
            if (use7th())
            {
                int sev = midiC (c.r + c.t7, c) - 12;
                while (sev < prevCenter - 6) sev += 12;
                while (sev > prevCenter + 6) sev -= 12;
                voicing.push_back (sev);
            }
            const int bass = midiC (c.r, c) - 24;

            const double st = s.start / 24.0, len = (s.end - s.start) / 24.0;
            auto add = [&] (double start, double l, int pitch, int vel)
            {
                if (pitch < 0 || pitch > 127) return;
                res.chords.push_back ({ start, l, pitch, vel });
            };

            add (st, len * 0.98, bass, 90);
            if (p.chordMode == 1)
            {
                for (int v : voicing) add (st, len * 0.98, v, 70);
            }
            else
            {
                const bool trip = p.feel != 1;
                for (int h = s.start; h < s.end; h += 48)
                {
                    const double a = h / 24.0;
                    const double second = trip ? (32 / 24.0) : (36 / 24.0);
                    for (int v : voicing)
                    {
                        add (a, second * 0.9, v, 76);
                        add (a + second, (2.0 - second) * 0.85, v, 62);
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

std::vector<std::string> MelodyGenerator::getStyleNames()
{
    return { "Perso", "Ninho", "Werenoi", "Trap dark", "Triolets" };
}

GenResult MelodyGenerator::generate (const GenParams& p)
{
    Gen g (p);
    return g.run();
}

std::vector<std::string> MelodyGenerator::getBankNames()
{
    return { "Toutes", "Melodies", "Guitare", "Nexus", "Melodic" };
}
