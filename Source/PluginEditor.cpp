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
    for (auto& n : notes) { lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch); }
    if (notes.empty()) { lo = 48; hi = 72; }
    lo -= 2; hi += 2;
    const int rows = hi - lo + 1;
    const float rowH = b.getHeight() / (float) rows;
    const float bw = b.getWidth() / (float) loopBeats;

    // touches noires
    for (int p = lo; p <= hi; ++p)
    {
        const int pc = p % 12;
        if (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10)
        {
            g.setColour (Col::bg.withAlpha (0.45f));
            g.fillRect (b.getX(), b.getBottom() - (float) (p - lo + 1) * rowH, b.getWidth(), rowH);
        }
    }

    // grille : mesures, temps, subdivisions
    for (int i = 0; i <= (int) loopBeats * 12; ++i)
    {
        const double beat = i / 12.0;
        const float xx = b.getX() + (float) beat * bw;
        if (i % 48 == 0)     g.setColour (Col::accent.withAlpha (0.55f));
        else if (i % 12 == 0) g.setColour (Col::grid.brighter (0.4f));
        else if (i % 4 == 0)  g.setColour (Col::grid.withAlpha (0.7f));   // triolets
        else if (i % 3 == 0)  g.setColour (Col::grid.withAlpha (0.35f));  // doubles
        else continue;
        g.drawVerticalLine ((int) xx, b.getY(), b.getBottom());
    }

    for (auto& n : notes)
    {
        juce::Rectangle<float> r (b.getX() + (float) n.start * bw,
                                  b.getBottom() - (float) (n.pitch - lo + 1) * rowH,
                                  juce::jmax (2.0f, (float) n.length * bw - 1.0f), rowH - 1.0f);
        const float v = (float) n.velocity / 127.0f;
        g.setColour (Col::accent.interpolatedWith (Col::blood, v * v));
        g.fillRoundedRectangle (r, 2.0f);
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
    g.setColour (hover ? Col::accent.withAlpha (0.25f) : Col::panel);
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (Col::accent);
    g.drawRoundedRectangle (b, 5.0f, 1.2f);
    g.setColour (Col::text);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("GLISSER MIDI >>", getLocalBounds(), juce::Justification::centred);
}

void MidiDragZone::mouseDrag (const juce::MouseEvent&)
{
    if (dragging) return;
    dragging = true;
    auto f = proc.writeTempMidiFile();
    if (f.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this,
                                                                    [this] { dragging = false; });
}

//==============================================================================
DarkTripletAudioProcessorEditor::DarkTripletAudioProcessorEditor (DarkTripletAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), dragZone (p)
{
    setLookAndFeel (&lnf);

    addCombo ("root",  "Tonique");
    addCombo ("scale", "Gamme / Mode");
    addCombo ("feel",  "Rythme");
    addCombo ("bars",  "Mesures");

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
    addKnob ("glide",    "Glide");
    addKnob ("volume",   "Volume");

    generateBtn.onClick = [this] { proc.randomizeSeed(); };
    previewBtn.setClickingTogglesState (true);
    previewBtn.setToggleState (proc.previewPlaying.load(), juce::dontSendNotification);
    previewBtn.onClick = [this] { proc.previewPlaying = previewBtn.getToggleState(); };
    exportBtn.onClick = [this] { exportMidi(); };
    synthAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "synthOn", synthToggle);

    for (auto* c : std::initializer_list<juce::Component*> { &generateBtn, &previewBtn, &exportBtn, &synthToggle, &dragZone, &roll, &info })
        addAndMakeVisible (c);

    info.setColour (juce::Label::textColourId, Col::dim);
    info.setFont (juce::FontOptions (12.0f));
    info.setJustificationType (juce::Justification::centredLeft);

    setSize (940, 560);
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

void DarkTripletAudioProcessorEditor::addCombo (const juce::String& id, const juce::String& label)
{
    auto c = std::make_unique<Combo>();
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
    chooser = std::make_unique<juce::FileChooser> ("Exporter la melodie en MIDI", start, "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              if (! f.hasFileExtension ("mid")) f = f.withFileExtension ("mid");
                              const bool ok = proc.writeMidiFile (f);
                              info.setText (ok ? "Exporte : " + f.getFullPathName() : "Erreur d'export",
                                            juce::dontSendNotification);
                          });
}

void DarkTripletAudioProcessorEditor::timerCallback()
{
    const int v = proc.getPatternVersion();
    if (v != lastVersion)
    {
        lastVersion = v;
        double L = 16.0;
        auto n = proc.getPatternCopy (L);
        info.setText (juce::String ((int) n.size()) + " notes  |  " + proc.getPatternName(), juce::dontSendNotification);
        roll.setNotes (std::move (n), L);
    }
    roll.setPlayhead (proc.getPlayheadBeats());
}

void DarkTripletAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Col::bg);
    g.setGradientFill (juce::ColourGradient (Col::accent, 20.0f, 0.0f, Col::blood, 320.0f, 0.0f, false));
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("DARK TRIPLET GEN", 20, 12, 320, 32, juce::Justification::centredLeft);
}

void DarkTripletAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    auto top = area.removeFromTop (40);
    top.removeFromLeft (310);
    dragZone.setBounds (top.removeFromRight (140).reduced (2));
    top.removeFromRight (6);
    exportBtn.setBounds (top.removeFromRight (110).reduced (2));
    top.removeFromRight (6);
    previewBtn.setBounds (top.removeFromRight (90).reduced (2));
    top.removeFromRight (6);
    generateBtn.setBounds (top.removeFromRight (110).reduced (2));
    synthToggle.setBounds (top.removeFromRight (80).reduced (2));

    area.removeFromTop (10);
    auto comboRow = area.removeFromTop (50);
    const int widths[] = { 90, 300, 140, 90 };
    for (size_t i = 0; i < combos.size(); ++i)
    {
        auto col = comboRow.removeFromLeft (widths[i]);
        combos[i]->label.setBounds (col.removeFromTop (18));
        combos[i]->box.setBounds (col.reduced (0, 2));
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

    info.setBounds (area.removeFromBottom (20));
    area.removeFromTop (8);
    roll.setBounds (area);
}

juce::AudioProcessorEditor* DarkTripletAudioProcessor::createEditor()
{
    return new DarkTripletAudioProcessorEditor (*this);
}
