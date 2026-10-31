#include "DrumProcessor.h"
#include "DrumEditor.h"

namespace
{
const juce::StringArray rootNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const int barChoices[] = { 2, 4, 8 };
const char* genIds[] = { "feel", "bars", "root", "prog", "hatDensity", "rolls", "hatPitch", "kickDensity",
                         "fills", "snareOn3", "bounce", "seed", "openHat" };
const char* partNames[] = { "kick", "snare", "hats", "808", "openhat", "relances" };
}

DarkTrapDrumsProcessor::DarkTrapDrumsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (auto* id : genIds) apvts.addParameterListener (id, this);
    regenerate();
    startTimerHz (20);
}

DarkTrapDrumsProcessor::~DarkTrapDrumsProcessor()
{
    stopTimer();
    for (auto* id : genIds) apvts.removeParameterListener (id, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout DarkTrapDrumsProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    StringArray progNames;
    for (auto& s : DrumGenerator::getProgressionNames()) progNames.add (s);

    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "feel", 1 }, "Rhythm", StringArray { "Triolets", "Normal", "Mix" }, 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "bars", 1 }, "Bars", StringArray { "2", "4", "8" }, 1));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "root", 1 }, "Root", rootNames, 9));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "prog", 1 }, "Progression", progNames, 0));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "hatDensity", 1 }, "Hats", NormalisableRange<float> (0, 1), 0.55f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "rolls", 1 }, "Rolls", NormalisableRange<float> (0, 1), 0.6f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "hatPitch", 1 }, "Hat Pitch", NormalisableRange<float> (0, 1), 0.6f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "kickDensity", 1 }, "Kick", NormalisableRange<float> (0, 1), 0.5f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "fills", 1 }, "Snare Fills", NormalisableRange<float> (0, 1), 0.4f, pct));
    ps.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "snareOn3", 1 }, "Snare on 3", true));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "bounce", 1 }, "808 Bounce", NormalisableRange<float> (0, 1), 0.5f, pct));
    ps.push_back (std::make_unique<AudioParameterInt>    (ParameterID { "seed", 1 }, "Seed", 1, 99999, 1337));
    ps.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "follow", 1 }, "808 follows MIDI", true));
    ps.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "kitOn", 1 }, "Kit On", true));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "volume", 1 }, "Volume", NormalisableRange<float> (0, 1), 0.7f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "openHat", 1 }, "Open Hat", NormalisableRange<float> (0, 1), 0.35f, pct));
    // sons (tes samples)
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "sndKick", 1 },  "Kick Sound",  SampleLibrary::namesFor (SampleLibrary::KICK), 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "sndSnare", 1 }, "Snare Sound", SampleLibrary::namesFor (SampleLibrary::SNARE), 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "sndHat", 1 },   "Hat Sound",   SampleLibrary::namesFor (SampleLibrary::HAT), 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "sndOpen", 1 },  "Open Hat Sound", SampleLibrary::namesFor (SampleLibrary::OPENHAT), 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "snd808", 1 },   "808 Sound",   SampleLibrary::namesFor (SampleLibrary::BASS), 0));
    StringArray fillNames { "= Snare" };
    for (auto& n : SampleLibrary::namesFor (SampleLibrary::RIM)) fillNames.add ("Rim " + n);
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "sndFill", 1 },  "Fill Sound",  fillNames, 0));
    return { ps.begin(), ps.end() };
}

DrumParams DarkTrapDrumsProcessor::readParams() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    DrumParams p;
    p.feel        = (int) v ("feel");
    p.bars        = barChoices[juce::jlimit (0, 2, (int) v ("bars"))];
    p.root        = (int) v ("root");
    p.progression = (int) v ("prog");
    p.hatDensity  = v ("hatDensity");
    p.rolls       = v ("rolls");
    p.hatPitch    = v ("hatPitch");
    p.kickDensity = v ("kickDensity");
    p.fills       = v ("fills");
    p.snareOn3    = v ("snareOn3") > 0.5f;
    p.bounce808   = v ("bounce");
    p.seed        = (int) v ("seed");
    p.openHat     = v ("openHat");
    return p;
}

void DarkTrapDrumsProcessor::regenerate()
{
    const auto prm = readParams();
    auto res = DrumGenerator::generate (prm);
    const double L = prm.bars * 4.0;

    std::vector<Ev> evs;
    auto add = [&] (const std::vector<DrumHit>& v, int ch)
    {
        for (auto& h : v)
        {
            evs.push_back ({ h.start, true, h.pitch, h.velocity, ch });
            evs.push_back ({ juce::jmin (h.start + h.length, L - 1.0e-4), false, h.pitch, 0, ch });
        }
    };
    add (res.kick, 1); add (res.snare, 2); add (res.hats, 3); add (res.bass808, 4);
    add (res.openHats, 5); add (res.fills, 6);
    std::sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b)
    {
        if (a.ppq != b.ppq) return a.ppq < b.ppq;
        return ! a.on && b.on;
    });
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        pattern = std::move (res);
        events.swap (evs);
        loopBeats = L;
    }
    patternChanged = true;
    ++patternVersion;
}

void DarkTrapDrumsProcessor::randomizeSeed()
{
    if (auto* p = apvts.getParameter ("seed"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) (1 + juce::Random::getSystemRandom().nextInt (99998))));
        p->endChangeGesture();
    }
}

void DarkTrapDrumsProcessor::timerCallback()
{
    if (needsRegen.exchange (false)) regenerate();
}

DrumResult DarkTrapDrumsProcessor::getPatternCopy (double& L)
{
    const juce::SpinLock::ScopedLockType sl (lock);
    L = loopBeats;
    return pattern;
}

juce::String DarkTrapDrumsProcessor::getPatternName() const
{
    const auto p = readParams();
    juce::String s = "DarkTrapDrums_" + juce::String (p.feel == 0 ? "triolets" : p.feel == 1 ? "normal" : "mix")
                   + "_" + rootNames[p.root].replace ("#", "s") + "_" + juce::String (p.bars) + "mes_seed" + juce::String (p.seed);
    return juce::File::createLegalFileName (s);
}

bool DarkTrapDrumsProcessor::writeMidiFile (const juce::File& file, Part part)
{
    double L = 16.0;
    auto r = getPatternCopy (L);
    constexpr int ppq = 960;
    auto track = [&] (const std::vector<DrumHit>& v, const juce::String& name)
    {
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, name), 0);
        for (auto& h : v)
        {
            seq.addEvent (juce::MidiMessage::noteOn (1, h.pitch, (juce::uint8) h.velocity), h.start * ppq);
            seq.addEvent (juce::MidiMessage::noteOff (1, h.pitch), (h.start + h.length) * ppq);
        }
        seq.addEvent (juce::MidiMessage::endOfTrack(), L * ppq);
        seq.updateMatchedPairs();
        return seq;
    };
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);
    const std::vector<DrumHit>* parts[] = { &r.kick, &r.snare, &r.hats, &r.bass808, &r.openHats, &r.fills };
    const char* names[] = { "Kick", "Snare", "Hats", "808", "Open Hat", "Relances" };
    for (int i = 0; i < 6; ++i)
        if (part == All || part == i) mf.addTrack (track (*parts[i], names[i]));

    file.deleteFile();
    juce::FileOutputStream out (file);
    return out.openedOk() && mf.writeTo (out);
}

juce::File DarkTrapDrumsProcessor::writeTempMidiFile (Part part)
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("DarkTrapDrums");
    dir.createDirectory();
    auto f = dir.getChildFile (getPatternName() + "_" + (part == All ? "tout" : partNames[part]) + ".mid");
    writeMidiFile (f, part);
    return f;
}

void DarkTrapDrumsProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    kit.prepare (sr);
    wasRunning = false;
    for (auto& c : active) for (auto& a : c) a = false;
    bassSounding = -1;
}

bool DarkTrapDrumsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto o = layouts.getMainOutputChannelSet();
    return o == juce::AudioChannelSet::stereo() || o == juce::AudioChannelSet::mono();
}

void DarkTrapDrumsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    buffer.clear();

    // entree MIDI : accords envoyes par Dark Triplet Gen (ou un clavier) -> la 808 suit
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) heldInput[m.getNoteNumber()] = true;
        else if (m.isNoteOff()) heldInput[m.getNoteNumber()] = false;
        else if (m.isAllNotesOff()) for (auto& h : heldInput) h = false;
    }
    midi.clear();
    int lowest = -1;
    for (int n = 0; n < 128; ++n) if (heldInput[n]) { lowest = n; break; }
    const bool follow = *apvts.getRawParameterValue ("follow") > 0.5f;
    followedRoot = (follow && lowest >= 0) ? lowest % 12 : -1;

    const int num = buffer.getNumSamples();
    double bpm = 140.0, ppq = 0.0;
    bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            hostPlaying = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    const bool preview = ! hostPlaying && previewPlaying.load();
    if (preview) ppq = internalPpq;
    const bool running = hostPlaying || preview;
    const double bps = bpm / 60.0 / sampleRate, blockBeats = num * bps;

    juce::MidiBuffer out;
    auto killAll = [&] (int s)
    {
        for (int c = 0; c < 6; ++c)
            for (int n = 0; n < 128; ++n)
                if (active[c][n]) { out.addEvent (juce::MidiMessage::noteOff (c + 1, n), s); active[c][n] = false; }
        bassSounding = -1;
    };
    if (patternChanged.exchange (false)) killAll (0);

    if (! running)
    {
        if (wasRunning) killAll (0);
        wasRunning = false; internalPpq = 0.0; playheadBeats = -1.0;
    }
    else
    {
        if (wasRunning && std::abs (ppq - expectedPpq) > 0.01) killAll (0);
        const juce::SpinLock::ScopedLockType sl (lock);
        const double L = loopBeats;
        double pos = std::fmod (ppq, L); if (pos < 0) pos += L;
        playheadBeats = pos;
        double remaining = blockBeats, consumed = 0.0;
        while (remaining > 1.0e-12)
        {
            const double segEnd = juce::jmin (L, pos + remaining);
            for (const auto& e : events)
            {
                if (e.ppq < pos || e.ppq >= segEnd) continue;
                const int s = juce::jlimit (0, num - 1, (int) ((consumed + e.ppq - pos) / bps));
                if (e.ch == 4)
                {
                    // 808 monophonique, eventuellement re-accordee sur l'entree MIDI
                    if (e.on)
                    {
                        int pitch = e.pitch;
                        if (followedRoot.load() >= 0)
                        {
                            const int oct = e.pitch > 66 ? 12 : 0;
                            pitch = 60 + followedRoot.load();
                            if (pitch > 66) pitch -= 12;
                            pitch += oct;
                        }
                        if (bassSounding >= 0) { out.addEvent (juce::MidiMessage::noteOff (4, bassSounding), s); active[3][bassSounding] = false; }
                        out.addEvent (juce::MidiMessage::noteOn (4, pitch, (juce::uint8) e.vel), s);
                        active[3][pitch] = true;
                        bassSounding = pitch;
                    }
                    else if (bassSounding >= 0)
                    {
                        // la note-off de la 808 coupe la note qui sonne, sauf si une nouvelle a deja demarre
                        bool sameStart = false;
                        for (const auto& e2 : events) if (e2.ch == 4 && e2.on && std::abs (e2.ppq - e.ppq) < 1.0e-6) sameStart = true;
                        if (! sameStart)
                        {
                            out.addEvent (juce::MidiMessage::noteOff (4, bassSounding), s);
                            active[3][bassSounding] = false;
                            bassSounding = -1;
                        }
                    }
                    continue;
                }
                bool& a = active[e.ch - 1][e.pitch];
                if (e.on)
                {
                    if (a) out.addEvent (juce::MidiMessage::noteOff (e.ch, e.pitch), s);
                    out.addEvent (juce::MidiMessage::noteOn (e.ch, e.pitch, (juce::uint8) e.vel), s);
                    a = true;
                }
                else if (a)
                {
                    out.addEvent (juce::MidiMessage::noteOff (e.ch, e.pitch), s);
                    a = false;
                }
            }
            const double used = segEnd - pos;
            consumed += used; remaining -= used;
            pos = (segEnd >= L) ? 0.0 : segEnd;
        }
        expectedPpq = ppq + blockBeats;
        wasRunning = true;
        if (preview) internalPpq += blockBeats;
    }

    if (*apvts.getRawParameterValue ("kitOn") > 0.5f)
    {
        auto iv = [this] (const char* id) { return (int) apvts.getRawParameterValue (id)->load(); };
        kit.setParams (apvts.getRawParameterValue ("volume")->load(), iv ("sndKick"), iv ("sndSnare"),
                       iv ("sndHat"), iv ("sndOpen"), iv ("snd808"), iv ("sndFill"));
        kit.render (buffer, out);
    }
    midi.swapWith (out); // canal 1 kick, 2 snare, 3 hats, 4 808, 5 open hat, 6 relances
}

void DarkTrapDrumsProcessor::getStateInformation (juce::MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}

void DarkTrapDrumsProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    needsRegen = true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DarkTrapDrumsProcessor();
}
