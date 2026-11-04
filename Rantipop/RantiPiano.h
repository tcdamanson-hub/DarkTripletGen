#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

// Piano de synthese (le meme que celui valide dans RAIZER) : harmoniques legerement
// inharmoniques, chaque harmonique s'eteint a sa vitesse, bruit de marteau, battements.
namespace ranti
{
class PianoSynth
{
public:
    void prepare (double sampleRate) { sr = sampleRate; for (auto& v : voices) v.active = false; }
    void setRelease (float s) { release = juce::jmax (0.02f, s); }

    void noteOn (int bus, int n, float vel)
    {
        Voice* v = nullptr;
        for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (v == nullptr)
        {
            v = &voices[0];
            for (auto& x : voices) if (x.age < v->age) v = &x;
        }
        const double f = 440.0 * std::pow (2.0, (n - 69) / 12.0);
        const float level = 0.36f * (0.25f + 0.75f * vel);
        const double tau1 = juce::jlimit (0.6, 8.0, 5.0 * std::pow (2.0, -(n - 48) / 18.0));
        const double B = 0.00012 * std::pow (2.0, (n - 48) / 12.0);
        const double bright = 0.35 + 0.65 * vel;
        const double twoPi = juce::MathConstants<double>::twoPi;
        v->active = true; v->held = true; v->note = n; v->bus = bus; v->age = ++counter; v->np = 0;
        for (int k = 1; k <= NP; ++k)
        {
            const double fk = k * f * std::sqrt (1.0 + B * k * k);
            if (fk > sr * 0.45) break;
            double a = std::pow ((double) k, -1.1) * std::exp (-(k - 1) * (1.0 - bright) * 0.55);
            if (k == 7) a *= 0.2;
            const double tau = tau1 / (1.0 + 0.35 * (k - 1));
            const double w = twoPi * fk / sr;
            const int i = v->np++;
            v->c[i] = 1.0f; v->s[i] = 0.0f;
            v->cr[i] = (float) std::cos (w); v->sn[i] = (float) std::sin (w);
            v->amp[i] = (float) a * level;
            v->dec[i] = (float) std::exp (-1.0 / (tau * sr));
        }
        v->rel = 1.0f; v->relMul = 1.0f;
        v->noise = 0.35f * vel * level;
        v->noiseDec = (float) std::exp (-1.0 / (0.004 * sr));
        v->lp = 0.0f;
        const float pan = juce::jlimit (-0.6f, 0.6f, (n - 60) / 40.0f);
        v->panL = std::sqrt (0.5f * (1.0f - pan));
        v->panR = std::sqrt (0.5f * (1.0f + pan));
        const double bw = twoPi * (0.6 + 0.8 * rnd.nextDouble()) / sr;
        v->bc = 1.0f; v->bs = 0.0f; v->bcr = (float) std::cos (bw); v->bsn = (float) std::sin (bw);
        v->renorm = 0;
    }

    void noteOff (int bus, int n)
    {
        for (auto& v : voices)
            if (v.active && v.held && v.note == n && v.bus == bus) { v.held = false; v.relMul = (float) std::exp (-1.0 / (release * sr)); }
    }

    void allOff() { for (auto& v : voices) if (v.active) { v.held = false; v.relMul = (float) std::exp (-1.0 / (0.05 * sr)); } }

    void render (juce::AudioBuffer<float>& buses, int start, int end)
    {
        for (auto& v : voices)
        {
            if (! v.active) continue;
            float* L = buses.getWritePointer (v.bus * 2);
            float* R = buses.getWritePointer (v.bus * 2 + 1);
            for (int i = start; i < end; ++i)
            {
                const float beat = 1.0f + 0.06f * v.bs;
                float sum = 0.0f;
                for (int k = 0; k < v.np; ++k)
                {
                    sum += v.s[k] * v.amp[k] * (k < 3 ? beat : 1.0f);
                    const float nc = v.c[k] * v.cr[k] - v.s[k] * v.sn[k];
                    const float ns = v.s[k] * v.cr[k] + v.c[k] * v.sn[k];
                    v.c[k] = nc; v.s[k] = ns;
                    v.amp[k] *= v.dec[k];
                }
                const float bnc = v.bc * v.bcr - v.bs * v.bsn;
                v.bs = v.bs * v.bcr + v.bc * v.bsn; v.bc = bnc;
                const float nz = (rnd.nextFloat() * 2.0f - 1.0f) * v.noise;
                v.noise *= v.noiseDec;
                v.lp += 0.35f * (nz - v.lp);
                sum += v.lp;
                if (! v.held) v.rel *= v.relMul;
                sum *= v.rel;
                L[i] += sum * v.panL;
                R[i] += sum * v.panR;
            }
            if ((v.renorm += end - start) > 512)
            {
                v.renorm = 0;
                for (int k = 0; k < v.np; ++k)
                {
                    const float m = 1.0f / std::sqrt (v.c[k] * v.c[k] + v.s[k] * v.s[k]);
                    v.c[k] *= m; v.s[k] *= m;
                }
                const float bm = 1.0f / std::sqrt (v.bc * v.bc + v.bs * v.bs);
                v.bc *= bm; v.bs *= bm;
            }
            if (v.rel < 1.0e-4f || (v.np > 0 && v.amp[0] < 1.0e-5f))
                v.active = false;
        }
    }

private:
    static constexpr int NP = 14;
    struct Voice
    {
        bool active = false, held = false;
        int note = -1, bus = 0, np = 0, renorm = 0;
        juce::uint32 age = 0;
        float c[NP] {}, s[NP] {}, cr[NP] {}, sn[NP] {}, amp[NP] {}, dec[NP] {};
        float rel = 1.0f, relMul = 1.0f;
        float noise = 0.0f, noiseDec = 0.0f, lp = 0.0f;
        float panL = 0.7f, panR = 0.7f;
        float bc = 1.0f, bs = 0.0f, bcr = 1.0f, bsn = 0.0f;
    };
    double sr = 44100.0;
    float release = 0.3f;
    std::array<Voice, 24> voices;
    juce::Random rnd { 1234 };
    juce::uint32 counter = 0;
};
} // namespace ranti
