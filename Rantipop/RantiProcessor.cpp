#include "RantiProcessor.h"
#include "RantiEditor.h"

using namespace ranti;

namespace
{
// sons de base du POP WAVE : melodie, accords, contre-melo, basse, arp, pad
// tes reglages de base POP WAVE
const char* popWaveSounds[6] = { "PW Warm Trumpet", "PW Warm Keys", "PW Black Star Saw", "PW Tom Sawyer Bass", "PW Harp INC", "PW Murder She Wrote" }; // kit officiel OB From Mars (mel, accords, contre, basse, arp, pad)
// volumes POP WAVE : melodie, accords, basse, contre-melo, arp, pad (dans l'ordre des volIds)
const float popWaveVol[6] = { 0.32f, 0.28f, 0.41f, 0.48f, 0.39f, 0.40f }; // melodie, accords, basse, contre-melo, arp, pad
// sons de base de la POP (Essential WAV From Mars) : melodie, accords, contre-melo, basse, arp, pad
const char* popSounds[6] = { "POP Env Square Voyetra", "POP E Piano 360", "POP DX Punch S612", "POP Mono Bass DX100", "POP Short Pulse Micro", "POP Analog Strings Kawaii Dreams" };
// prefixe des sons de la selection d'un genre ("" : pas de selection propre)
const char* soundPrefix (int genre) { return genre == 4 ? "PW " : genre == 2 ? "POP " : ""; }
// tes reglages de base AFRO (meme ordre) : volumes
const float afroVol[6] = { 0.45f, 0.27f, 0.80f, 0.54f, 0.60f, 0.50f };
const float defaultVol[6] = { 0.80f, 0.70f, 0.80f, 0.60f, 0.60f, 0.50f };
// une entree par piste melodique : (piste, volume, son)
constexpr int numRows = 6;
const int rowPart[numRows]      = { PMel, PChords, PBass, PExtra, PArp, PPad };
const char* volIds[numRows]  = { "volMel", "volChords", "volBass", "volExtra", "volArp", "volPad" };
const char* instIds[numRows] = { "instMel", "instChords", "instBass", "instExtra", "instArp", "instPad" };
// sortie MIDI : canal par partie, drums sur le canal 10 (General MIDI)
const int gmDrum[NumLanes] = { 36, 38, 39, 42, 44, 46, 37, 63, 64, 76, 49 };
}

RantiProcessor::RantiProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RANTIPOP", createLayout())
{
    clock.poll();
    previewPlaying = clock.running.load();
    startTimerHz (20);
    // sons POP WAVE de base charges des le demarrage : le passage en POP WAVE sonne tout de suite
    {
        auto& bank = SoundBank::get();
        for (int i = 0; i < 6; ++i)
        {
            bank.request (i == 3 ? bank.bass (bank.findBass (popWaveSounds[i])) : bank.melodic (bank.findMelodic (popWaveSounds[i])));
            bank.request (i == 3 ? bank.bass (bank.findBass (popSounds[i])) : bank.melodic (bank.findMelodic (popSounds[i])));
        }
    }
    for (auto& k : keyCount) k = 0;
    // premier son : un R&B au hasard
    applyState (Generator::randomState (0, 0, -1, rng), true);
}

juce::AudioProcessorValueTreeState::ParameterLayout RantiProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    const auto& bank = SoundBank::get();
    const auto& mel = bank.melodicMenu();
    auto findM = [&] (const char* n, int fb) { const int i = mel.indexOf (n); return i < 0 ? fb : i; };
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "instMel", 1 },    "Melody Sound", mel, findM ("Piano 31", 0)));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "instChords", 1 }, "Chords Sound", mel, 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "instExtra", 1 },  "Counter Sound", mel, findM ("Rhodes Doux", 0)));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "instBass", 1 },   "808 Sound", bank.bassMenu(), juce::jmax (0, bank.bassMenu().indexOf ("808 All Alone"))));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "instArp", 1 },    "Arp Sound", mel, findM ("Pluck 01", 0)));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "instPad", 1 },    "Pad Sound", mel, findM ("Pad 18", 0)));
    const float defVol[numRows] = { 0.8f, 0.7f, 0.8f, 0.6f, 0.6f, 0.5f };
    const char* volNames[numRows] = { "Melody Vol", "Chords Vol", "808 Vol", "Counter Vol", "Arp Vol", "Pad Vol" };
    for (int i = 0; i < numRows; ++i)
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { volIds[i], 1 }, volNames[i], NormalisableRange<float> (0.0f, 1.5f), defVol[i], pct));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "reverb", 1 }, "Reverb", NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "master", 1 }, "Master", NormalisableRange<float> (0.0f, 1.5f), 0.8f, pct));
    return { ps.begin(), ps.end() };
}

// ------------------------------------------------------------------ generation
void RantiProcessor::applyState (const GenState& g, bool pushHistory)
{
    auto pat = Generator::build (g);
    std::vector<Ev> evs;
    for (int p = 0; p < NumParts; ++p)
        for (auto& n : pat.parts[(size_t) p])
        {
            evs.push_back ({ n.start, true, p, n.pitch, n.vel });
            evs.push_back ({ juce::jmin (n.start + n.length, pat.length - 1.0e-4), false, p, n.pitch, 0 });
        }
    std::stable_sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b)
    {
        if (a.beat != b.beat) return a.beat < b.beat;
        return ! a.on && b.on;
    });
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        state = g;
        pattern = std::move (pat);
        events.swap (evs);
    }
    if (pushHistory)
    {
        if (histPos + 1 < (int) history.size()) history.erase (history.begin() + histPos + 1, history.end());
        history.push_back (g);
        if (history.size() > 100) history.erase (history.begin());
        histPos = (int) history.size() - 1;
    }
    patternChanged = true;
    ++patternVersion;
}

void RantiProcessor::generate()
{
    const auto cur = getState();
    applyState (Generator::randomState (cur.genre, keyChoice, cur.song, rng, modeChoice), true);
    if (cur.genre == 4 || cur.genre == 2) shuffleFamilySounds();
}

// POP WAVE : a chaque generation, chaque piste prend un autre son de la meme famille (une autre basse, un autre lead...).
// Le nouveau son est d'abord charge en arriere-plan ; l'ancien joue jusqu'a ce qu'il soit pret (pas de blanc).
void RantiProcessor::shuffleFamilySounds()
{
    auto& bank = ranti::SoundBank::get();
    const juce::String prefix = soundPrefix (getState().genre);
    if (prefix.isEmpty()) return;
    for (int r = 0; r < numRows; ++r)
    {
        auto* prm = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (instIds[r]));
        if (prm == nullptr) continue;
        if (! prm->choices[prm->getIndex()].startsWith (prefix)) continue;
        const auto fam = bank.categoryOf (prm->choices[prm->getIndex()]);
        if (fam.isEmpty()) continue;
        // notes jouees par cette piste : le son doit avoir un echantillon a moins d'une octave de chacune
        // (sinon une seule prise etiree sur trois octaves : il reste dans le menu, mais n'est pas tire au hasard)
        static const int rowLo[numRows] = { 52, 45, 26, 41, 41, 48 }, rowHi[numRows] = { 77, 75, 48, 69, 70, 80 };   // notes des morceaux POP (Equinox)
        auto covers = [&] (int i)
        {
            const auto* in = rowPart[r] == ranti::PBass ? bank.bass (i) : bank.melodic (i);
            if (in == nullptr || in->zones.empty()) return false;
            if ((rowPart[r] == ranti::PChords || rowPart[r] == ranti::PPad) && in->minLen < 1.5f) return false;   // accords / nappe : notes tenues
            for (int n = rowLo[r]; n <= rowHi[r]; ++n)
            {
                int d = 999;
                for (auto& z : in->zones) d = juce::jmin (d, std::abs (z.root - n));
                if (d > 12) return false;
            }
            return true;
        };
        juce::Array<int> cand;
        for (int i = 0; i < prm->choices.size(); ++i)
            if (i != prm->getIndex() && prm->choices[i].startsWith (prefix) && bank.categoryOf (prm->choices[i]) == fam && (prefix != "POP " || covers (i))) cand.add (i);
        if (cand.isEmpty()) continue;
        const int pick = cand[rng.nextInt (cand.size())];
        pendingSound[r] = pick;
        bank.request (rowPart[r] == ranti::PBass ? bank.bass (pick) : bank.melodic (pick));
    }
}

void RantiProcessor::goBack()    { if (canGoBack())    applyState (history[(size_t) --histPos], false); }
void RantiProcessor::goForward() { if (canGoForward()) applyState (history[(size_t) ++histPos], false); }

void RantiProcessor::setGenre (int g)
{
    if (! genreAvailable (g)) return;
    const int before = getState().genre;
    applyState (Generator::randomState (g, keyChoice, -1, rng, modeChoice), true);
    if (g == 1 || g == 2 || g == 3 || g == 4 || before == 1 || before == 2 || before == 3 || before == 4) applyGenreSounds (g, before);
}

// afro : guitares pour les pistes de guitare, Rhodes pour les keys, 808 ; en quittant l'afro on remet les sons d'origine
void RantiProcessor::applyGenreSounds (int genre, int before)
{
    const auto& bank = SoundBank::get();
    auto set = [&] (const char* id, int index)
    {
        if (index < 0) return;
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
            p->endChangeGesture();
        }
    };
    auto setVol = [&] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        }
    };
    // volumes : equilibre POP WAVE en y entrant, volumes d'origine en en sortant
    if (genre == 4 || genre == 2)            for (int i = 0; i < numRows; ++i) setVol (volIds[i], popWaveVol[i]);
    else if (genre == 1)                     for (int i = 0; i < numRows; ++i) setVol (volIds[i], afroVol[i]);
    else if (before == 4 || before == 1 || before == 2) for (int i = 0; i < numRows; ++i) setVol (volIds[i], defaultVol[i]);
    auto pick = [&] (std::initializer_list<const char*> names) -> int
    {
        std::vector<int> c;
        for (auto* n : names) { const int i = bank.melodicMenu().indexOf (n); if (i > 0) c.push_back (i); }
        return c.empty() ? -1 : c[(size_t) rng.nextInt ((int) c.size())];
    };
    if (genre == 1)
    {
        // afro : tes reglages de base
        set ("instMel", bank.findMelodic ("Electric Key 3"));
        set ("instChords", bank.findMelodic ("Rhodes Doux"));
        set ("instExtra", bank.findMelodic ("Rhodes 01"));
        set ("instBass", bank.findBass ("Bass 62"));
        set ("instArp", bank.findMelodic ("Synth 20"));
        set ("instPad", bank.findMelodic ("Pad 67"));
    }
    else if (genre == 3)
    {
        // synthwave : tes reglages de base
        set ("instMel", bank.findMelodic ("Electric Key 2"));
        set ("instChords", bank.findMelodic ("Pad 18"));
        set ("instExtra", bank.findMelodic ("Synth 25"));
        set ("instBass", bank.findBass ("808 Senior"));
    }
    else if (genre == 4)
    {
        // pop wave : synthes 80s
        set ("instMel", bank.findMelodic (popWaveSounds[0]));
        set ("instChords", bank.findMelodic (popWaveSounds[1]));
        set ("instExtra", bank.findMelodic (popWaveSounds[2]));
        set ("instBass", bank.findBass (popWaveSounds[3]));
        set ("instArp", bank.findMelodic (popWaveSounds[4]));
        set ("instPad", bank.findMelodic (popWaveSounds[5]));
    }
    else if (genre == 2)
    {
        // pop : Essential WAV From Mars
        set ("instMel", bank.findMelodic (popSounds[0]));
        set ("instChords", bank.findMelodic (popSounds[1]));
        set ("instExtra", bank.findMelodic (popSounds[2]));
        set ("instBass", bank.findBass (popSounds[3]));
        set ("instArp", bank.findMelodic (popSounds[4]));
        set ("instPad", bank.findMelodic (popSounds[5]));
    }
    else
    {
        set ("instMel", bank.findMelodic ("Piano 31"));
        set ("instChords", 0);
        set ("instExtra", bank.findMelodic ("Rhodes Doux"));
        set ("instBass", bank.findBass ("808 All Alone"));
        // en sortant de la POP / POP WAVE : arp et pad d'origine (les sons de ces selections ne restent pas)
        if (before == 2 || before == 4)
        {
            set ("instArp", bank.findMelodic ("Pluck 01"));
            set ("instPad", bank.findMelodic ("Pad 18"));
        }
    }
}

void RantiProcessor::setGroove (int grooveIndex)
{
    auto g = getState();
    g.groove = grooveIndex;
    applyState (g, true);
}

void RantiProcessor::stepSong (int dir)
{
    auto g = getState();
    std::vector<int> list;
    for (int i = 0; i < numSongs; ++i) if (songs[i].genre == g.genre && modeMatches (i, modeChoice)) list.push_back (i);
    if (list.empty()) for (int i = 0; i < numSongs; ++i) if (songs[i].genre == g.genre) list.push_back (i);
    if (list.empty()) return;
    int idx = 0;
    for (size_t i = 0; i < list.size(); ++i) if (list[i] == g.song) idx = (int) i;
    idx = (idx + dir + (int) list.size()) % (int) list.size();
    g.song = list[(size_t) idx];
    for (auto& x : g.stem) x = 0;
    if (g.genre == 0 && g.bass808 < 0) g.bass808 = Generator::random808 (songs[g.song].bpm, rng);
    g.transpose = Generator::transposeFor (songs[g.song].key, keyChoice);
    applyState (g, true);
}

void RantiProcessor::setMode (int m)
{
    modeChoice = juce::jlimit (-3, NumModes - 1, m);
    const auto g = getState();
    // le MIDI actuel n'est pas dans cette gamme : on en prend un qui l'est
    if (g.song < 0 || ! modeMatches (g.song, modeChoice))
        applyState (Generator::randomState (g.genre, keyChoice, g.song, rng, modeChoice), true);
    else
        ++patternVersion;
}

void RantiProcessor::setKey (int k)
{
    keyChoice = juce::jlimit (0, 12, k);
    auto g = getState();
    if (g.song >= 0) g.transpose = Generator::transposeFor (songs[g.song].key, keyChoice);
    applyState (g, true);
}

GenState RantiProcessor::getState() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return state;
}

Pattern RantiProcessor::getPattern()
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return pattern;
}

std::array<bool, 128> RantiProcessor::getActiveKeys()
{
    std::array<bool, 128> a {};
    for (int i = 0; i < 128; ++i) a[(size_t) i] = keyCount[(size_t) i].load() > 0;
    return a;
}

juce::File RantiProcessor::writeTempMidi (int part)
{
    const auto pat = getPattern();
    static const char* names[] = { "melodie", "accords", "808", "contre_melo", "drums", "arp", "pad" };
    constexpr int res = 960;
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (res);
    for (int p = 0; p < NumParts; ++p)
    {
        if (part != allParts && part != p) continue;
        if (pat.parts[(size_t) p].empty()) continue;
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, juce::String (names[p])), 0);
        const int ch = p == PDrums ? 10 : 1 + p;
        for (auto& n : pat.parts[(size_t) p])
        {
            const int pitch = p == PDrums ? gmDrum[juce::jlimit (0, NumLanes - 1, n.pitch)] : n.pitch;
            seq.addEvent (juce::MidiMessage::noteOn (ch, pitch, (juce::uint8) n.vel), n.start * res);
            seq.addEvent (juce::MidiMessage::noteOff (ch, pitch), (n.start + (p == PDrums ? 0.1 : n.length)) * res);
        }
        seq.addEvent (juce::MidiMessage::endOfTrack(), pat.length * res);
        seq.updateMatchedPairs();
        mf.addTrack (seq);
    }
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Rantipop");
    dir.createDirectory();
    auto name = juce::File::createLegalFileName ("Rantipop " + pat.songName + " " + keyNames[pat.key] + (pat.minor ? "m" : "") + " "
                                                 + (part == allParts ? juce::String ("tout") : juce::String (names[juce::jlimit (0, NumParts - 1, part)])));
    auto f = dir.getChildFile (name.replaceCharacter (' ', '_') + ".mid");
    f.deleteFile();
    juce::FileOutputStream out (f);
    if (out.openedOk()) mf.writeTo (out);
    return f;
}

// ------------------------------------------------------------------ audio
void RantiProcessor::prepareToPlay (double sr, int block)
{
    sampleRate = sr;
    engine.prepare (sr, block);
    wasRunning = false;
    for (auto& p : active) for (auto& a : p) a = false;
    for (auto& k : keyCount) k = 0;
}

bool RantiProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void RantiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const int numSamples = buffer.getNumSamples();
    double bpm = 120.0, ppq = 0.0;
    bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            hostPlaying = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    const bool preview = ! hostPlaying && clock.running.load();
    if (preview)
    {
        // depart commun MELODIE + DRUMS, puis on avance a l'echantillon pres
        if (! previewWasOn) internalPpq = clock.beatsNow (bpm);
        ppq = internalPpq;
    }
    previewWasOn = preview;
    const bool running = hostPlaying || preview;
    const double beatsPerSample = bpm / 60.0 / sampleRate;
    const double blockBeats = numSamples * beatsPerSample;

    Engine::Settings st;
    for (int i = 0; i < numRows; ++i) st.inst[rowPart[i]] = (int) apvts.getRawParameterValue (instIds[i])->load();
    for (int i = 0; i < numRows; ++i) st.vol[rowPart[i]] = apvts.getRawParameterValue (volIds[i])->load();
    st.vol[PDrums] = 0.0f;
    st.reverb = apvts.getRawParameterValue ("reverb")->load();
    st.master = apvts.getRawParameterValue ("master")->load();
    engine.setSettings (st);

    std::vector<std::pair<int, std::array<int, 4>>> evs;
    juce::MidiBuffer out;
    auto send = [&] (int s, bool on, int part, int pitch, int vel)
    {
        evs.push_back ({ s, { on ? 1 : 0, part, pitch, vel } });
        const int ch = part == PDrums ? 10 : part + 1;
        const int mp = part == PDrums ? gmDrum[juce::jlimit (0, NumLanes - 1, pitch)] : pitch;
        if (on) out.addEvent (juce::MidiMessage::noteOn (ch, mp, (juce::uint8) vel), s);
        else    out.addEvent (juce::MidiMessage::noteOff (ch, mp), s);
        if (part != PDrums) keyCount[(size_t) pitch] += on ? 1 : -1;
    };
    auto killAll = [&] (int s)
    {
        for (int p = 0; p < NumParts; ++p)
            for (int n = 0; n < 128; ++n)
                if (active[p][n]) { send (s, false, p, n, 0); active[p][n] = false; }
        engine.allOff();
        for (auto& k : keyCount) k = 0;
    };

    if (patternChanged.exchange (false)) killAll (0);

    if (! running)
    {
        if (wasRunning) killAll (0);
        wasRunning = false;
        internalPpq = 0.0;
        playhead = -1.0;
    }
    else
    {
        if (wasRunning && std::abs (ppq - expectedPpq) > 0.01) killAll (0);
        const juce::SpinLock::ScopedLockType sl (lock);
        engine.setState (state);
        const double L = pattern.length;
        double pos = std::fmod (ppq, L);
        if (pos < 0.0) pos += L;
        playhead = pos;
        double remaining = blockBeats, consumed = 0.0;
        while (remaining > 1.0e-12)
        {
            const double segEnd = juce::jmin (L, pos + remaining);
            for (const auto& e : events)
            {
                if (e.beat < pos || e.beat >= segEnd) continue;
                const int s = juce::jlimit (0, numSamples - 1, (int) ((consumed + e.beat - pos) / beatsPerSample));
                bool& a = active[e.part][e.pitch];
                if (e.on)
                {
                    if (e.part != PDrums && a) send (s, false, e.part, e.pitch, 0);
                    send (s, true, e.part, e.pitch, e.vel);
                    a = e.part != PDrums;
                    if (e.part == PDrums) send (s, false, e.part, e.pitch, 0);
                }
                else if (a) { send (s, false, e.part, e.pitch, 0); a = false; }
            }
            const double used = segEnd - pos;
            consumed += used;
            remaining -= used;
            pos = (segEnd >= L) ? 0.0 : segEnd;
        }
        expectedPpq = ppq + blockBeats;
        wasRunning = true;
        if (preview) internalPpq += blockBeats;
    }
    for (auto& k : keyCount) if (k.load() < 0) k = 0;

    engine.render (buffer, evs);
    meter = engine.getMeter();
    midi.swapWith (out);
}

// ------------------------------------------------------------------ etat
void RantiProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto s = apvts.copyState();
    s.setProperty ("gen", getState().toString(), nullptr);
    s.setProperty ("key", keyChoice, nullptr);
    s.setProperty ("mode", modeChoice, nullptr);
    if (auto xml = s.createXml()) copyXmlToBinary (*xml, destData);
}

void RantiProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (vt);
            keyChoice = (int) vt.getProperty ("key", 0);
            modeChoice = juce::jlimit (-3, NumModes - 1, (int) vt.getProperty ("mode", -1));
            auto g = GenState::fromString (vt.getProperty ("gen", "").toString());
            if (g.genre == 3) g.genre = 4;   // la SYNTHWAVE est dans la POP WAVE
            if (g.song >= 0 && g.song < numSongs) applyState (g, true);
        }
}

juce::AudioProcessorEditor* RantiProcessor::createEditor() { return new RantiEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RantiProcessor(); }


void RantiProcessor::requestSelectedSounds()
{
    auto& bank = ranti::SoundBank::get();
    // sons choisis a la generation : on bascule des qu'ils sont charges
    for (int r = 0; r < numRows; ++r)
    {
        const int pick = pendingSound[r];
        if (pick < 0) continue;
        auto* in = rowPart[r] == ranti::PBass ? bank.bass (pick) : bank.melodic (pick);
        bank.request (in);
        if (! ranti::SoundBank::isLoaded (in)) continue;
        if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (instIds[r])))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) pick));
            prm->endChangeGesture();
        }
        pendingSound[r] = -1;
    }
    for (int i = 0; i < numRows; ++i)
    {
        const int idx = (int) apvts.getRawParameterValue (instIds[i])->load();
        bank.request (rowPart[i] == ranti::PBass ? bank.bass (idx) : bank.melodic (idx));
    }
}
