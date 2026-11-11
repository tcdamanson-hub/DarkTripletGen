#include "RantiDrumsProcessor.h"
#include "RantiDrumsEditor.h"

using namespace ranti;

namespace
{
const int gmDrum[NumLanes] = { 36, 38, 39, 42, 44, 46, 37, 63, 64, 76, 49 };
}

const char* RantiDrumsProcessor::laneParam (int lane)
{
    static const char* ids[NumLanes] = { "volKick", "volSnare", "volClap", "volHat", "volShaker", "volOpen", "volRim", "volPerc", "volPerc", "volPerc", "volCrash" };
    return ids[juce::jlimit (0, NumLanes - 1, lane)];
}

RantiDrumsProcessor::RantiDrumsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RANTIDRUMS", createLayout())
{
    clock.poll();
    previewPlaying = clock.running.load();
    startTimerHz (20);
    for (auto& h : laneHit) h = 0;
    GenState g;
    g.genre = 0;
    Generator::randomRnb (g, rng);
    Generator::randomKit (g, rng);
    applyState (g, true);
}

juce::AudioProcessorValueTreeState::ParameterLayout RantiDrumsProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    struct P { const char* id; const char* name; float def; };
    const P list[] = { { "volKick", "Kick", 1.0f }, { "volSnare", "Snare", 0.9f }, { "volClap", "Clap", 0.8f }, { "volRim", "Rim", 0.7f },
                       { "volHat", "Hi-Hat", 0.7f }, { "volOpen", "Open Hat", 0.6f }, { "volPerc", "Percus", 0.75f }, { "volShaker", "Shaker / Hat 2", 0.65f },
                       { "volCrash", "Crash", 0.6f }, { "master", "Master", 0.8f } };
    for (auto& p : list)
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { p.id, 1 }, p.name, NormalisableRange<float> (0.0f, 1.5f), p.def, pct));
    return { ps.begin(), ps.end() };
}

void RantiDrumsProcessor::applyState (const GenState& g, bool pushHistory)
{
    juce::String name;
    double len = (g.groove >= 0 && g.groove < numGrooves) ? grooves[g.groove].len : 16.0;
    auto h = g.genre == 0 ? buildRnbDrums (g, len, &name) : buildDrums (g.groove, len, &name);
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        state = g;
        hits.swap (h);
    }
    length = len;
    if (pushHistory)
    {
        if (histPos + 1 < (int) history.size()) history.erase (history.begin() + histPos + 1, history.end());
        history.push_back (g);
        if (history.size() > 100) history.erase (history.begin());
        histPos = (int) history.size() - 1;
    }
    ++version;
}

void RantiDrumsProcessor::generate()
{
    auto g = getState();
    const int prev = g.groove;
    if (g.genre == 0) Generator::randomRnb (g, rng);
    else
        for (int tries = 0; tries < 6; ++tries)
        {
            g.groove = Generator::randomGroove (g.genre, rng, hostBpm.load());
            if (g.groove != prev) break;
        }
    Generator::randomKit (g, rng);
    applyState (g, true);
}

void RantiDrumsProcessor::newKit()
{
    auto g = getState();
    Generator::randomKit (g, rng);
    applyState (g, true);
}

void RantiDrumsProcessor::goBack()    { if (canGoBack())    applyState (history[(size_t) --histPos], false); }
void RantiDrumsProcessor::goForward() { if (canGoForward()) applyState (history[(size_t) ++histPos], false); }

void RantiDrumsProcessor::setGenre (int genre)
{
    GenState g;
    g.genre = genre;
    if (genre == 0) Generator::randomRnb (g, rng);
    else g.groove = Generator::randomGroove (genre, rng, hostBpm.load());
    Generator::randomKit (g, rng);
    applyState (g, true);
}

void RantiDrumsProcessor::stepGroove (int dir)
{
    auto g = getState();
    if (g.genre == 0) { Generator::randomRnb (g, rng); applyState (g, true); return; }
    std::vector<int> list;
    for (int i = 0; i < numGrooves; ++i) if (grooves[i].genre == g.genre) list.push_back (i);
    if (list.empty()) return;
    int idx = 0;
    for (size_t i = 0; i < list.size(); ++i) if (list[i] == g.groove) idx = (int) i;
    idx = (idx + dir + (int) list.size()) % (int) list.size();
    const int before = g.groove;
    g.groove = list[(size_t) idx];
    // pop : si on change de groupe, on prend un kit du nouveau groupe
    if (g.genre == 2 && (before < 0 || grooves[before].group != grooves[g.groove].group)) Generator::randomKit (g, rng);
    // pop wave : si la machine n'a pas tous les elements du nouveau groove, on en prend une qui les a
    if (g.genre == 4 && ! Generator::kitCoversGroove (g)) Generator::randomKit (g, rng);
    applyState (g, true);
}

void RantiDrumsProcessor::setGroove (int gr)
{
    auto g = getState();
    const int before = g.groove;
    g.groove = gr;
    if (g.genre == 2 && gr >= 0 && (before < 0 || grooves[before].group != grooves[gr].group)) Generator::randomKit (g, rng);
    if (g.genre == 4 && ! Generator::kitCoversGroove (g)) Generator::randomKit (g, rng);
    applyState (g, true);
}

void RantiDrumsProcessor::setSample (int lane, int index)
{
    auto g = getState();
    g.kit[(size_t) lane] = index;
    applyState (g, true);
    pendingPad = lane;
}

GenState RantiDrumsProcessor::getState() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return state;
}

std::vector<PNote> RantiDrumsProcessor::getHits()
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return hits;
}

juce::File RantiDrumsProcessor::writeTempMidi()
{
    const auto h = getHits();
    const auto g = getState();
    constexpr int res = 960;
    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    seq.addEvent (juce::MidiMessage::textMetaEvent (3, "drums"), 0);
    for (auto& n : h)
    {
        const int p = gmDrum[juce::jlimit (0, NumLanes - 1, n.pitch)];
        seq.addEvent (juce::MidiMessage::noteOn (10, p, (juce::uint8) n.vel), n.start * res);
        seq.addEvent (juce::MidiMessage::noteOff (10, p), (n.start + 0.1) * res);
    }
    seq.addEvent (juce::MidiMessage::endOfTrack(), getLength() * res);
    seq.updateMatchedPairs();
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (res);
    mf.addTrack (seq);
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Rantipop");
    dir.createDirectory();
    const juce::String nm = g.genre == 0 ? juce::String ("RnB") : (g.groove >= 0 ? juce::String (grooves[g.groove].name) : juce::String ("groove"));
    auto f = dir.getChildFile (juce::File::createLegalFileName ("Rantipop_Drums_" + nm.replaceCharacter (' ', '_')) + ".mid");
    f.deleteFile();
    juce::FileOutputStream out (f);
    if (out.openedOk()) mf.writeTo (out);
    return f;
}

void RantiDrumsProcessor::prepareToPlay (double sr, int block)
{
    sampleRate = sr;
    engine.prepare (sr, block);
    wasRunning = false;
}

bool RantiDrumsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void RantiDrumsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const int numSamples = buffer.getNumSamples();
    double bpm = 120.0, ppq = 0.0;
    bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) { bpm = *b; hostBpm = bpm; }
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
    for (int i = 0; i < NumParts; ++i) st.vol[i] = 0.0f;
    st.vol[PDrums] = 1.0f;
    for (int l = 0; l < NumLanes; ++l) st.lane[l] = apvts.getRawParameterValue (laneParam (l))->load();
    st.reverb = 0.0f;
    st.master = apvts.getRawParameterValue ("master")->load();
    engine.setSettings (st);

    std::vector<std::pair<int, std::array<int, 4>>> evs;
    juce::MidiBuffer out;
    auto hit = [&] (int s, int lane, int vel)
    {
        evs.push_back ({ s, { 1, PDrums, lane, vel } });
        out.addEvent (juce::MidiMessage::noteOn (10, gmDrum[lane], (juce::uint8) vel), s);
        out.addEvent (juce::MidiMessage::noteOff (10, gmDrum[lane]), juce::jmin (numSamples - 1, s + 1));
        laneHit[(size_t) lane] = ++hitCounter;
    };

    const juce::SpinLock::ScopedLockType sl (lock);
    engine.setState (state);
    const int pad = pendingPad.exchange (-1);
    if (pad >= 0 && pad < NumLanes) hit (0, pad, 110);

    if (! running)
    {
        wasRunning = false;
        internalPpq = 0.0;
        playhead = -1.0;
    }
    else
    {
        const double L = length.load();
        double pos = std::fmod (ppq, L);
        if (pos < 0.0) pos += L;
        playhead = pos;
        double remaining = blockBeats, consumed = 0.0;
        while (remaining > 1.0e-12)
        {
            const double segEnd = juce::jmin (L, pos + remaining);
            for (const auto& e : hits)
            {
                if (e.start < pos || e.start >= segEnd) continue;
                const int s = juce::jlimit (0, numSamples - 1, (int) ((consumed + e.start - pos) / beatsPerSample));
                hit (s, e.pitch, e.vel);
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
    std::stable_sort (evs.begin(), evs.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
    engine.render (buffer, evs);
    meter = engine.getMeter();
    midi.swapWith (out);
}

void RantiDrumsProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto s = apvts.copyState();
    s.setProperty ("gen", getState().toString(), nullptr);
    if (auto xml = s.createXml()) copyXmlToBinary (*xml, destData);
}

void RantiDrumsProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (vt);
            auto g = GenState::fromString (vt.getProperty ("gen", "").toString());
            if (g.genre == 1 || g.genre == 4) { g.genre = 0; Generator::randomRnb (g, rng); Generator::randomKit (g, rng); }   // AFRO et POP WAVE supprimes
            if ((g.groove >= 0 && g.groove < numGrooves) || g.genre == 0) applyState (g, true);
        }
}

juce::AudioProcessorEditor* RantiDrumsProcessor::createEditor() { return new RantiDrumsEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RantiDrumsProcessor(); }
