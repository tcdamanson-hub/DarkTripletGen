#pragma once
#include <set>
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
    juce::File file, fileR;                // son d'un pack externe (charge a la demande) ; fileR = canal droit si enregistre en deux fichiers
    int loopStart = -1, loopEnd = -1;      // boucle de maintien (packs multi-echantillonnes)
    std::shared_ptr<std::atomic<juce::uint32>> useStamp;   // derniere utilisation (ms) : un son externe inutilise est libere
    std::shared_ptr<std::atomic<bool>> ready = std::make_shared<std::atomic<bool>> (false);
    bool isReady() const { return ready->load (std::memory_order_acquire); }
};

struct Instrument
{
    juce::String group, name;
    std::vector<Sample> zones; // triees par racine
    bool external = false;
    juce::String category;     // famille (packs externes) : Basses, Cordes, Leads...
    float minLen = 99.0f;      // duree (s) de son echantillon le plus court (packs avec durees.txt), 99 = inconnue
    std::shared_ptr<std::atomic<int>> loadState = std::make_shared<std::atomic<int>> (0); // 0 rien, 1 demande, 2 charge
    std::shared_ptr<std::atomic<juce::uint32>> useStamp = std::make_shared<std::atomic<juce::uint32>> (0u);

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
        static const juce::StringArray order { "Piano", "EPiano", "Keys", "Guitares", "Mallets", "Plucks", "Pads", "Cordes", "Bells", "Leads", "Synths", "Cuivres", "Bass" };
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
        scanExternalPacks (instIndex);
        for (auto& in : instruments) if (in.external) for (auto& z : in.zones) z.useStamp = in.useStamp;
        for (auto& c : drumCats)
            std::sort (c.second.begin(), c.second.end(), [] (const Sample& a, const Sample& b) { return a.name < b.name; });

        std::sort (instruments.begin(), instruments.end(), [] (const Instrument& a, const Instrument& b)
        {
            const int ga = groupOrder (a.group), gb = groupOrder (b.group);
            return ga != gb ? ga < gb : a.name < b.name;
        });
        melNames.add ("Ranti Piano Synthe");
        for (size_t i = 0; i < instruments.size(); ++i)
        {
            auto& in = instruments[i];
            std::sort (in.zones.begin(), in.zones.end(), [] (const Sample& a, const Sample& b) { return a.root < b.root; });
            if (in.group == "Bass" || in.group == "PopWaveBass") { bassIdx.push_back ((int) i); bassNames.add (in.name); }
            else                    { melIdx.push_back ((int) i);  melNames.add (in.name); }
        }
        if (bassNames.isEmpty()) bassNames.add ("-");
        // ordre de chargement : instruments puis tambours ; tout est pret en une ou deux secondes
        for (auto& in : instruments) if (! in.external) for (auto& z : in.zones) all.push_back (&z);
        for (auto& c : drumCats) for (auto& z : c.second) all.push_back (&z);
        loader = std::make_unique<Loader> (*this);
        loader->startThread (juce::Thread::Priority::low);
    }

    // packs externes : chaque son ramene au meme volume percu (RMS de la premiere seconde, grave allege comme l'oreille)
    // sur les notes du milieu de sa tessiture ; une seule valeur par son, donc l'equilibre entre ses notes est garde
    static void normalise (Instrument& in)
    {
        double acc = 0.0; int cnt = 0; float peak = 0.0f;
        const int lo = in.group == "PopWaveBass" ? 28 : 48, hi = in.group == "PopWaveBass" ? 52 : 76;
        for (auto& z : in.zones)
        {
            for (int c = 0; c < z.data.getNumChannels(); ++c) peak = juce::jmax (peak, z.data.getMagnitude (c, 0, z.data.getNumSamples()));
            if (z.root < lo || z.root > hi || z.data.getNumSamples() == 0) continue;
            const int n = juce::jmin (z.data.getNumSamples(), (int) z.sampleRate);
            const float* d = z.data.getReadPointer (0);
            float y = 0.0f, prev = 0.0f; double e = 0.0;
            const float k = in.group == "PopWaveBass" ? 0.0f : 0.991f;   // passe-haut ~60 Hz pour les sons non-basse
            for (int i = 0; i < n; ++i) { const float x = d[i]; y = k > 0.0f ? k * (y + x - prev) : x; prev = x; e += (double) y * y; }
            acc += e / n; ++cnt;
        }
        if (cnt == 0 || acc <= 0.0) return;
        const float rms = (float) std::sqrt (acc / cnt);
        float g = 0.2f / rms;                                    // -14 dBFS
        if (peak * g > 0.95f) g = 0.95f / peak;
        for (auto& z : in.zones) z.data.applyGain (g);
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
            // packs externes : chaque instrument est charge quand on le choisit (pas tout le pack en memoire)
            while (! threadShouldExit())
            {
                bool did = false;
                for (auto& in : bank.instruments)
                {
                    if (threadShouldExit()) return;
                    if (! in.external || in.loadState->load() != 1) continue;
                    for (auto& z : in.zones)
                    {
                        if (threadShouldExit()) return;
                        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (z.file));
                        if (reader != nullptr)
                        {
                            const int nch = (reader->numChannels > 1 || z.fileR.existsAsFile()) ? 2 : 1;
                            z.data.setSize (nch, (int) reader->lengthInSamples);
                            reader->read (&z.data, 0, (int) reader->lengthInSamples, 0, true, reader->numChannels > 1);
                            if (z.fileR.existsAsFile() && reader->numChannels == 1)
                            {
                                std::unique_ptr<juce::AudioFormatReader> rr (formats.createReaderFor (z.fileR));
                                if (rr != nullptr)
                                {
                                    juce::AudioBuffer<float> tmp (1, (int) reader->lengthInSamples);
                                    tmp.clear();
                                    rr->read (&tmp, 0, (int) juce::jmin (rr->lengthInSamples, reader->lengthInSamples), 0, true, false);
                                    z.data.copyFrom (1, 0, tmp, 0, 0, tmp.getNumSamples());
                                }
                                else z.data.copyFrom (1, 0, z.data, 0, 0, z.data.getNumSamples());
                            }
                            z.sampleRate = reader->sampleRate;
                            const auto& md = reader->metadataValues;
                            if (md.getValue ("NumSampleLoops", "0").getIntValue() > 0)
                            {
                                const int ls = md.getValue ("Loop0Start", "-1").getIntValue(), le = md.getValue ("Loop0End", "-1").getIntValue();
                                if (ls >= 0 && le > ls + 64 && le < (int) reader->lengthInSamples) { z.loopStart = ls; z.loopEnd = le; }
                            }
                        }
                        z.ready->store (true, std::memory_order_release);
                    }
                    normalise (in);
                    in.loadState->store (2);
                    did = true;
                }
                const auto nowMs = juce::Time::getMillisecondCounter();
                for (auto& in : bank.instruments)
                {
                    if (! in.external || in.loadState->load() != 2) continue;
                    if (nowMs - in.useStamp->load() < 90000u) continue;
                    for (auto& z : in.zones) z.ready->store (false, std::memory_order_release);
                    wait (300);   // laisse finir un eventuel bloc audio en cours
                    if (juce::Time::getMillisecondCounter() - in.useStamp->load() < 90000u)
                    {   // re-utilise entre-temps : on le garde
                        for (auto& z : in.zones) z.ready->store (true, std::memory_order_release);
                        continue;
                    }
                    for (auto& z : in.zones) { z.data.setSize (0, 0); z.loopStart = z.loopEnd = -1; }
                    in.loadState->store (0);
                }
                if (! did) wait (150);
            }
        }
    };
    std::vector<Sample*> all;
    std::unique_ptr<Loader> loader;

    // Packs externes : dossier "RANTIPOP Sounds" a cote des plugins (VST3) ou dans Documents.
    // Structure libre : .../<Categorie>/<Son>/<NN> <nom>.wav ou NN = note MIDI enregistree (multi-echantillons).
    // Un son par dossier ; categorie contenant "Bass" -> menu basse. Nom affiche = nom du dossier du son ("Ranti Wave 07", "Ranti Pop 112").
    static juce::Array<juce::File> externalRoots()
    {
        juce::Array<juce::File> roots;
        auto f = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        for (int up = 0; up < 6 && f.exists(); ++up)
        {
            f = f.getParentDirectory();
            // sons livres dans le plugin (RANTIPOP MELODIE.vst3/Contents/Resources/RANTIPOP Sounds) ou a cote
            for (auto c : { f.getChildFile ("Resources").getChildFile ("RANTIPOP Sounds"), f.getChildFile ("RANTIPOP Sounds") })
                if (c.isDirectory()) roots.addIfNotAlreadyThere (c);
        }
        for (auto loc : { juce::File::userDocumentsDirectory, juce::File::commonDocumentsDirectory })
        {
            auto c = juce::File::getSpecialLocation (loc).getChildFile ("RANTIPOP Sounds");
            if (c.isDirectory()) roots.addIfNotAlreadyThere (c);
        }
        // un seul emplacement : le premier trouve (dans le plugin d'abord), pas de sons en double
        if (roots.size() > 1) roots.removeRange (1, roots.size() - 1);
        return roots;
    }

    static juce::String familyName (const juce::String& c)
    {
        const auto l = c.toLowerCase();
        if (l.contains ("bass")) return "Basses";
        if (l.contains ("brass") || l.contains ("wind")) return "Cuivres & bois";
        if (l.contains ("string")) return "Cordes";
        if (l.contains ("pad")) return "Pads";
        if (l.contains ("lead")) return "Leads";
        if (l.contains ("organ")) return "Orgues";
        if (l.contains ("key") || l.contains ("piano")) return "Claviers";
        if (l.contains ("fx") || l.contains ("effect")) return "Effets";
        return c;
    }

    void scanExternalPacks (std::map<juce::String, size_t>& instIndex)
    {
        for (auto& sroot : externalRoots())
        {
            std::map<juce::String, float> packLen;   // durees.txt : "<Son>|<secondes>" (echantillon le plus court)
            std::set<juce::String> packsRead;
            for (const auto& entry : juce::RangedDirectoryIterator (sroot, true, "*.wav;*.flac", juce::File::findFiles))
            {
                const auto f = entry.getFile();
                const auto base = f.getFileNameWithoutExtension();
                // variantes (couches de velocite / doubles) : on garde la prise principale
                if (base.matchesWildcard ("*_0001", true) || base.matchesWildcard ("*_0002", true) || base.matchesWildcard ("*_0003", true)) continue;
                const auto lead = base.upToFirstOccurrenceOf (" ", false, false);
                if (! lead.containsOnly ("0123456789") || lead.isEmpty()) continue;
                const int root = lead.getIntValue();
                if (root < 0 || root > 127) continue;
                const auto patchDir = f.getParentDirectory();
                const auto cat = patchDir.getParentDirectory().getFileName();
                const juce::String group = cat.containsIgnoreCase ("bass") ? "PopWaveBass" : "PopWave";
                const auto pack = f.getRelativePathFrom (sroot).upToFirstOccurrenceOf (juce::File::getSeparatorString(), false, false);
                if (packsRead.insert (pack).second)
                {
                    juce::StringArray lines;
                    lines.addLines (sroot.getChildFile (pack).getChildFile ("durees.txt").loadFileAsString());
                    for (auto& ln : lines)
                        if (ln.contains ("|")) packLen[ln.upToFirstOccurrenceOf ("|", false, false)] = ln.fromFirstOccurrenceOf ("|", false, false).getFloatValue();
                }
                const juce::String name = patchDir.getFileName();   // "Ranti Wave 07", "Ranti Pop 112"...
                const auto key = group + "|" + name;
                auto it = instIndex.find (key);
                if (it == instIndex.end())
                {
                    Instrument in;
                    in.group = group; in.name = name; in.external = true; in.category = familyName (cat);
                    if (auto ml = packLen.find (name); ml != packLen.end()) in.minLen = ml->second;
                    instruments.push_back (std::move (in));
                    it = instIndex.emplace (key, instruments.size() - 1).first;
                }
                // stereo en deux fichiers ("... L OB ..." / "... R OB ...") : une seule zone, deux canaux
                const bool isL = base.contains (" L "), isR = base.contains (" R ");
                auto& zones = instruments[it->second].zones;
                if (isL || isR)
                {
                    Sample* z = nullptr;
                    for (auto& q : zones) if (q.root == root) { z = &q; break; }
                    if (z == nullptr) { Sample smp; smp.root = root; smp.name = base; zones.push_back (std::move (smp)); z = &zones.back(); }
                    (isL ? z->file : z->fileR) = f;
                    continue;
                }
                Sample smp;
                smp.file = f; smp.root = root; smp.name = base;
                zones.push_back (std::move (smp));
            }
        }
    }
public:
    static bool isLoaded (const Instrument* in) { return in == nullptr || ! in->external || in->loadState->load() == 2; }
    // famille d'un son (menu POP WAVE) ; vide pour les sons integres
    juce::String categoryOf (const juce::String& name) const
    {
        for (auto& in : instruments) if (in.name == name) return in.category;
        return {};
    }
    // a appeler (fil message) quand un instrument est choisi : le charge en arriere-plan s'il vient d'un pack externe
    void request (const Instrument* in)
    {
        if (in == nullptr || ! in->external) return;
        in->useStamp->store (juce::Time::getMillisecondCounter());
        int expected = 0;
        if (in->loadState->compare_exchange_strong (expected, 1) && loader != nullptr) loader->notify();
    }
    ~SoundBank() { if (loader != nullptr) loader->stopThread (4000); }
private:
};

// -----------------------------------------------------------------------------
//  Lecteur de samples polyphonique : transpose (Hermite), une sortie stereo par bus
// -----------------------------------------------------------------------------
class Sampler
{
public:
    static constexpr int numBuses = 7; // 0 melodie, 1 accords, 2 basse, 3 contre-melo, 4 drums, 5 arp, 6 pad

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

    // coupe tout ce qui sonne encore sur une piste (notes tenues ET queues de relache) : la note suivante arrive propre
    void cutBus (int bus, float release)
    {
        const float target = (float) std::exp (-1.0 / (juce::jmax (0.01f, release) * sr));
        for (auto& v : voices)
            if (v.active && v.bus == bus && (v.held || v.relMul > target)) startRelease (v, release);
    }

    void render (juce::AudioBuffer<float>& buses, int start, int end)
    {
        for (auto& v : voices)
        {
            if (! v.active) continue;
            float* outL = buses.getWritePointer (v.bus * 2);
            float* outR = buses.getWritePointer (v.bus * 2 + 1);
            if (v.s->useStamp != nullptr) v.s->useStamp->store (juce::Time::getMillisecondCounter(), std::memory_order_relaxed);
            if (! v.s->isReady() || v.s->data.getNumSamples() == 0) { v.active = false; continue; }
            const float* dL = v.s->data.getReadPointer (0);
            const float* dR = v.s->data.getReadPointer (v.s->data.getNumChannels() > 1 ? 1 : 0);
            const int len = v.s->data.getNumSamples();
            for (int i = start; i < end; ++i)
            {
                if (v.s->loopEnd > 0 && v.pos >= v.s->loopEnd) v.pos -= (v.s->loopEnd - v.s->loopStart);
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
