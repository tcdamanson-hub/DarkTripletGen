#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Scales.h"

namespace
{
const juce::StringArray rootNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const int barChoices[] = { 1, 2, 4, 8 };

const char* genParamIds[] = { "root", "scale", "rotation", "octave", "bars", "density", "darkness",
                              "rolls", "rests", "octJumps", "range", "gate", "seed", "feel" };
}

DarkTripletAudioProcessor::DarkTripletAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (auto* id : genParamIds)
        apvts.addParameterListener (id, this);

    regenerate();
    startTimerHz (20);
}

DarkTripletAudioProcessor::~DarkTripletAudioProcessor()
{
    stopTimer();
    for (auto* id : genParamIds)
        apvts.removeParameterListener (id, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout DarkTripletAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    StringArray scaleNames;
    for (auto& s : getScales())
        scaleNames.add (s.name);

    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "root", 1 }, "Root", rootNames, 1));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scaleNames,
                                                          findScaleIndex ("Harmonic Minor")));
    ps.push_back (std::make_unique<AudioParameterInt> (ParameterID { "rotation", 1 }, "Mode Rotation", 0, 11, 0));
    ps.push_back (std::make_unique<AudioParameterInt> (ParameterID { "octave", 1 }, "Octave", 1, 7, 4));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "bars", 1 }, "Bars",
                                                          StringArray { "1", "2", "4", "8" }, 2));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "feel", 1 }, "Rhythm",
                                                          StringArray { "Triplets", "Straight", "Mix" }, 0));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "density", 1 },  "Density",     0.0f, 1.0f, 0.6f));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "darkness", 1 }, "Darkness",    0.0f, 1.0f, 0.8f));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "rolls", 1 },    "Rolls",       0.0f, 1.0f, 0.3f));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "rests", 1 },    "Rests",       0.0f, 1.0f, 0.25f));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "octJumps", 1 }, "Octave Jumps",0.0f, 1.0f, 0.15f));
    ps.push_back (std::make_unique<AudioParameterInt>   (ParameterID { "range", 1 },    "Range (oct)", 1, 3, 2));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 },     "Gate",        0.1f, 1.0f, 0.85f));
    ps.push_back (std::make_unique<AudioParameterInt>   (ParameterID { "seed", 1 },     "Seed",        1, 99999, 1337));

    ps.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "synthOn", 1 },  "Synth On", true));
    NormalisableRange<float> cutRange (200.0f, 12000.0f);
    cutRange.setSkewForCentre (1500.0f);
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "cutoff", 1 },   "Cutoff", cutRange, 2200.0f));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "glide", 1 },    "Glide (ms)", 0.0f, 200.0f, 25.0f));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "volume", 1 },   "Volume", 0.0f, 1.0f, 0.7f));

    return { ps.begin(), ps.end() };
}

GenParams DarkTripletAudioProcessor::readParams() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    GenParams p;
    p.root         = (int) v ("root");
    p.scaleIndex   = (int) v ("scale");
    p.rotation     = (int) v ("rotation");
    p.octave       = (int) v ("octave");
    p.bars         = barChoices[juce::jlimit (0, 3, (int) v ("bars"))];
    p.feel         = (int) v ("feel");
    p.density      = v ("density");
    p.darkness     = v ("darkness");
    p.rolls        = v ("rolls");
    p.rests        = v ("rests");
    p.octaveJumps  = v ("octJumps");
    p.rangeOctaves = (int) v ("range");
    p.gate         = v ("gate");
    p.seed         = (int) v ("seed");
    return p;
}

void DarkTripletAudioProcessor::regenerate()
{
    const auto params = readParams();
    auto notes = MelodyGenerator::generate (params);
    const double L = params.bars * 4.0;

    std::vector<Ev> evs;
    evs.reserve (notes.size() * 2);
    for (auto& n : notes)
    {
        evs.push_back ({ n.start, true, n.pitch, n.velocity });
        evs.push_back ({ juce::jmin (n.start + n.length, L - 1.0e-4), false, n.pitch, 0 });
    }
    std::sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b)
    {
        if (a.ppq != b.ppq) return a.ppq < b.ppq;
        return ! a.on && b.on; // note-off avant note-on au meme instant
    });

    {
        const juce::SpinLock::ScopedLockType sl (patternLock);
        pattern.swap (notes);
        events.swap (evs);
        loopBeats = L;
    }
    patternChanged = true;
    ++patternVersion;
}

void DarkTripletAudioProcessor::randomizeSeed()
{
    if (auto* p = apvts.getParameter ("seed"))
    {
        const int s = 1 + juce::Random::getSystemRandom().nextInt (99998);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) s));
        p->endChangeGesture();
    }
}

void DarkTripletAudioProcessor::timerCallback()
{
    if (needsRegen.exchange (false))
        regenerate();
}

std::vector<MelodyNote> DarkTripletAudioProcessor::getPatternCopy (double& loopBeatsOut)
{
    const juce::SpinLock::ScopedLockType sl (patternLock);
    loopBeatsOut = loopBeats;
    return pattern;
}

juce::String DarkTripletAudioProcessor::getPatternName() const
{
    const auto p = readParams();
    juce::String scale = getScales()[(size_t) juce::jlimit (0, (int) getScales().size() - 1, p.scaleIndex)].name;
    juce::String s = "DarkTriplet_" + rootNames[p.root] + "_" + scale;
    if (p.rotation != 0) s << "_mode" << p.rotation;
    s << (p.feel == 0 ? "_triplets" : p.feel == 1 ? "_straight" : "_mix");
    s << "_" << p.bars << "bars_seed" << p.seed;
    return juce::File::createLegalFileName (s.replaceCharacters (" #()", "_s__")) ;
}

bool DarkTripletAudioProcessor::writeMidiFile (const juce::File& file)
{
    double L = 16.0;
    auto notes = getPatternCopy (L);

    constexpr int ppqRes = 960;
    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    seq.addEvent (juce::MidiMessage::textMetaEvent (3, getPatternName()), 0);

    for (auto& n : notes)
    {
        const double on  = n.start * ppqRes;
        const double off = (n.start + n.length) * ppqRes;
        seq.addEvent (juce::MidiMessage::noteOn (1, n.pitch, (juce::uint8) n.velocity), on);
        seq.addEvent (juce::MidiMessage::noteOff (1, n.pitch), off);
    }
    seq.addEvent (juce::MidiMessage::endOfTrack(), L * ppqRes);
    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppqRes);
    mf.addTrack (seq);

    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk())
        return false;
    return mf.writeTo (out);
}

juce::File DarkTripletAudioProcessor::writeTempMidiFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("DarkTripletGen");
    dir.createDirectory();
    auto f = dir.getChildFile (getPatternName() + ".mid");
    writeMidiFile (f);
    return f;
}

void DarkTripletAudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    synth.prepare (sr, samplesPerBlock);
    wasRunning = false;
    activeNote = -1;
}

bool DarkTripletAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void DarkTripletAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    midi.clear(); // l'entree MIDI est ignoree : le plugin genere sa propre melodie

    const int numSamples = buffer.getNumSamples();
    double bpm = 140.0, ppq = 0.0;
    bool hostPlaying = false;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            hostPlaying = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    }

    const bool preview = ! hostPlaying && previewPlaying.load();
    if (preview) ppq = internalPpq;
    const bool running = hostPlaying || preview;

    const double beatsPerSample = bpm / 60.0 / sampleRate;
    const double blockBeats = numSamples * beatsPerSample;

    juce::MidiBuffer out;

    auto killActive = [&] (int samplePos)
    {
        if (activeNote >= 0)
        {
            out.addEvent (juce::MidiMessage::noteOff (1, activeNote), samplePos);
            activeNote = -1;
        }
    };

    if (patternChanged.exchange (false))
        killActive (0);

    if (! running)
    {
        if (wasRunning) killActive (0);
        wasRunning = false;
        internalPpq = 0.0;
        playheadBeats = -1.0;
    }
    else
    {
        if (wasRunning && std::abs (ppq - expectedPpq) > 0.01)
            killActive (0); // saut de position dans le transport

        const juce::SpinLock::ScopedLockType sl (patternLock);
        const double L = loopBeats;
        double pos = std::fmod (ppq, L);
        if (pos < 0.0) pos += L;
        playheadBeats = pos;

        double remaining = blockBeats, consumed = 0.0;
        while (remaining > 1.0e-12)
        {
            const double segEnd = juce::jmin (L, pos + remaining);
            for (const auto& e : events)
            {
                if (e.ppq < pos || e.ppq >= segEnd) continue;
                const int s = juce::jlimit (0, numSamples - 1,
                                            (int) ((consumed + e.ppq - pos) / beatsPerSample));
                if (e.on)
                {
                    if (activeNote >= 0) killActive (s);
                    out.addEvent (juce::MidiMessage::noteOn (1, e.pitch, (juce::uint8) e.vel), s);
                    activeNote = e.pitch;
                }
                else if (e.pitch == activeNote)
                {
                    killActive (s);
                }
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

    if (*apvts.getRawParameterValue ("synthOn") > 0.5f)
    {
        synth.setParams (apvts.getRawParameterValue ("cutoff")->load(),
                         apvts.getRawParameterValue ("glide")->load(),
                         apvts.getRawParameterValue ("volume")->load());
        synth.render (buffer, out);
    }

    midi.swapWith (out); // sortie MIDI vers le DAW
}

void DarkTripletAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DarkTripletAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    needsRegen = true; // la melodie est recalculee a l'identique grace au seed
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DarkTripletAudioProcessor();
}
