#include "PluginEditor.h"

namespace Col
{
const juce::Colour bg       { 0xff0b0a10 };
const juce::Colour panel    { 0xff15121d };
const juce::Colour grid     { 0xff241f2e };
const juce::Colour accent   { 0xff9d2bff };
const juce::Colour blood    { 0xffe0234e };
const juce::Colour text     { 0xffd9d3e6 };
const juce::Colour dim      { 0xff7a7288 };
const juce::Colour chord    { 0xff3f6fd8 };
}

//==============================================================================
DarkLookAndFeel::DarkLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Col::text);
    setColour (juce::ComboBox::backgroundColourId, Col::panel);
    setColour (juce::ComboBox::outlineColourId, Col::grid);
    setColour (juce::ComboBox::textColourId, Col::text);
    setColour (juce::ComboBox::arrowColourId, Col::accent);
    setColour (juce::PopupMenu::backgroundColourId, Col::panel);
    setColour (juce::PopupMenu::textColourId, Col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Col::accent.withAlpha (0.5f));
    setColour (juce::TextButton::buttonColourId, Col::panel);
    setColour (juce::TextButton::buttonOnColourId, Col::accent);
    setColour (juce::TextButton::textColourOffId, Col::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::ToggleButton::textColourId, Col::text);
    setColour (juce::ToggleButton::tickColourId, Col::accent);
}

void DarkLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float a0, float a1, juce::Slider&)
{
    auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = a0 + pos * (a1 - a0);

    juce::Path track, val;
    track.addCentredArc (c.x, c.y, r - 3, r - 3, 0, a0, a1, true);
    val.addCentredArc (c.x, c.y, r - 3, r - 3, 0, a0, ang, true);
    g.setColour (Col::grid);
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (Col::accent, c.x - r, c.y, Col::blood, c.x + r, c.y, false));
    g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (Col::panel.brighter (0.15f));
    g.fillEllipse (c.x - r * 0.62f, c.y - r * 0.62f, r * 1.24f, r * 1.24f);
    juce::Path p;
    p.addRoundedRectangle (-1.5f, -r * 0.6f, 3.0f, r * 0.35f, 1.5f);
    g.setColour (Col::text);
    g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

//==============================================================================
void PianoRoll::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Col::panel);
    g.fillRoundedRectangle (b, 6.0f);

    int lo = 127, hi = 0;
    for (auto* v : { &melody, &chords })
        for (auto& n : *v) { lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch); }
    if (melody.empty() && chords.empty()) { lo = 48; hi = 72; }
    lo -= 2; hi += 2;
    const int rows = hi - lo + 1;
    const float rowH = b.getHeight() / (float) rows;
    const float bw = b.getWidth() / (float) loopBeats;

    for (int p = lo; p <= hi; ++p)
    {
        const int pc = p % 12;
        if (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10)
        {
            g.setColour (Col::bg.withAlpha (0.45f));
            g.fillRect (b.getX(), b.getBottom() - (float) (p - lo + 1) * rowH, b.getWidth(), rowH);
        }
    }

    for (int i = 0; i <= (int) loopBeats * 12; ++i)
    {
        const float xx = b.getX() + (float) (i / 12.0) * bw;
        if (i % 48 == 0)      g.setColour (Col::accent.withAlpha (0.55f));
        else if (i % 12 == 0) g.setColour (Col::grid.brighter (0.4f));
        else if (i % 4 == 0)  g.setColour (Col::grid.withAlpha (0.7f));
        else if (i % 3 == 0)  g.setColour (Col::grid.withAlpha (0.35f));
        else continue;
        g.drawVerticalLine ((int) xx, b.getY(), b.getBottom());
    }

    auto rect = [&] (const MelodyNote& n)
    {
        return juce::Rectangle<float> (b.getX() + (float) n.start * bw,
                                       b.getBottom() - (float) (n.pitch - lo + 1) * rowH,
                                       juce::jmax (2.0f, (float) n.length * bw - 1.0f), juce::jmax (1.0f, rowH - 1.0f));
    };

    g.setColour (Col::chord.withAlpha (0.55f));
    for (auto& n : chords)
        g.fillRoundedRectangle (rect (n), 2.0f);

    for (auto& n : melody)
    {
        const float v = (float) n.velocity / 127.0f;
        g.setColour (Col::accent.interpolatedWith (Col::blood, v * v));
        g.fillRoundedRectangle (rect (n), 2.0f);
    }

    if (playhead >= 0.0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawVerticalLine ((int) (b.getX() + (float) playhead * bw), b.getY(), b.getBottom());
    }
}

//==============================================================================
void MidiDragZone::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const auto c = layer == DarkTripletAudioProcessor::Chords ? Col::chord : Col::accent;
    g.setColour (hover ? c.withAlpha (0.25f) : Col::panel);
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (c);
    g.drawRoundedRectangle (b, 5.0f, 1.2f);
    g.setColour (Col::text);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (text, getLocalBounds(), juce::Justification::centred);
}

void MidiDragZone::mouseDrag (const juce::MouseEvent&)
{
    if (dragging) return;
    dragging = true;
    auto f = proc.writeTempMidiFile (layer);
    if (f.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this,
                                                                    [this] { dragging = false; });
}

//==============================================================================
DarkTripletAudioProcessorEditor::DarkTripletAudioProcessorEditor (DarkTripletAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      dragMelo (p, DarkTripletAudioProcessor::Melody, "MELO >>"),
      dragChords (p, DarkTripletAudioProcessor::Chords, "ACCORDS >>")
{
    setLookAndFeel (&lnf);

    addCombo ("root",      "Tonique",     80);
    addCombo ("scale",     "Gamme / Mode", 220);
    addCombo ("prog",      "Progression", 180);
    addCombo ("chordMode", "Accords",     100);
    addCombo ("feel",      "Rythme",      110);
    addCombo ("bars",      "Mesures",     80);

    addKnob ("rotation", "Mode rot.");
    addKnob ("octave",   "Octave");
    addKnob ("range",    "Etendue");
    addKnob ("density",  "Densite");
    addKnob ("darkness", "Darkness");
    addKnob ("rolls",    "Rolls");
    addKnob ("rests",    "Silences");
    addKnob ("octJumps", "Sauts 8ve");
    addKnob ("gate",     "Gate");
    addKnob ("seed",     "Seed");
    addKnob ("cutoff",   "Cutoff");
    addKnob ("release",  "Release");
    addKnob ("volume",   "Volume");

    generateBtn.onClick = [this] { proc.randomizeSeed(); };
    previewBtn.setClickingTogglesState (true);
    previewBtn.setToggleState (proc.previewPlaying.load(), juce::dontSendNotification);
    previewBtn.onClick = [this] { proc.previewPlaying = previewBtn.getToggleState(); };
    exportBtn.onClick = [this] { exportMidi(); };
    synthAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "synthOn", synthToggle);

    for (auto* c : std::initializer_list<juce::Component*> { &generateBtn, &previewBtn, &exportBtn, &synthToggle,
                                                             &dragMelo, &dragChords, &roll, &chordLabel, &info })
        addAndMakeVisible (c);

    chordLabel.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    chordLabel.setColour (juce::Label::textColourId, Col::text);
    chordLabel.setJustificationType (juce::Justification::centredLeft);
    info.setColour (juce::Label::textColourId, Col::dim);
    info.setFont (juce::FontOptions (12.0f));
    info.setJustificationType (juce::Justification::centredRight);

    setSize (1000, 590);
    startTimerHz (30);
}

DarkTripletAudioProcessorEditor::~DarkTripletAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DarkTripletAudioProcessorEditor::addKnob (const juce::String& id, const juce::String& label)
{
    auto k = std::make_unique<Knob>();
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    k->label.setText (label, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
    knobs.push_back (std::move (k));
}

void DarkTripletAudioProcessorEditor::addCombo (const juce::String& id, const juce::String& label, int width)
{
    auto c = std::make_unique<Combo>();
    c->width = width;
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c->box.addItemList (param->choices, 1);
    c->label.setText (label, juce::dontSendNotification);
    c->label.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (c->box);
    addAndMakeVisible (c->label);
    c->att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, c->box);
    combos.push_back (std::move (c));
}

void DarkTripletAudioProcessorEditor::exportMidi()
{
    auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile (proc.getPatternName() + ".mid");
    chooser = std::make_unique<juce::FileChooser> ("Exporter en MIDI (melodie + accords)", start, "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              if (! f.hasFileExtension ("mid")) f = f.withFileExtension ("mid");
                              auto melo   = f.getSiblingFile (f.getFileNameWithoutExtension() + "_melo.mid");
                              auto accord = f.getSiblingFile (f.getFileNameWithoutExtension() + "_accords.mid");
                              const bool ok = proc.writeMidiFile (melo, DarkTripletAudioProcessor::Melody)
                                           && proc.writeMidiFile (accord, DarkTripletAudioProcessor::Chords);
                              info.setText (ok ? "Exporte : " + melo.getFileName() + " + " + accord.getFileName()
                                               : "Erreur d'export", juce::dontSendNotification);
                          });
}

void DarkTripletAudioProcessorEditor::timerCallback()
{
    const int v = proc.getPatternVersion();
    if (v != lastVersion)
    {
        lastVersion = v;
        double L = 16.0;
        auto mel = proc.getMelodyCopy (L);
        auto chd = proc.getChordsCopy();
        chordLabel.setText (proc.getChordText(), juce::dontSendNotification);
        info.setText (juce::String ((int) mel.size()) + " notes  |  " + proc.getPatternName(), juce::dontSendNotification);
        roll.setNotes (std::move (mel), std::move (chd), L);
    }
    roll.setPlayhead (proc.getPlayheadBeats());
}

void DarkTripletAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Col::bg);
    g.setGradientFill (juce::ColourGradient (Col::accent, 20.0f, 0.0f, Col::blood, 320.0f, 0.0f, false));
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("DARK TRIPLET GEN", 20, 12, 300, 32, juce::Justification::centredLeft);
}

void DarkTripletAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    auto top = area.removeFromTop (40);
    top.removeFromLeft (290);
    dragChords.setBounds (top.removeFromRight (115).reduced (2));
    top.removeFromRight (4);
    dragMelo.setBounds (top.removeFromRight (100).reduced (2));
    top.removeFromRight (8);
    exportBtn.setBounds (top.removeFromRight (105).reduced (2));
    top.removeFromRight (4);
    previewBtn.setBounds (top.removeFromRight (85).reduced (2));
    top.removeFromRight (4);
    generateBtn.setBounds (top.removeFromRight (100).reduced (2));
    synthToggle.setBounds (top.removeFromRight (75).reduced (2));

    area.removeFromTop (10);
    auto comboRow = area.removeFromTop (50);
    for (auto& c : combos)
    {
        auto col = comboRow.removeFromLeft (c->width);
        c->label.setBounds (col.removeFromTop (18));
        c->box.setBounds (col.reduced (0, 2));
        comboRow.removeFromLeft (12);
    }

    area.removeFromTop (8);
    auto knobRow = area.removeFromTop (100);
    const int kw = knobRow.getWidth() / (int) knobs.size();
    for (auto& k : knobs)
    {
        auto col = knobRow.removeFromLeft (kw);
        k->label.setBounds (col.removeFromTop (16));
        k->slider.setBounds (col);
    }

    area.removeFromTop (6);
    auto infoRow = area.removeFromBottom (24);
    chordLabel.setBounds (infoRow.removeFromLeft (infoRow.getWidth() / 2));
    info.setBounds (infoRow);
    area.removeFromBottom (4);
    roll.setBounds (area);
}

juce::AudioProcessorEditor* DarkTripletAudioProcessor::createEditor()
{
    return new DarkTripletAudioProcessorEditor (*this);
}
