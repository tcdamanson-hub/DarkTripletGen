#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include "Instruments.h"

// Piano de pre-ecoute (synthese additive) :
//  - harmoniques legerement inharmoniques (cordes de piano)
//  - chaque harmonique s'eteint a sa vitesse (les aigus partent plus vite)
//  - bruit de marteau, battements entre cordes, etouffoir au relachement
//  - filtre de brillance + reverb
//  canal 1 = melodie, canal 2 = accords (plus doux)
class DarkSynth
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        filter.prepare ({ sr, (juce::uint32) juce::jmax (1, maxBlock), 2 });
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setResonance (0.5f);
        reverb.setSampleRate (sr);
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.62f; rp.damping = 0.45f; rp.wetLevel = 0.22f; rp.dryLevel = 0.85f; rp.width = 1.0f;
        reverb.setParameters (rp);
        reverb.reset();
        filter.reset();
        for (auto& v : voices) v.active = false;
        player.prepare (sr);

        // mix des instruments samples : chaque couche a sa place dans le spectre
        const juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, maxBlock), 2 };
        layerBuf.setSize (8, juce::jmax (1, maxBlock));
        const float hpHz[4] = { 0, 180.0f, 220.0f, 380.0f };   // melodie, accords, couche 2 : on laisse le grave a la 808
        const float lpHz[4] = { 0, 16000.0f, 6000.0f, 10000.0f }; // les accords restent derriere
        for (int c = 1; c <= 3; ++c)
        {
            hp[c].prepare (spec); hp[c].setType (juce::dsp::StateVariableTPTFilterType::highpass);
            hp[c].setCutoffFrequency (hpHz[c]); hp[c].setResonance (0.6f); hp[c].reset();
            lp[c].prepare (spec); lp[c].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
            lp[c].setCutoffFrequency (lpHz[c]); lp[c].setResonance (0.6f); lp[c].reset();
            layerLp[c] = lpHz[c];
        }
        // EQ de placement : la melodie ressort (presence), les accords et la couche 2 lui laissent la place
        const float eqHz[4]   = { 0, 3000.0f, 2600.0f, 1900.0f };
        const float eqGain[4] = { 0, 3.5f, -5.0f, -2.5f };  // dB
        const float eqQ[4]    = { 0, 0.8f, 0.9f, 1.0f };
        for (int c = 1; c <= 3; ++c)
        {
            peak[c].prepare (spec);
            *peak[c].state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, eqHz[c], eqQ[c], juce::Decibels::decibelsToGain (eqGain[c]));
            peak[c].reset();
            layerComp[c].prepare (spec);
            layerComp[c].setThreshold (c == 1 ? -20.0f : -16.0f);
            layerComp[c].setRatio (c == 1 ? 3.5f : 2.0f);
            layerComp[c].setAttack (c == 1 ? 6.0f : 15.0f);
            layerComp[c].setRelease (110.0f);
            layerComp[c].reset();
        }
        busComp.prepare (spec);
        busComp.setThreshold (-16.0f); busComp.setRatio (3.0f); busComp.setAttack (8.0f); busComp.setRelease (120.0f);
        busComp.reset();
    }

    // instrument par canal : 0 = piano de synthese, sinon index+1 dans InstrumentLibrary
    // volume par couche (canal 1 melodie, 2 accords, 3 couche 2), 0 = coupe
    void setLayerGains (float melody, float chords, float counterLayer)
    {
        layerGain[1] = melody; layerGain[2] = chords; layerGain[3] = counterLayer;
        player.setLayerGains (layerGain);
    }

    void setInstruments (int melody, int chords, int counterLayer)
    {
        inst[1] = melody; inst[2] = chords; inst[3] = counterLayer;
    }

    void setParams (float cutoffHz, float releaseSec, float gain)
    {
        filter.setCutoffFrequency (juce::jlimit (200.0f, 18000.0f, cutoffHz));
        for (int c = 1; c <= 3; ++c)
            lp[c].setCutoffFrequency (juce::jlimit (300.0f, 18000.0f, juce::jmin (cutoffHz * 1.6f, layerLp[c])));
        release = juce::jmax (0.02f, releaseSec);
        player.setRelease (release);
        outGain = gain;
    }

    void render (juce::AudioBuffer<float>& buf, const juce::MidiBuffer& midi)
    {
        if (layerBuf.getNumSamples() < buf.getNumSamples())
            layerBuf.setSize (8, buf.getNumSamples(), false, false, true);
        layerBuf.clear();
        int pos = 0;
        for (const auto meta : midi)
        {
            const int t = juce::jlimit (0, buf.getNumSamples(), meta.samplePosition);
            renderRange (buf, pos, t);
            handle (meta.getMessage());
            pos = t;
        }
        renderRange (buf, pos, buf.getNumSamples());

        // brillance + reverb + volume
        const int num = buf.getNumSamples();
        auto* L = buf.getWritePointer (0);
        auto* R = buf.getNumChannels() > 1 ? buf.getWritePointer (1) : nullptr;
        for (int i = 0; i < num; ++i)
        {
            L[i] = filter.processSample (0, L[i]);
            if (R != nullptr) R[i] = filter.processSample (1, R[i]);
        }
        if (R != nullptr) reverb.processStereo (L, R, num);
        else              reverb.processMono (L, num);

        // instruments samples : lecture simple et propre, comme le kit de drums
        // (pas d'EQ, pas de compresseur, pas de reverb ajoutee : le son de ton sample tel quel)
        for (int c = 1; c <= 3; ++c)
        {
            const float g = sampleLevel[c];
            const float* sl = layerBuf.getReadPointer (c * 2);
            const float* sr2 = layerBuf.getReadPointer (c * 2 + 1);
            for (int i = 0; i < num; ++i)
            {
                L[i] += sl[i] * g;
                if (R != nullptr) R[i] += sr2[i] * g;
                else L[i] += sr2[i] * g;
            }
        }

        for (int c = 0; c < buf.getNumChannels(); ++c)
        {
            auto* d = buf.getWritePointer (c);
            for (int i = 0; i < num; ++i)
                d[i] = std::tanh (d[i] * outGain * 0.42f) * 0.9f;
        }
    }

private:
    static constexpr int NP = 14; // harmoniques max par note

    struct Voice
    {
        bool active = false, held = false;
        int note = -1, channel = 1, np = 0, renorm = 0;
        juce::uint32 age = 0;
        float c[NP] {}, s[NP] {}, cr[NP] {}, sn[NP] {}, amp[NP] {}, dec[NP] {};
        float rel = 1.0f, relMul = 1.0f;
        float noise = 0.0f, noiseDec = 0.0f, lp = 0.0f;
        float panL = 0.7f, panR = 0.7f;
        float bc = 1.0f, bs = 0.0f, bcr = 1.0f, bsn = 0.0f; // battement lent entre cordes
    };

    double sr = 44100.0;
    std::array<Voice, 16> voices;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::Reverb reverb;
    InstrumentPlayer player;
    juce::AudioBuffer<float> layerBuf;          // canaux 2..7 : couches 1..3 en stereo, 0..1 : bus
    juce::dsp::StateVariableTPTFilter<float> hp[4], lp[4];
    float layerLp[4] = { 0, 16000.0f, 6000.0f, 10000.0f };
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> peak[4];
    juce::dsp::Compressor<float> layerComp[4];
    juce::dsp::Compressor<float> busComp;
    float sampleLevel[4] = { 0, 2.8f, 3.0f, 3.5f };   // melodie, accords, contre-melo
    int inst[4] = { 0, 0, 0, 0 };
    float layerGain[4] = { 1, 1, 1, 1 };
    juce::Random rnd { 1234 };
    float release = 0.3f, outGain = 0.7f;
    juce::uint32 counter = 0;

    static double mtof (int n) { return 440.0 * std::pow (2.0, (n - 69) / 12.0); }

    void handle (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())        noteOn (m.getChannel(), m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())  noteOff (m.getChannel(), m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            for (auto& v : voices) releaseVoice (v);
            player.allOff();
        }
    }

    void releaseVoice (Voice& v)
    {
        v.held = false;
        v.relMul = (float) std::exp (-1.0 / (release * sr));
    }

    void noteOff (int ch, int n)
    {
        player.noteOff (ch, (ch >= 1 && ch <= 3 && inst[ch] > 0) ? liftNote (ch, n) : n);
        for (auto& v : voices)
            if (v.active && v.held && v.note == n && v.channel == ch)
                releaseVoice (v);
    }

    // registre des sons samples : accords et contre-melo entre Do 3 et Re 5, melodie au-dessus de Do 3
    static int liftNote (int ch, int n)
    {
        while (n < 48) n += 12;
        if (ch == 2) while (n > 74) n -= 12;
        return n;
    }

    void noteOn (int ch, int n, float vel)
    {
        const int sel = (ch >= 1 && ch <= 3) ? inst[ch] : 0;
        if (sel > 0)
        {
            // notes trop graves pour un son samples : remontees d'octave (la 808 garde le grave)
            n = liftNote (ch, n);
            const float lvl = 1.0f;

            player.noteOn (ch, n, vel, sel - 1, lvl);
            return;
        }
        Voice* v = nullptr;
        for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (v == nullptr)
        {
            v = &voices[0];
            for (auto& x : voices) if (x.age < v->age) v = &x; // vole la plus ancienne
        }

        const double f = mtof (n);
        const float level = (ch == 1 ? 0.36f : ch == 3 ? 0.26f : (n < 48 ? 0.32f : 0.2f)) * (0.25f + 0.75f * vel);
        const double tau1 = juce::jlimit (0.6, 8.0, 5.0 * std::pow (2.0, -(n - 48) / 18.0));
        const double B = 0.00012 * std::pow (2.0, (n - 48) / 12.0);   // inharmonicite
        const double bright = 0.35 + 0.65 * vel;
        const double twoPi = juce::MathConstants<double>::twoPi;

        v->active = true; v->held = true;
        v->note = n; v->channel = ch; v->age = ++counter;
        v->np = 0;
        for (int k = 1; k <= NP; ++k)
        {
            const double fk = k * f * std::sqrt (1.0 + B * k * k);
            if (fk > sr * 0.45) break;
            double a = std::pow ((double) k, -1.1) * std::exp (-(k - 1) * (1.0 - bright) * 0.55);
            if (k == 7) a *= 0.2; // le marteau frappe pres du 1/7 de la corde
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

    void renderRange (juce::AudioBuffer<float>& buf, int start, int end)
    {
        if (end <= start) return;
        auto* L = buf.getWritePointer (0);
        auto* R = buf.getNumChannels() > 1 ? buf.getWritePointer (1) : nullptr;
        float* pl[4] = { nullptr, layerBuf.getWritePointer (2), layerBuf.getWritePointer (4), layerBuf.getWritePointer (6) };
        float* pr[4] = { nullptr, layerBuf.getWritePointer (3), layerBuf.getWritePointer (5), layerBuf.getWritePointer (7) };
        player.renderRange (pl, pr, start, end);

        for (auto& v : voices)
        {
            if (! v.active) continue;
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

                sum *= layerGain[juce::jlimit (0, 3, v.channel)];
                L[i] += sum * v.panL;
                if (R != nullptr) R[i] += sum * v.panR;
            }

            // stabilite numerique des oscillateurs
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
};
