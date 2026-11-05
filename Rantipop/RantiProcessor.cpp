#include "RantiProcessor.h"
#include "RantiEditor.h"

using namespace ranti;

namespace
{
const char* volIds[4] = { "volMel", "volChords", "volBass", "volExtra" };
const char* instIds[4]       = { "instMel", "instChords", "instBass", "instExtra" };
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
    const float defVol[4] = { 0.8f, 0.7f, 0.8f, 0.6f };
    const char* volNames[4] = { "Melody Vol", "Chords Vol", "808 Vol", "Counter Vol" };
    for (int i = 0; i < 4; ++i)
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
    applyState (Generator::randomState (cur.genre, keyChoice, cur.song, rng), true);
}

void RantiProcessor::goBack()    { if (canGoBack())    applyState (history[(size_t) --histPos], false); }
void RantiProcessor::goForward() { if (canGoForward()) applyState (history[(size_t) ++histPos], false); }

void RantiProcessor::setGenre (int g)
{
    if (! genreAvailable (g)) return;
    applyState (Generator::randomState (g, keyChoice, -1, rng), true);
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
    for (int i = 0; i < numSongs; ++i) if (songs[i].genre == g.genre) list.push_back (i);
    if (list.empty()) return;
    int idx = 0;
    for (size_t i = 0; i < list.size(); ++i) if (list[i] == g.song) idx = (int) i;
    idx = (idx + dir + (int) list.size()) % (int) list.size();
    g.song = list[(size_t) idx];
    g.cross = -1;
    for (int r = 0; r < 4; ++r) g.stem[(size_t) r] = 0;
    if (g.genre == 0 && g.bass808 < 0) g.bass808 = Generator::random808 (songs[g.song].bpm, rng);
    g.transpose = Generator::transposeFor (songs[g.song].key, keyChoice);
    applyState (g, true);
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
    static const char* names[] = { "melodie", "accords", "808", "contre_melo", "drums", "tout" };
    constexpr int res = 960;
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (res);
    for (int p = 0; p < NumParts; ++p)
    {
        if (part != 5 && part != p) continue;
        if (pat.parts[(size_t) p].empty()) continue;
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, juce::String (names[p])), 0);
        const int ch = p == PDrums ? 10 : 1;
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
    auto name = juce::File::createLegalFileName ("Rantipop " + pat.songName + " " + keyNames[pat.key] + (pat.minor ? "m" : "") + " " + names[part]);
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
    for (int i = 0; i < 4; ++i) st.inst[i] = (int) apvts.getRawParameterValue (instIds[i])->load();
    for (int i = 0; i < 4; ++i) st.vol[i] = apvts.getRawParameterValue (volIds[i])->load();
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
            const auto g = GenState::fromString (vt.getProperty ("gen", "").toString());
            if (g.song >= 0 && g.song < numSongs) applyState (g, true);
        }
}

juce::AudioProcessorEditor* RantiProcessor::createEditor() { return new RantiEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RantiProcessor(); }
