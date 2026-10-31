#include "DrumGenerator.h"
#include "DrumData.h"
#include "LearnedData.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// =============================================================================
//  Generateur de drums trap appris depuis les MIDI d'exemple
//   - hats  : pattern appris (rolls, pitch) + croisement + rolls ajoutes
//   - kick  : pattern appris
//   - snare : snare principal sur le 3 (half-time) + relances apprises
//   - 808   : suit le kick, joue la fondamentale de l'accord de chaque mesure,
//             envolees d'octave en fin de phrase
//  Grille : 24 ticks par noire, 96 par mesure, patterns de 4 mesures (384 ticks)
// =============================================================================

namespace
{
struct Rng
{
    std::uint64_t s;
    explicit Rng (int seed) : s ((std::uint64_t) (std::uint32_t) seed * 0x9E3779B97F4A7C15ull + 0x9876543ull) {}
    std::uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (std::uint32_t) (s >> 16); }
    float nextFloat()      { return (float) (next() & 0xFFFFFF) / (float) 0x1000000; }
    int   nextInt (int n)  { return n <= 0 ? 0 : (int) (next() % (std::uint32_t) n); }
    bool  chance (float p) { return nextFloat() < p; }
    int pickWeighted (const std::vector<float>& w)
    {
        float total = 0; for (float x : w) total += std::max (0.0f, x);
        if (total <= 0) return -1;
        float r = nextFloat() * total;
        for (int i = 0; i < (int) w.size(); ++i) { r -= std::max (0.0f, w[(size_t) i]); if (r <= 0 && w[(size_t) i] > 0) return i; }
        for (int i = (int) w.size() - 1; i >= 0; --i) if (w[(size_t) i] > 0) return i;
        return -1;
    }
};

struct Ev { int t, dur, pitch, vel; };
using Pat = std::vector<Ev>;

// progressions nommees (memes que dans Dark Triplet Gen), degres de la gamme mineure
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
const int minorSemis[7] = { 0, 2, 3, 5, 7, 8, 10 };
const char* noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

class Gen
{
public:
    explicit Gen (const DrumParams& params) : p (params), rng (params.seed) {}

    DrumResult run()
    {
        DrumResult r;
        const int bars = std::clamp (p.bars, 1, 8);
        total = bars * 96;

        // ---------- accords (pour la 808) ----------
        int prog4[4] = { 0, 0, 0, 0 };
        if (p.progression <= 0 || p.progression > numProgs)
        {
            std::vector<float> w;
            for (int i = 0; i < learned::numProgs; ++i)
            {
                const auto& lp = learned::progs[i];
                bool moves = false, hasII = false;
                for (int k = 0; k < 4; ++k) { moves |= lp.r[k] != lp.r[0]; hasII |= lp.r[k] == 1; }
                w.push_back ((float) lp.w * (moves ? 1.0f : 0.35f) * (hasII ? 0.1f : 1.0f));
            }
            const auto& lp = learned::progs[rng.pickWeighted (w)];
            for (int k = 0; k < 4; ++k) prog4[k] = lp.r[k];
        }
        else
            for (int k = 0; k < 4; ++k) prog4[k] = progs[p.progression - 1].deg[k];

        for (int b = 0; b < bars; ++b)
        {
            const int semi = minorSemis[std::clamp (prog4[b % 4], 0, 6)];
            r.chordRoots.push_back (semi);
            if (b < 4)
            {
                const int pc = (p.root + semi) % 12;
                const bool major = prog4[b % 4] == 2 || prog4[b % 4] == 5 || prog4[b % 4] == 6;
                r.chordNames.push_back (std::string (noteNames[pc]) + (major ? "" : (prog4[b % 4] == 1 ? "dim" : "m")));
            }
        }

        // ---------- HATS ----------
        Pat hats = buildHats();
        // ---------- KICK ----------
        Pat kick = buildKick();
        // ---------- SNARE ----------
        Pat fills;
        Pat snare = buildSnare (fills);
        Pat open = buildOpenHats (hats);
        // ---------- 808 ----------
        Pat bass = build808 (kick, r.chordRoots);

        toHits (hats, r.hats, 3);
        toHits (kick, r.kick, 6);
        toHits (snare, r.snare, 6);
        toHits (fills, r.fills, 6);
        toHits (open, r.openHats, 18);
        toHits (bass, r.bass808, -1);
        return r;
    }

private:
    const DrumParams& p;
    Rng rng;
    int total = 384;

    // ----- outils -----
    static Pat loadPat (const drumdata::DPat& dp)
    {
        Pat out;
        for (int i = 0; i < dp.count; ++i)
        {
            const auto& n = drumdata::notes[dp.start + i];
            out.push_back ({ n.t, n.dur, n.pitch, n.vel });
        }
        return out;
    }

    bool feelOk (int f) const
    {
        if (p.feel == 2 || f == 2) return true;
        return f == p.feel;
    }

    int pick (const drumdata::DPat* pats, int num, float target, bool strictFeel)
    {
        std::vector<float> w ((size_t) num, 0.0f);
        bool any = false;
        for (int i = 0; i < num; ++i)
        {
            if (strictFeel && ! feelOk (pats[i].feel)) continue;
            const float d = (float) pats[i].count - target;
            w[(size_t) i] = std::exp (-(d * d) / (2.0f * std::max (16.0f, target * 0.35f) * std::max (16.0f, target * 0.35f))) + 0.02f;
            any = true;
        }
        if (! any) return pick (pats, num, target, false);
        return rng.pickWeighted (w);
    }

    // repete un pattern de 4 mesures sur la longueur voulue
    Pat tile (const Pat& src, int length) const
    {
        Pat out;
        for (int off = 0; off < length; off += 384)
            for (auto& e : src)
                if (off + e.t < length) out.push_back ({ off + e.t, e.dur, e.pitch, e.vel });
        return out;
    }

    // remplace la mesure "bar" (0..3) de a par celle de b
    static Pat swapBar (const Pat& a, const Pat& b, int bar)
    {
        Pat out;
        for (auto& e : a) if (e.t / 96 != bar) out.push_back (e);
        for (auto& e : b) if (e.t / 96 == bar) out.push_back (e);
        std::sort (out.begin(), out.end(), [] (const Ev& x, const Ev& y) { return x.t < y.t; });
        return out;
    }

    int snapStraight (int t) const { return (p.feel == 1) ? (int) std::lround (t / 6.0) * 6 : t; }

    Pat buildHats()
    {
        const float target = 50.0f + p.hatDensity * 110.0f;
        const int a = pick (drumdata::hats, drumdata::num_hats, target, true);
        Pat A = loadPat (drumdata::hats[a]);

        // croisement : la 4e mesure (souvent la relance) vient d'un autre pattern
        if (rng.chance (0.6f))
        {
            const int b = pick (drumdata::hats, drumdata::num_hats, target, true);
            if (b >= 0) A = swapBar (A, loadPat (drumdata::hats[b]), 3);
        }

        // rolls : on garde / enleve les notes rapides
        Pat out;
        for (size_t i = 0; i < A.size(); ++i)
        {
            const int gapPrev = i > 0 ? A[i].t - A[i - 1].t : 99;
            const bool fast = gapPrev > 0 && gapPrev < 6;
            if (fast && ! rng.chance (std::min (1.0f, p.rolls * 1.6f))) continue;
            out.push_back (A[i]);
        }

        // roll ajoute en fin de mesure 2 et 4
        for (int bar : { 1, 3 })
        {
            if (! rng.chance (p.rolls * 0.55f)) continue;
            const bool trip = p.feel == 0 || (p.feel == 2 && rng.chance (0.5f));
            const int step = trip ? (rng.chance (0.5f) ? 4 : 2) : 3;
            const int from = bar * 96 + (rng.chance (0.5f) ? 72 : 84);
            const int to = bar * 96 + 96;
            out.erase (std::remove_if (out.begin(), out.end(), [&] (const Ev& e) { return e.t >= from && e.t < to; }), out.end());
            int k = 0;
            const bool down = rng.chance (0.6f);
            for (int t = from; t < to; t += step, ++k)
            {
                const int pitch = down ? -3 * (k / 3) : 0;      // descente en tierces mineures
                out.push_back ({ t, step, pitch, std::min (115, 40 + k * 8) });
            }
        }

        // hauteur (hats pitches)
        for (auto& e : out)
        {
            const float scaled = (float) e.pitch * p.hatPitch * 1.25f;
            e.pitch = (int) std::lround (scaled / 3.0f) * 3;
            e.pitch = std::clamp (e.pitch, -24, 24);
            e.t = snapStraight (e.t);
        }
        std::sort (out.begin(), out.end(), [] (const Ev& x, const Ev& y) { return x.t < y.t; });

        Pat full = tile (out, std::min (total, 384));
        if (total > 384)
        {
            Pat B = out;
            const int c = pick (drumdata::hats, drumdata::num_hats, target, true);
            if (c >= 0) B = swapBar (B, loadPat (drumdata::hats[c]), 3);
            for (auto& e : B) if (384 + e.t < total) full.push_back ({ 384 + e.t, e.dur, e.pitch, e.vel });
        }
        humanize (full, 6);
        return full;
    }

    Pat buildKick()
    {
        const float target = 6.0f + p.kickDensity * 26.0f;
        const int a = pick (drumdata::kicks, drumdata::num_kicks, target, false);
        Pat A = loadPat (drumdata::kicks[a]);
        if (rng.chance (0.4f))
        {
            const int b = pick (drumdata::kicks, drumdata::num_kicks, target, false);
            if (b >= 0) A = swapBar (A, loadPat (drumdata::kicks[b]), 3);
        }
        // un kick sur le 1 de la boucle, toujours
        bool hasOne = false;
        for (auto& e : A) if (e.t == 0) hasOne = true;
        if (! hasOne) A.push_back ({ 0, 24, 0, 110 });
        for (auto& e : A) { e.pitch = 0; e.t = snapStraight (e.t); }
        std::sort (A.begin(), A.end(), [] (const Ev& x, const Ev& y) { return x.t < y.t; });

        Pat full = tile (A, std::min (total, 384));
        if (total > 384)
        {
            Pat B = A;
            const int c = pick (drumdata::kicks, drumdata::num_kicks, target, false);
            if (c >= 0) B = swapBar (B, loadPat (drumdata::kicks[c]), 3);
            for (auto& e : B) if (384 + e.t < total) full.push_back ({ 384 + e.t, e.dur, 0, e.vel });
        }
        dedupe (full);
        humanize (full, 4);
        return full;
    }

    // open hats : un coup ouvert sur le "et" du 2 ou du 4, les hats fermes a cet endroit sont retires
    Pat buildOpenHats (Pat& hats)
    {
        Pat out;
        for (int b = 0; b < total / 96; ++b)
        {
            if (! rng.chance (p.openHat)) continue;
            const bool trip = p.feel == 0 || (p.feel == 2 && rng.chance (0.5f));
            const int choices[] = { trip ? 88 : 84, trip ? 40 : 36, trip ? 64 : 60 };
            const int t = b * 96 + choices[rng.chance (0.6f) ? 0 : (rng.chance (0.6f) ? 1 : 2)];
            hats.erase (std::remove_if (hats.begin(), hats.end(), [t] (const Ev& e) { return std::abs (e.t - t) <= 2; }), hats.end());
            out.push_back ({ t, 18, 0, 92 + rng.nextInt (16) });
        }
        return out;
    }

    Pat buildSnare (Pat& fillsOut)
    {
        Pat out;
        if (p.snareOn3)
            for (int b = 0; b < total / 96; ++b)
                out.push_back ({ b * 96 + 48, 24, 0, 112 });

        if (p.fills > 0.01f)
        {
            const int a = pick (drumdata::snares, drumdata::num_snares, 8.0f + p.fills * 30.0f, false);
            Pat F = loadPat (drumdata::snares[a]);
            Pat fills;
            for (auto& e : F)
            {
                const int bar = e.t / 96;
                const float keep = (bar == 3 || bar == 1) ? std::min (1.0f, p.fills * 1.6f) : p.fills * 0.8f;
                if (! rng.chance (keep)) continue;
                const int t = snapStraight (e.t);
                if (p.snareOn3 && std::abs ((t % 96) - 48) <= 2) continue; // deja le snare principal
                fills.push_back ({ t, e.dur, std::clamp (e.pitch, -12, 12), std::min (e.vel, 100) });
            }
            for (int off = 0; off < total; off += 384)
                for (auto& e : fills)
                    if (off + e.t < total) fillsOut.push_back ({ off + e.t, e.dur, e.pitch, e.vel });
            std::sort (fillsOut.begin(), fillsOut.end(), [] (const Ev& x, const Ev& y) { return x.t < y.t; });
            dedupe (fillsOut);
            humanize (fillsOut, 4);
        }
        std::sort (out.begin(), out.end(), [] (const Ev& x, const Ev& y) { return x.t < y.t; });
        dedupe (out);
        humanize (out, 4);
        return out;
    }

    // 808 : une note par kick, tenue jusqu'au kick suivant, sur la fondamentale de l'accord
    Pat build808 (const Pat& kick, const std::vector<int>& roots)
    {
        Pat out;
        std::vector<int> on;
        for (auto& e : kick) on.push_back (e.t);
        for (int b = 0; b < total / 96; ++b) on.push_back (b * 96); // changement d'accord = nouvelle note
        std::sort (on.begin(), on.end());
        on.erase (std::unique (on.begin(), on.end(), [] (int a, int b) { return b - a < 4; }), on.end());

        for (size_t i = 0; i < on.size(); ++i)
        {
            const int t = on[i];
            const int next = i + 1 < on.size() ? on[i + 1] : total;
            const int bar = std::min ((int) roots.size() - 1, t / 96);
            int pitch = rootPitch (roots[(size_t) bar]);
            int vel = 100;
            // envolee d'octave apprise : derniers temps des mesures 2 et 4
            const int inBar = t % 96, barIdx = (t / 96) % 4;
            if ((barIdx == 1 || barIdx == 3) && inBar >= 48 && rng.chance (p.bounce808 * 0.7f))
            {
                pitch += 12;
                vel = 92;
            }
            out.push_back ({ t, std::max (6, std::min (next - t, 192)), pitch, vel });
        }

        // petit "bounce" : triolets de noires sur l'octave en fin de boucle
        if (rng.chance (p.bounce808 * 0.6f) && total >= 192)
        {
            const int bar = total / 96 - 1;
            const int base = rootPitch (roots[(size_t) bar]) + 12;
            const int from = bar * 96 + 48;
            out.erase (std::remove_if (out.begin(), out.end(), [&] (const Ev& e) { return e.t >= from; }), out.end());
            for (auto& e : out) if (e.t < from && e.t + e.dur > from) e.dur = from - e.t;
            const int st = p.feel == 1 ? 12 : 16;
            for (int tt = from; tt < bar * 96 + 96; tt += st)
                out.push_back ({ tt, st, base, 88 });
        }
        std::sort (out.begin(), out.end(), [] (const Ev& x, const Ev& y) { return x.t < y.t; });
        return out;
    }

    int rootPitch (int semiFromTonic) const
    {
        int pc = (p.root + semiFromTonic) % 12;
        int pitch = 60 + pc;          // autour de C5 (convention FL : racine d'un canal = C5)
        if (pitch > 66) pitch -= 12;  // reste dans la zone grave F#4..F#5
        return pitch;
    }

    static void dedupe (Pat& v)
    {
        std::sort (v.begin(), v.end(), [] (const Ev& x, const Ev& y) { return x.t != y.t ? x.t < y.t : x.vel > y.vel; });
        v.erase (std::unique (v.begin(), v.end(), [] (const Ev& a, const Ev& b) { return a.t == b.t && a.pitch == b.pitch; }), v.end());
    }

    void humanize (Pat& v, int amount)
    {
        for (auto& e : v)
            e.vel = std::clamp (e.vel + rng.nextInt (2 * amount + 1) - amount, 1, 127);
    }

    void toHits (Pat& v, std::vector<DrumHit>& out, int fixedLen)
    {
        std::sort (v.begin(), v.end(), [] (const Ev& x, const Ev& y) { return x.t < y.t; });
        for (size_t i = 0; i < v.size(); ++i)
        {
            const auto& e = v[i];
            if (e.t < 0 || e.t >= total) continue;
            int len = fixedLen > 0 ? fixedLen : e.dur;
            // pas de chevauchement avec la note suivante de meme hauteur
            for (size_t j = i + 1; j < v.size(); ++j)
                if (v[j].t > e.t) { if (fixedLen < 0 || v[j].pitch == e.pitch) len = std::min (len, v[j].t - e.t); break; }
            len = std::max (1, std::min (len, total - e.t));
            out.push_back ({ e.t / 24.0, len / 24.0 - 1.0e-3, std::clamp (60 + (fixedLen < 0 ? e.pitch - 60 : e.pitch), 0, 127), e.vel });
        }
        out.erase (std::unique (out.begin(), out.end(), [] (const DrumHit& a, const DrumHit& b)
                                { return a.start == b.start && a.pitch == b.pitch; }), out.end());
    }
};
} // namespace

std::vector<std::string> DrumGenerator::getProgressionNames()
{
    std::vector<std::string> v { "Auto" };
    for (auto& pr : progs) v.push_back (pr.name);
    return v;
}

DrumResult DrumGenerator::generate (const DrumParams& p)
{
    Gen g (p);
    return g.run();
}
