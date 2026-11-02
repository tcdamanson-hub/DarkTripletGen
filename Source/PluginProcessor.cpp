#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Scales.h"

namespace
{
const juce::StringArray rootNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const int barChoices[] = { 1, 2, 4, 8 };

const char* genParamIds[] = { "root", "scale", "rotation", "octave", "bars", "feel", "prog", "chordMode",
                              "density", "darkness", "rolls", "rests", "octJumps", "range", "gate", "seed", "style", "motif", "layer2", "bank", "variation" };
}

DarkTripletAudioProcessor::DarkTripletAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout()),
      history (apvts, std::vector<juce::String> (std::begin (genParamIds), std::end (genParamIds)))
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
    StringArray progNames;
    for (auto& s : MelodyGenerator::getProgressionNames())
        progNames.add (s);

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; });

    StringArray styleNames;
    for (auto& s : MelodyGenerator::getStyleNames())
        styleNames.add (s);
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "style", 3 }, "Style", styleNames, 1));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "motif", 10 }, "Engine",
                                                          StringArray { "Phrases (tes MIDI)", "Topline", "Melange" }, 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "layer2", 5 }, "Counter Melody",
                                                          StringArray { "Off", "On" }, 1));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "root", 2 }, "Root", rootNames, 9));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 2 }, "Scale", scaleNames,
                                                          findScaleIndex ("Aeolian (Natural Minor)")));
    ps.push_back (std::make_unique<AudioParameterInt>    (ParameterID { "rotation", 2 }, "Mode Rotation", 0, 11, 0));
    ps.push_back (std::make_unique<AudioParameterInt>    (ParameterID { "octave", 2 }, "Octave", 1, 7, 4));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "bars", 2 }, "Bars",
                                                          StringArray { "1", "2", "4", "8" }, 2));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "feel", 2 }, "Rhythm",
                                                          StringArray { "Triolets", "Normal", "Mix" }, 1));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "prog", 2 }, "Progression", progNames, 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "chordMode", 2 }, "Chords",
                                                          StringArray { "Off", "Tenus", "Rythmes" }, 1));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "density", 2 },  "Density",      NormalisableRange<float> (0.0f, 1.0f), 0.55f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "darkness", 2 }, "Darkness",     NormalisableRange<float> (0.0f, 1.0f), 0.4f,  pct));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "rolls", 2 },    "Runs",         NormalisableRange<float> (0.0f, 1.0f), 0.3f,  pct));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "rests", 2 },    "Rests",        NormalisableRange<float> (0.0f, 1.0f), 0.1f,  pct));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "octJumps", 2 }, "Octave Jumps", NormalisableRange<float> (0.0f, 1.0f), 0.15f, pct));
    ps.push_back (std::make_unique<AudioParameterInt>    (ParameterID { "range", 2 },    "Range (oct)", 1, 3, 2));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "gate", 2 },     "Gate",         NormalisableRange<float> (0.1f, 1.0f), 0.95f, pct));
    ps.push_back (std::make_unique<AudioParameterInt>    (ParameterID { "seed", 2 },     "Seed",        1, 99999, 1337));

    StringArray varNames;
    for (auto& s : MelodyGenerator::getVariationNames()) varNames.add (s);
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "variation", 9 }, "Variation", varNames, 0));
    StringArray bankNames;
    for (auto& s : MelodyGenerator::getBankNames()) bankNames.add (s);
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "bank", 7 }, "MIDI Bank", bankNames, 0));
    // instruments de pre-ecoute (tes one-shots)
    const auto instNames = InstrumentLibrary::menuNames();
    auto findInst = [&instNames] (const char* prefix, int fallback)
    {
        for (int i = 0; i < instNames.size(); ++i) if (instNames[i].startsWith (prefix)) return i;
        return fallback;
    };
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "instMel", 7 },     "Melody Sound",  instNames, findInst ("Bells", 0)));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "instChords", 7 },  "Chords Sound",  instNames, findInst ("Piano - ", 0)));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "instCounter", 7 }, "Counter Melody Sound", instNames, findInst ("Plucks", 0)));

    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "volMel", 9 },     "Melody Vol",  NormalisableRange<float> (0.0f, 1.5f), 0.1f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "volChords", 9 },  "Chords Vol",  NormalisableRange<float> (0.0f, 1.5f), 0.1f, pct));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "volCounter", 9 }, "Counter Melody Vol", NormalisableRange<float> (0.0f, 1.5f), 0.1f, pct));

    ps.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "synthOn", 2 },  "Synth On", true));
    NormalisableRange<float> cutRange (300.0f, 16000.0f);
    cutRange.setSkewForCentre (2000.0f);
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "cutoff", 2 }, "Cutoff", cutRange, 9000.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v)) + " Hz"; })));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "release", 2 }, "Release", NormalisableRange<float> (0.05f, 3.0f), 0.3f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " s"; })));
    ps.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "volume", 2 }, "Volume", NormalisableRange<float> (0.0f, 1.0f), 0.7f, pct));

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
    p.progression  = (int) v ("prog");
    p.chordMode    = (int) v ("chordMode");
    p.density      = v ("density");
    p.darkness     = v ("darkness");
    p.rolls        = v ("rolls");
    p.rests        = v ("rests");
    p.octaveJumps  = v ("octJumps");
    p.rangeOctaves = (int) v ("range");
    p.gate         = v ("gate");
    p.seed         = (int) v ("seed");
    p.style        = (int) v ("style");
    p.motif        = (int) v ("motif");
    p.layer2       = (int) v ("layer2");
    p.bank         = (int) v ("bank");
    p.variation    = (int) v ("variation");
    return p;
}

void DarkTripletAudioProcessor::regenerate()
{
    const auto params = readParams();
    auto res = MelodyGenerator::generate (params);
    const double L = params.bars * 4.0;

    std::vector<Ev> evs;
    auto addEvents = [&] (const std::vector<MelodyNote>& notes, int ch)
    {
        for (auto& n : notes)
        {
            evs.push_back ({ n.start, true, n.pitch, n.velocity, ch });
            evs.push_back ({ juce::jmin (n.start + n.length, L - 1.0e-4), false, n.pitch, 0, ch });
        }
    };
    addEvents (res.melody, 1);
    addEvents (res.chords, 2);
    addEvents (res.counter, 3);
    std::sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b)
    {
        if (a.ppq != b.ppq) return a.ppq < b.ppq;
        return ! a.on && b.on; // note-off avant note-on au meme instant
    });

    juce::StringArray names;
    for (auto& c : res.chordNames) names.add (c);

    {
        const juce::SpinLock::ScopedLockType sl (patternLock);
        melody.swap (res.melody);
        chords.swap (res.chords);
        counter.swap (res.counter);
        events.swap (evs);
        chordText = names.joinIntoString ("  -  ");
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
        history.beforeGenerate();
        if (auto* var = apvts.getParameter ("variation"))
            if (var->getValue() != 0.0f) { var->beginChangeGesture(); var->setValueNotifyingHost (0.0f); var->endChangeGesture(); }
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) s));
        p->endChangeGesture();
        history.afterGenerate();
    }
}

void DarkTripletAudioProcessor::applyStyle (int style)
{
    struct Preset { const char* scale; int feel, chordMode; float density, darkness, runs, rests, octJumps, gate; };
    static const Preset presets[] = {
        { nullptr,                    0, 0, 0,     0,    0,    0,    0,    0    }, // Perso : ne touche a rien
        { "Aeolian (Natural Minor)",  1, 1, 0.55f, 0.40f, 0.30f, 0.10f, 0.10f, 0.95f }, // Ninho
        { "Aeolian (Natural Minor)",  2, 1, 0.55f, 0.85f, 0.45f, 0.10f, 0.15f, 0.95f }, // Werenoi
        { "Phrygian",                 0, 2, 0.50f, 1.00f, 0.40f, 0.15f, 0.20f, 0.90f }, // Trap dark
        { "Aeolian (Natural Minor)",  0, 1, 0.65f, 0.70f, 0.50f, 0.05f, 0.15f, 0.95f }, // Triolets
    };
    style = juce::jlimit (0, 4, style);
    auto set = [this] (const char* id, float value)
    {
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
            prm->endChangeGesture();
        }
    };
    set ("style", (float) style);
    if (style == 0) return;
    const auto& pr = presets[style];
    set ("scale", (float) findScaleIndex (pr.scale));
    set ("rotation", 0.0f);
    set ("prog", 0.0f); // Auto : progressions typiques du style
    set ("feel", (float) pr.feel);
    set ("chordMode", (float) pr.chordMode);
    set ("density", pr.density);
    set ("darkness", pr.darkness);
    set ("rolls", pr.runs);
    set ("rests", pr.rests);
    set ("octJumps", pr.octJumps);
    set ("gate", pr.gate);
    set ("motif", 0.0f);
}

void DarkTripletAudioProcessor::timerCallback()
{
    if (needsRegen.exchange (false))
        regenerate();
}

std::vector<MelodyNote> DarkTripletAudioProcessor::getMelodyCopy (double& loopBeatsOut)
{
    const juce::SpinLock::ScopedLockType sl (patternLock);
    loopBeatsOut = loopBeats;
    return melody;
}

std::vector<MelodyNote> DarkTripletAudioProcessor::getChordsCopy()
{
    const juce::SpinLock::ScopedLockType sl (patternLock);
    return chords;
}

std::vector<MelodyNote> DarkTripletAudioProcessor::getCounterCopy()
{
    const juce::SpinLock::ScopedLockType sl (patternLock);
    return counter;
}

juce::String DarkTripletAudioProcessor::getChordText()
{
    const juce::SpinLock::ScopedLockType sl (patternLock);
    return chordText;
}

juce::String DarkTripletAudioProcessor::getPatternName() const
{
    const auto p = readParams();
    juce::String scale = getScales()[(size_t) juce::jlimit (0, (int) getScales().size() - 1, p.scaleIndex)].name;
    juce::String s = "DarkTriplet_" + rootNames[p.root] + "_" + scale;
    if (p.rotation != 0) s << "_mode" << p.rotation;
    s << (p.feel == 0 ? "_triolets" : p.feel == 1 ? "_normal" : "_mix");
    s << "_" << p.bars << "mes_seed" << p.seed;
    return juce::File::createLegalFileName (s.replaceCharacters (" #()", "_s__"));
}

bool DarkTripletAudioProcessor::writeMidiFile (const juce::File& file, Layer layer)
{
    double L = 16.0;
    auto mel = getMelodyCopy (L);
    auto chd = getChordsCopy();
    auto ctr = getCounterCopy();

    constexpr int ppqRes = 960;
    auto makeTrack = [&] (const std::vector<MelodyNote>& notes, int ch, const juce::String& name)
    {
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, name), 0);
        for (auto& n : notes)
        {
            seq.addEvent (juce::MidiMessage::noteOn (ch, n.pitch, (juce::uint8) n.velocity), n.start * ppqRes);
            seq.addEvent (juce::MidiMessage::noteOff (ch, n.pitch), (n.start + n.length) * ppqRes);
        }
        seq.addEvent (juce::MidiMessage::endOfTrack(), L * ppqRes);
        seq.updateMatchedPairs();
        return seq;
    };

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppqRes);
    if (layer == Melody || layer == Both) mf.addTrack (makeTrack (mel, 1, "Melodie"));
    if (layer == Counter)                 mf.addTrack (makeTrack (ctr, 1, "Melodie 2"));
    if (layer == Chords || layer == Both) mf.addTrack (makeTrack (chd, layer == Both ? 2 : 1, "Accords"));

    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk())
        return false;
    return mf.writeTo (out);
}

juce::File DarkTripletAudioProcessor::writeTempMidiFile (Layer layer)
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("DarkTripletGen");
    dir.createDirectory();
    auto f = dir.getChildFile (getPatternName() + (layer == Chords ? "_accords" : layer == Counter ? "_contre_melo" : "_melo") + ".mid");
    writeMidiFile (f, layer);
    return f;
}

void DarkTripletAudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    synth.prepare (sr, samplesPerBlock);
    wasRunning = false;
    for (auto& ch : active) for (auto& a : ch) a = false;
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
    midi.clear(); // l'entree MIDI est ignoree : le plugin genere sa propre musique

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

    auto killAll = [&] (int samplePos)
    {
        for (int c = 0; c < 3; ++c)
            for (int n = 0; n < 128; ++n)
                if (active[c][n])
                {
                    out.addEvent (juce::MidiMessage::noteOff (c + 1, n), samplePos);
                    active[c][n] = false;
                }
    };

    if (patternChanged.exchange (false))
        killAll (0);

    if (! running)
    {
        if (wasRunning) killAll (0);
        wasRunning = false;
        internalPpq = 0.0;
        playheadBeats = -1.0;
    }
    else
    {
        if (wasRunning && std::abs (ppq - expectedPpq) > 0.01)
            killAll (0); // saut de position dans le transport

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
                bool& a = active[e.ch - 1][e.pitch];
                if (e.on && apvts.getRawParameterValue (e.ch == 1 ? "volMel" : e.ch == 2 ? "volChords" : "volCounter")->load() <= 0.001f)
                    continue; // couche coupee : pas de note
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
        synth.setLayerGains (apvts.getRawParameterValue ("volMel")->load(),
                             apvts.getRawParameterValue ("volChords")->load(),
                             apvts.getRawParameterValue ("volCounter")->load());
        synth.setInstruments ((int) apvts.getRawParameterValue ("instMel")->load(),
                              (int) apvts.getRawParameterValue ("instChords")->load(),
                              (int) apvts.getRawParameterValue ("instCounter")->load());
        synth.setParams (apvts.getRawParameterValue ("cutoff")->load(),
                         apvts.getRawParameterValue ("release")->load(),
                         apvts.getRawParameterValue ("volume")->load());
        synth.render (buffer, out);
    }

    midi.swapWith (out); // sortie MIDI vers le DAW (canal 1 = melodie, canal 2 = accords)
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
