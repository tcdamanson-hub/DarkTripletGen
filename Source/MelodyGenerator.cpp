#include "MelodyGenerator.h"
#include "Scales.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
// Generateur pseudo-aleatoire deterministe (meme seed = meme melodie sur toutes les machines)
struct Rng
{
    std::uint64_t s;
    explicit Rng (int seed) : s ((std::uint64_t) (std::uint32_t) seed * 0x9E3779B97F4A7C15ull + 0x1234567ull) {}
    std::uint32_t next()
    {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        return (std::uint32_t) (s >> 16);
    }
    float nextFloat()     { return (float) (next() & 0xFFFFFF) / (float) 0x1000000; }
    int   nextInt (int n) { return n <= 0 ? 0 : (int) (next() % (std::uint32_t) n); }
    bool  chance (float p){ return nextFloat() < p; }
};

// Grille : 24 cases par noire.
//  Triolets : 8 = triolet de croches, 4 = triolet de doubles, 16 = triolet de noires
//  Normal   : 6 = double croche, 12 = croche, 3 = triple croche (rolls)
struct Slot { int on, dur; };
enum CellKind { Normal, Roll, Rest };
struct CellDef { std::vector<Slot> slots; int beats; float base; float densBias; CellKind kind; int feel; }; // feel: 0 triolet, 1 normal, 2 les deux

const std::vector<CellDef>& cells()
{
    static const std::vector<CellDef> c = {
        // ---------- TRIOLETS ----------
        { { {0,8},{8,8},{16,8} },                         1, 3.0f,  1.0f, Normal, 0 }, // 3 triolets de croches
        { { {0,8},{8,16} },                               1, 2.0f,  0.0f, Normal, 0 },
        { { {0,16},{16,8} },                              1, 2.0f,  0.0f, Normal, 0 },
        { { {8,8},{16,8} },                               1, 1.6f,  0.0f, Normal, 0 }, // syncope
        { { {0,8},{16,8} },                               1, 1.5f, -0.3f, Normal, 0 },
        { { {0,16},{16,16},{32,16} },                     2, 1.6f, -0.5f, Normal, 0 }, // triolet de noires
        { { {0,4},{4,4},{8,4},{12,4},{16,4},{20,4} },     1, 1.0f,  1.0f, Roll,   0 }, // roll 6 notes
        { { {0,8},{8,8},{16,4},{20,4} },                  1, 1.4f,  1.0f, Roll,   0 },
        { { {0,4},{4,4},{8,4},{12,4},{16,8} },            1, 1.2f,  1.0f, Roll,   0 },
        // ---------- NORMAL (binaire) ----------
        { { {0,6},{6,6},{12,6},{18,6} },                  1, 2.2f,  1.0f, Normal, 1 }, // 4 doubles
        { { {0,12},{12,12} },                             1, 2.4f,  0.0f, Normal, 1 }, // 2 croches
        { { {0,18},{18,6} },                              1, 2.0f,  0.0f, Normal, 1 }, // croche pointee + double
        { { {0,6},{6,18} },                               1, 1.4f,  0.0f, Normal, 1 },
        { { {0,12},{12,6},{18,6} },                       1, 2.0f,  0.5f, Normal, 1 },
        { { {0,6},{6,6},{12,12} },                        1, 1.8f,  0.5f, Normal, 1 },
        { { {6,6},{12,12} },                              1, 1.3f,  0.0f, Normal, 1 }, // syncope
        { { {12,12} },                                    1, 1.0f, -0.5f, Normal, 1 }, // contretemps
        { { {0,18},{18,18},{36,12} },                     2, 2.2f, -0.3f, Normal, 1 }, // 3-3-2 trap
        { { {0,3},{3,3},{6,3},{9,3},{12,3},{15,3},{18,3},{21,3} }, 1, 0.8f, 1.0f, Roll, 1 }, // roll triples
        { { {0,6},{6,6},{12,3},{15,3},{18,3},{21,3} },    1, 1.2f,  1.0f, Roll,   1 },
        // ---------- COMMUNS ----------
        { { {0,24} },                                     1, 1.2f, -1.0f, Normal, 2 }, // noire
        { { {0,48} },                                     2, 0.6f, -1.0f, Normal, 2 }, // blanche
        { {},                                             1, 1.0f, -1.0f, Rest,   2 },
    };
    return c;
}

// Poids "dark" par intervalle (demi-tons depuis la tonique)
//                           1    b2   2    b3   3    4    b5   5    b6   6    b7   7
const float darkTable[12] = { 3.0f,2.4f,0.9f,2.2f,0.5f,1.1f,1.8f,2.2f,2.0f,0.6f,1.3f,1.2f };

struct GNote { int on; int dur; int degree; int vel; bool roll; bool octUp; };
using Bar = std::vector<GNote>;

class Gen
{
public:
    explicit Gen (const GenParams& params)
        : p (params), rng (params.seed),
          iv (MelodyGenerator::getScaleIntervals (params.scaleIndex, params.rotation))
    {
        n  = (int) iv.size();
        lo = -(n / 3);
        hi = std::max (1, p.rangeOctaves) * n;
        base = 12 * (p.octave + 1) + p.root;
    }

    std::vector<MelodyNote> run()
    {
        int cur = 0;
        Bar A = rhythm (0, 4);
        pitches (A, 0, cur);

        Bar A2 = repitchFrom (A, 48);   // meme rythme, 2e moitie re-melodisee
        Bar A3 = repitchFrom (A, 48);
        Bar B  = newTail (A, 48);       // meme debut, nouvelle fin (rythme + notes)

        // Structure : A A' A B (questions / reponses), en 8 mesures A A' A B A A'' A B
        const Bar* bars8[] = { &A, &A2, &A, &B, &A, &A3, &A, &B };
        const Bar* bars4[] = { &A, &A2, &A, &B };
        const Bar* bars2[] = { &A, &A2 };
        const Bar* bars1[] = { &A };
        const Bar* const* order = bars4;
        int count = 4;
        if      (p.bars == 1) { order = bars1; count = 1; }
        else if (p.bars == 2) { order = bars2; count = 2; }
        else if (p.bars == 8) { order = bars8; count = 8; }

        std::vector<MelodyNote> out;
        for (int b = 0; b < count; ++b)
        {
            Bar bar = *order[b];
            if (b == count - 1)
                resolveEnding (bar);

            for (auto& g : bar)
            {
                MelodyNote m;
                m.start    = b * 4.0 + g.on / 24.0;
                m.length   = std::max (1.0 / 24.0, (g.dur / 24.0) * (double) p.gate);
                m.pitch    = std::clamp (midiOf (g.degree) + (g.octUp ? 12 : 0), 0, 127);
                m.velocity = std::clamp (g.vel, 1, 127);
                out.push_back (m);
            }
        }
        return out;
    }

private:
    const GenParams& p;
    Rng rng;
    std::vector<int> iv;
    int n = 7, lo = -2, hi = 14, base = 61;

    static int floorDiv (int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }

    int semitoneOf (int d) const
    {
        int o = floorDiv (d, n);
        return iv[(size_t) (d - o * n)] % 12;
    }

    int midiOf (int d) const
    {
        int o = floorDiv (d, n);
        return base + o * 12 + iv[(size_t) (d - o * n)];
    }

    int clampDeg (int d) const { return std::clamp (d, lo, hi); }

    const CellDef& pickCell (int remaining, int beat)
    {
        const auto& cs = cells();
        std::vector<float> w (cs.size(), 0.0f);
        float total = 0.0f;
        for (size_t i = 0; i < cs.size(); ++i)
        {
            const auto& c = cs[i];
            if (c.beats > remaining || (c.beats == 2 && beat % 2 != 0))
                continue;
            if (c.feel != 2 && p.feel != 2 && c.feel != p.feel)
                continue;
            float x;
            if (c.kind == Rest)
                x = p.rests * 3.0f * (beat == 0 ? 0.2f : 1.0f);
            else
            {
                x = c.base * std::max (0.05f, 1.0f + c.densBias * (p.density - 0.5f) * 2.0f);
                if (c.kind == Roll)
                    x *= p.rolls * 2.5f;
            }
            w[i] = std::max (0.0f, x);
            total += w[i];
        }
        if (total <= 0.0f)
            return cs[0];
        float r = rng.nextFloat() * total;
        for (size_t i = 0; i < cs.size(); ++i)
        {
            r -= w[i];
            if (r <= 0.0f && w[i] > 0.0f)
                return cs[i];
        }
        return cs[0];
    }

    Bar rhythm (int fromBeat, int toBeat)
    {
        Bar out;
        int beat = fromBeat;
        while (beat < toBeat)
        {
            const auto& c = pickCell (toBeat - beat, beat);
            for (const auto& s : c.slots)
                out.push_back ({ beat * 24 + s.on, s.dur, 0, 100, c.kind == Roll, false });
            beat += c.beats;
        }
        if (out.empty())
            out.push_back ({ fromBeat * 24, 24, 0, 100, false, false });
        return out;
    }

    int pickNext (int cur, bool strong)
    {
        std::vector<std::pair<int, float>> cands;
        float total = 0.0f;
        const float center = (float) n * 0.4f;
        for (int d = std::max (lo, cur - n); d <= std::min (hi, cur + n); ++d)
        {
            int delta = d - cur, ad = std::abs (delta);
            float w;
            if      (ad == 0) w = 0.7f;
            else if (ad == 1) w = 3.0f;
            else if (ad == 2) w = 2.2f;
            else if (ad == 3) w = 1.2f;
            else if (ad == 4) w = 0.8f;
            else if (ad == n) w = 0.3f + p.octaveJumps * 2.0f;
            else              w = 0.3f;

            int semi = semitoneOf (d);
            w *= 1.0f + p.darkness * (darkTable[semi] - 1.0f);
            if (strong && (semi == 0 || semi == 7 || semi == 3 || semi == 1 || semi == 8))
                w *= 1.7f;
            if (delta < 0)
                w *= 1.0f + 0.4f * p.darkness;
            w *= std::exp (-std::abs ((float) d - center) / ((float) n * 1.2f));

            cands.push_back ({ d, w });
            total += w;
        }
        if (cands.empty() || total <= 0.0f)
            return clampDeg (cur);
        float r = rng.nextFloat() * total;
        for (auto& c : cands)
        {
            r -= c.second;
            if (r <= 0.0f)
                return c.first;
        }
        return cands.back().first;
    }

    void pitches (Bar& notes, size_t from, int& cur)
    {
        for (size_t i = from; i < notes.size(); ++i)
        {
            auto& nt = notes[i];
            const bool strong = (nt.on % 24) == 0;
            const bool contRoll = nt.roll && i > 0 && notes[i - 1].roll
                                  && notes[i - 1].on + notes[i - 1].dur == nt.on;
            if (contRoll)
            {
                float r = rng.nextFloat();
                if      (r < 0.55f) {}                                       // repetition (roll trap)
                else if (r < 0.80f) cur = clampDeg (cur + (rng.chance (0.5f) ? 1 : -1));
                else                cur = clampDeg (cur - 2);
            }
            else
            {
                if (i == 0 && from == 0 && notes[0].on == 0 && rng.chance (0.4f + 0.4f * p.darkness))
                    cur = 0; // la melodie s'ancre sur la tonique
                else
                    cur = pickNext (cur, strong);
            }
            nt.degree = cur;
            nt.octUp  = ! nt.roll && rng.chance (p.octaveJumps * 0.5f) && midiOf (cur) + 12 <= 127;

            if (strong)            nt.vel = 98 + rng.nextInt (16);
            else if (nt.on % 12 == 0 || nt.on % 8 == 0) nt.vel = 82 + rng.nextInt (16);
            else                   nt.vel = 68 + rng.nextInt (18);
        }
    }

    static int degreeBefore (const Bar& b, int slot, int fallback)
    {
        int d = fallback;
        for (auto& g : b)
            if (g.on < slot) d = g.degree;
        return d;
    }

    Bar repitchFrom (const Bar& src, int slot)
    {
        Bar b = src;
        size_t first = 0;
        while (first < b.size() && b[first].on < slot) ++first;
        int cur = degreeBefore (b, slot, 0);
        pitches (b, first, cur);
        return b;
    }

    Bar newTail (const Bar& src, int slot)
    {
        Bar b;
        for (auto& g : src)
            if (g.on < slot) b.push_back (g);
        int cur = degreeBefore (src, slot, 0);
        size_t from = b.size();
        Bar tail = rhythm (slot / 24, 4);
        b.insert (b.end(), tail.begin(), tail.end());
        pitches (b, from, cur);
        return b;
    }

    // Derniere note -> tonique la plus proche, prolongee jusqu'a la fin de la mesure
    void resolveEnding (Bar& b)
    {
        if (b.empty()) return;
        auto& last = b.back();
        int o = floorDiv (last.degree + n / 2, n);
        int target = o * n;
        if (target > hi) target -= n;
        if (target < lo) target += n;
        last.degree = target;
        last.octUp  = false;
        last.roll   = false;
        if (96 - last.on <= 48)
            last.dur = 96 - last.on;
        last.vel = std::max (last.vel, 100);
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

std::vector<MelodyNote> MelodyGenerator::generate (const GenParams& p)
{
    Gen g (p);
    return g.run();
}
