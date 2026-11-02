#include "PluginEditor.h"

namespace Col
{
const juce::Colour bg       = Raizer::space;
const juce::Colour panel    { 0x990a0d24 };
const juce::Colour grid     { 0xff1c2148 };
const juce::Colour accent   = Raizer::cyan;     // melodie
const juce::Colour blood    = Raizer::magenta;
const juce::Colour text     = Raizer::text;
const juce::Colour dim      = Raizer::dim;
const juce::Colour chord    { 0xff6b4dff };     // accords
const juce::Colour layer2   { 0xff58f0c0 };     // couche 2
}

//==============================================================================
void PianoRoll::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Col::panel);
    g.fillRoundedRectangle (b, 6.0f);

    int lo = 127, hi = 0;
    for (auto* v : { &melody, &chords, &counter })
        for (auto& n : *v) { lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch); }
    if (melody.empty() && chords.empty() && counter.empty()) { lo = 48; hi = 72; }
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

    g.setColour (Col::layer2.withAlpha (0.8f));
    for (auto& n : counter)
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
    const auto c = layer == DarkTripletAudioProcessor::Chords ? Col::chord
                 : layer == DarkTripletAudioProcessor::Counter ? Col::layer2 : Col::accent;
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
      dragMelo2 (p, DarkTripletAudioProcessor::Counter, "CONTRE-MELO >>"),
      dragChords (p, DarkTripletAudioProcessor::Chords, "ACCORDS >>")
{
    setLookAndFeel (&lnf);

    // Style : boite simple (pas d'attachment) -> un choix utilisateur charge le preset du style
    styleBox.addItemList (proc.apvts.getParameter ("style") != nullptr
                              ? dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("style"))->choices
                              : juce::StringArray(), 1);
    styleBox.setSelectedItemIndex ((int) proc.apvts.getRawParameterValue ("style")->load(), juce::dontSendNotification);
    styleBox.onChange = [this] { proc.applyStyle (styleBox.getSelectedItemIndex()); };
    styleLabel.setText ("Style", juce::dontSendNotification);
    styleLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    styleLabel.setColour (juce::Label::textColourId, Col::accent.brighter (0.4f));
    addAndMakeVisible (styleBox);
    addAndMakeVisible (styleLabel);

    addCombo ("motif",     "Moteur",      125);
    addCombo ("layer2",    "Contre-melo",    85);
    addCombo ("root",      "Tonique",     70);
    addCombo ("scale",     "Gamme / Mode", 170);
    addCombo ("prog",      "Progression", 150);
    addCombo ("chordMode", "Accords",     85);
    addCombo ("feel",      "Rythme",      85);
    addCombo ("bars",      "Mesures",     60);
    addCombo ("variation",   "Variation",     110, true);
    addCombo ("bank",        "Banque MIDI",   110, true);
    addCombo ("instMel",     "Son melodie",   170, true);
    addCombo ("instChords",  "Son accords",   170, true);
    addCombo ("instCounter", "Son contre-melo",  170, true);

    addKnob ("rotation", "Mode rot.");
    addKnob ("octave",   "Octave");
    addKnob ("range",    "Etendue");
    addKnob ("density",  "Densite");
    addKnob ("darkness", "Darkness");
    addKnob ("rolls",    "Runs");
    addKnob ("rests",    "Silences");
    addKnob ("octJumps", "Sauts 8ve");
    addKnob ("gate",     "Gate");
    addKnob ("seed",     "Seed");
    addKnob ("cutoff",   "Cutoff");
    addKnob ("release",  "Release");
    addKnob ("volume",   "Volume");
    addKnob ("volMel",     "Vol melodie", true);
    addKnob ("volChords",  "Vol accords", true);
    addKnob ("volCounter", "Vol contre-melo", true);

    generateBtn.onClick = [this] { proc.randomizeSeed(); };
    backBtn.onClick = [this] { proc.history.back(); };
    fwdBtn.onClick  = [this] { proc.history.forward(); };
    backBtn.setTooltip ("Revenir a la generation precedente");
    fwdBtn.setTooltip ("Generation suivante");
    addAndMakeVisible (backBtn);
    addAndMakeVisible (fwdBtn);
    previewBtn.setClickingTogglesState (true);
    previewBtn.setToggleState (proc.previewPlaying.load(), juce::dontSendNotification);
    previewBtn.onClick = [this] { proc.previewPlaying = previewBtn.getToggleState(); };
    exportBtn.onClick = [this] { exportMidi(); };
    synthAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "synthOn", synthToggle);

    for (auto* c : std::initializer_list<juce::Component*> { &generateBtn, &previewBtn, &exportBtn, &synthToggle,
                                                             &dragMelo, &dragMelo2, &dragChords, &roll, &chordLabel, &info })
        addAndMakeVisible (c);

    chordLabel.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    chordLabel.setColour (juce::Label::textColourId, Col::text);
    chordLabel.setJustificationType (juce::Justification::centredLeft);
    info.setColour (juce::Label::textColourId, Col::dim);
    info.setFont (juce::FontOptions (12.0f));
    info.setJustificationType (juce::Justification::centredRight);

    setSize (1120, 700);
    startTimerHz (30);
}

DarkTripletAudioProcessorEditor::~DarkTripletAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DarkTripletAudioProcessorEditor::addKnob (const juce::String& id, const juce::String& label, bool mix)
{
    auto k = std::make_unique<Knob>();
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    k->label.setText (label, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
    if (mix) k->slider.setDoubleClickReturnValue (true, 0.1);
    (mix ? mixKnobs : knobs).push_back (std::move (k));
}

void DarkTripletAudioProcessorEditor::addCombo (const juce::String& id, const juce::String& label, int width, bool secondRow)
{
    auto c = std::make_unique<Combo>();
    c->width = width;
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
    {
        // listes d'instruments "Groupe - Nom" : rangees par groupe avec un titre
        juce::String lastGroup;
        for (int i = 0; i < param->choices.size(); ++i)
        {
            const auto& item = param->choices[i];
            if (id.startsWith ("inst") && item.contains (" - "))
            {
                const auto g = item.upToFirstOccurrenceOf (" - ", false, false);
                if (g != lastGroup) { c->box.addSectionHeading (g); lastGroup = g; }
                c->box.addItem (item.fromFirstOccurrenceOf (" - ", false, false), i + 1);
            }
            else
                c->box.addItem (item, i + 1);
        }
    }
    c->label.setText (label, juce::dontSendNotification);
    c->label.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (c->box);
    addAndMakeVisible (c->label);
    c->att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, c->box);
    (secondRow ? combos2 : combos).push_back (std::move (c));
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
                              auto melo2  = f.getSiblingFile (f.getFileNameWithoutExtension() + "_contre_melo.mid");
                              const bool ok = proc.writeMidiFile (melo, DarkTripletAudioProcessor::Melody)
                                           && proc.writeMidiFile (accord, DarkTripletAudioProcessor::Chords)
                                           && proc.writeMidiFile (melo2, DarkTripletAudioProcessor::Counter);
                              info.setText (ok ? "Exporte : " + melo.getFileName() + " + " + accord.getFileName()
                                               : "Erreur d'export", juce::dontSendNotification);
                          });
}

void DarkTripletAudioProcessorEditor::timerCallback()
{
    backBtn.setEnabled (proc.history.canBack());
    fwdBtn.setEnabled (proc.history.canForward());
    const int v = proc.getPatternVersion();
    if (v != lastVersion)
    {
        lastVersion = v;
        double L = 16.0;
        auto mel = proc.getMelodyCopy (L);
        auto chd = proc.getChordsCopy();
        auto ctr = proc.getCounterCopy();
        chordLabel.setText (proc.getChordText(), juce::dontSendNotification);
        info.setText (juce::String ((int) mel.size()) + " notes  |  " + proc.getPatternName(), juce::dontSendNotification);
        roll.setNotes (std::move (mel), std::move (chd), std::move (ctr), L);
    }
    roll.setPlayhead (proc.getPlayheadBeats());
    {
        const double ph = proc.getPlayheadBeats();
        const float target = ph >= 0.0 ? (float) std::pow (1.0 - std::fmod (ph, 1.0), 3.0) : 0.0f;
        if (std::abs (target - pulse) > 0.01f) { pulse = target; repaint (0, 0, 90, 80); }
    }
    const int st = (int) proc.apvts.getRawParameterValue ("style")->load();
    if (st != styleBox.getSelectedItemIndex())
        styleBox.setSelectedItemIndex (st, juce::dontSendNotification);
}

void DarkTripletAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (bgImg.getWidth() != getWidth() || bgImg.getHeight() != getHeight())
        bgImg = Raizer::makeBackground (getWidth(), getHeight());
    g.drawImageAt (bgImg, 0, 0);
    for (auto& r : panelRects) if (! r.isEmpty()) Raizer::drawPanel (g, r);
    Raizer::drawOrb (g, { 40.0f, 36.0f }, 15.0f, pulse);
    Raizer::drawTitle (g, { 70.0f, 18.0f, 230.0f, 40.0f }, "RAIZER DA VST", "Melodies & accords - dark trap");
}

void DarkTripletAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    auto top = area.removeFromTop (40);
    top.removeFromLeft (290);
    dragChords.setBounds (top.removeFromRight (110).reduced (2));
    top.removeFromRight (4);
    dragMelo2.setBounds (top.removeFromRight (125).reduced (2));
    top.removeFromRight (4);
    dragMelo.setBounds (top.removeFromRight (90).reduced (2));
    top.removeFromRight (8);
    exportBtn.setBounds (top.removeFromRight (105).reduced (2));
    top.removeFromRight (4);
    previewBtn.setBounds (top.removeFromRight (85).reduced (2));
    top.removeFromRight (4);
    fwdBtn.setBounds (top.removeFromRight (30).reduced (2));
    generateBtn.setBounds (top.removeFromRight (100).reduced (2));
    backBtn.setBounds (top.removeFromRight (30).reduced (2));
    synthToggle.setBounds (top.removeFromRight (75).reduced (2));

    area.removeFromTop (10);
    auto comboRow = area.removeFromTop (50);
    panelRects[0] = comboRow.toFloat().expanded (8.0f, 4.0f);
    {
        auto col = comboRow.removeFromLeft (110);
        styleLabel.setBounds (col.removeFromTop (18));
        styleBox.setBounds (col.reduced (0, 2));
        comboRow.removeFromLeft (12);
    }
    for (auto& c : combos)
    {
        auto col = comboRow.removeFromLeft (c->width);
        c->label.setBounds (col.removeFromTop (18));
        c->box.setBounds (col.reduced (0, 2));
        comboRow.removeFromLeft (12);
    }

    area.removeFromTop (6);
    auto row2 = area.removeFromTop (68);
    panelRects[1] = row2.toFloat().expanded (8.0f, 2.0f);
    for (auto& c : combos2)
    {
        auto col = row2.removeFromLeft (c->width);
        c->label.setBounds (col.removeFromTop (16));
        c->box.setBounds (col.removeFromTop (30).reduced (0, 2));
        row2.removeFromLeft (12);
    }
    // volumes par couche (0 = coupe, double-clic = 100 %)
    for (auto& k : mixKnobs)
    {
        auto col = row2.removeFromLeft (juce::jmin (110, row2.getWidth() / 3));
        k->label.setBounds (col.removeFromTop (14));
        k->slider.setBounds (col);
    }

    area.removeFromTop (8);
    auto knobRow = area.removeFromTop (100);
    panelRects[2] = knobRow.toFloat().expanded (8.0f, 4.0f);
    const int kw = knobRow.getWidth() / (int) knobs.size();
    for (auto& k : knobs)
    {
        auto col = knobRow.removeFromLeft (kw);
        k->label.setBounds (col.removeFromTop (16));
        k->slider.setBounds (col);
    }

    area.removeFromTop (14);
    auto infoRow = area.removeFromBottom (24);
    chordLabel.setBounds (infoRow.removeFromLeft (infoRow.getWidth() / 2));
    info.setBounds (infoRow);
    area.removeFromBottom (4);
    roll.setBounds (area);
    panelRects[3] = area.toFloat().expanded (6.0f);
}

juce::AudioProcessorEditor* DarkTripletAudioProcessor::createEditor()
{
    return new DarkTripletAudioProcessorEditor (*this);
}
