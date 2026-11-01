#include "DrumEditor.h"

namespace Col
{
const juce::Colour bg     { 0xff0b0a10 };
const juce::Colour panel  { 0xff15121d };
const juce::Colour grid   { 0xff241f2e };
const juce::Colour accent { 0xffe0234e };
const juce::Colour violet { 0xff9d2bff };
const juce::Colour text   { 0xffd9d3e6 };
const juce::Colour dim    { 0xff7a7288 };
const juce::Colour kick   { 0xffe0234e };
const juce::Colour snare  { 0xfff2a33a };
const juce::Colour hats   { 0xff27b8a4 };
const juce::Colour bass   { 0xff9d2bff };
const juce::Colour open   { 0xff7fd4ff };
const juce::Colour fill   { 0xffc9792c };
const juce::Colour perc   { 0xff8bd450 };
const juce::Colour fx     { 0xffb0b8ff };
}

//==============================================================================
DrumLookAndFeel::DrumLookAndFeel()
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

void DrumLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
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
    g.setGradientFill (juce::ColourGradient (Col::violet, c.x - r, c.y, Col::accent, c.x + r, c.y, false));
    g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (Col::panel.brighter (0.15f));
    g.fillEllipse (c.x - r * 0.62f, c.y - r * 0.62f, r * 1.24f, r * 1.24f);
    juce::Path p;
    p.addRoundedRectangle (-1.5f, -r * 0.6f, 3.0f, r * 0.35f, 1.5f);
    g.setColour (Col::text);
    g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

//==============================================================================
void DrumGrid::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Col::panel);
    g.fillRoundedRectangle (b, 6.0f);

    const char* names[] = { "FX", "OPEN", "HATS", "PERC", "RELANCE", "SNARE", "KICK", "808" };
    const juce::Colour cols[] = { Col::fx, Col::open, Col::hats, Col::perc, Col::fill, Col::snare, Col::kick, Col::bass };
    const std::vector<DrumHit>* lanes[] = { &res.fx, &res.openHats, &res.hats, &res.perc, &res.fills, &res.snare, &res.kick, &res.bass808 };
    constexpr int numLanes = 8;
    const float labelW = 64.0f;
    auto area = b.withTrimmedLeft (labelW);
    const float bw = area.getWidth() / (float) loopBeats;

    for (int i = 0; i <= (int) loopBeats * 12; ++i)
    {
        const float xx = area.getX() + (float) (i / 12.0) * bw;
        if (i % 48 == 0)      g.setColour (Col::accent.withAlpha (0.5f));
        else if (i % 12 == 0) g.setColour (Col::grid.brighter (0.4f));
        else if (i % 4 == 0)  g.setColour (Col::grid.withAlpha (0.6f));
        else continue;
        g.drawVerticalLine ((int) xx, b.getY(), b.getBottom());
    }

    const float laneH = b.getHeight() / (float) numLanes;
    for (int l = 0; l < numLanes; ++l)
    {
        const float y0 = b.getY() + l * laneH;
        g.setColour (Col::dim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (names[l], juce::Rectangle<float> (b.getX() + 6, y0, labelW - 8, laneH), juce::Justification::centredLeft);
        if (l > 0) { g.setColour (Col::grid); g.drawHorizontalLine ((int) y0, b.getX(), b.getRight()); }

        for (auto& h : *lanes[l])
        {
            const float x = area.getX() + (float) h.start * bw;
            const float v = (float) h.velocity / 127.0f;
            g.setColour (cols[l].withAlpha (0.35f + 0.65f * v));
            if (l == numLanes - 1)
            {
                // 808 : barre de duree, hauteur selon la note
                const float off = (float) juce::jlimit (-12, 24, h.pitch - 60) / 36.0f;
                const float yy = y0 + laneH * (0.7f - off * 0.8f);
                g.fillRoundedRectangle (x, yy - 3, juce::jmax (3.0f, (float) h.length * bw - 1), 6, 2);
            }
            else
            {
                const float off = (float) juce::jlimit (-12, 12, h.pitch - 60) / 12.0f;
                const float hh = laneH * (0.25f + 0.45f * v);
                const float yy = y0 + laneH * 0.5f - off * laneH * 0.25f;
                g.fillRect (x, yy - hh * 0.5f, juce::jmax (2.0f, bw / 14.0f), hh);
            }
        }
    }

    if (playhead >= 0.0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawVerticalLine ((int) (area.getX() + (float) playhead * bw), b.getY(), b.getBottom());
    }
}

//==============================================================================
void DrumDragZone::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (hover ? colour.withAlpha (0.25f) : Col::panel);
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (colour);
    g.drawRoundedRectangle (b, 5.0f, 1.2f);
    g.setColour (Col::text);
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawText (text, getLocalBounds(), juce::Justification::centred);
}

void DrumDragZone::mouseDrag (const juce::MouseEvent&)
{
    if (dragging) return;
    dragging = true;
    auto f = proc.writeTempMidiFile (part);
    if (f.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this,
                                                                    [this] { dragging = false; });
}

//==============================================================================
DarkTrapDrumsEditor::DarkTrapDrumsEditor (DarkTrapDrumsProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      dKick  (p, DarkTrapDrumsProcessor::Kick,  "KICK >>",  Col::kick),
      dSnare (p, DarkTrapDrumsProcessor::Snare, "SNARE >>", Col::snare),
      dFill  (p, DarkTrapDrumsProcessor::Fills, "RELANCE >>", Col::fill),
      dHats  (p, DarkTrapDrumsProcessor::Hats,  "HATS >>",  Col::hats),
      dOpen  (p, DarkTrapDrumsProcessor::OpenHat, "OPEN >>", Col::open),
      d808   (p, DarkTrapDrumsProcessor::Bass,  "808 >>",   Col::bass),
      dPerc  (p, DarkTrapDrumsProcessor::Perc,  "PERC >>",  Col::perc),
      dFx    (p, DarkTrapDrumsProcessor::Fx,    "FX >>",    Col::fx),
      dAll   (p, DarkTrapDrumsProcessor::All,   "TOUT >>",  Col::text)
{
    setLookAndFeel (&lnf);

    addCombo ("feel", "Rythme", 110);
    addCombo ("bars", "Mesures", 80);
    addCombo ("root", "Tonique (808)", 110);
    addCombo ("prog", "Accords (808)", 180);
    addToggle ("snareOn3", "Snare sur le 3");
    addToggle ("follow", "808 suit les accords MIDI");
    addToggle ("kitOn", "Kit");

    addCombo ("sndKick",   "Son kick",     108, true);
    addCombo ("sndSnare",  "Son snare",    108, true);
    addCombo ("sndClap",   "+ Clap",       108, true);
    addCombo ("sndFill",   "Son relances", 108, true);
    addCombo ("sndHat",    "Son hats",     108, true);
    addCombo ("sndOpen",   "Son open hat", 108, true);
    addCombo ("sndPerc",   "Son perc",     108, true);
    addCombo ("snd808",    "Son 808",      108, true);
    addCombo ("sndImpact", "Son impact",   108, true);
    addCombo ("sndRiser",  "Son riser",    108, true);

    addKnob ("hatDensity",  "Hats");
    addKnob ("rolls",       "Rolls");
    addKnob ("hatPitch",    "Pitch hats");
    addKnob ("kickDensity", "Kick");
    addKnob ("fills",       "Relances");
    addKnob ("openHat",     "Open hat");
    addKnob ("perc",        "Percs");
    addKnob ("fx",          "FX");
    addKnob ("bounce",      "808 bounce");
    addKnob ("seed",        "Seed");
    addKnob ("volume",      "Volume");

    addKnob ("volKick",  "Kick",    true);
    addKnob ("volSnare", "Snare",   true);
    addKnob ("volFill",  "Relances", true);
    addKnob ("volHat",   "Hats",    true);
    addKnob ("volOpen",  "Open hat", true);
    addKnob ("volPerc",  "Percs",   true);
    addKnob ("vol808",   "808",     true);
    addKnob ("volFx",    "FX",      true);
    mixLabel.setText ("MIX (0 = coupe, double-clic = 100 %)", juce::dontSendNotification);
    mixLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    mixLabel.setColour (juce::Label::textColourId, Col::dim);
    addAndMakeVisible (mixLabel);

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

    for (auto* c : std::initializer_list<juce::Component*> { &generateBtn, &previewBtn, &exportBtn,
                                                             &dKick, &dSnare, &dFill, &dHats, &dOpen, &d808, &dPerc, &dFx, &dAll, &grid, &info })
        addAndMakeVisible (c);
    info.setColour (juce::Label::textColourId, Col::dim);
    info.setFont (juce::FontOptions (12.0f));

    setSize (1240, 770);
    startTimerHz (30);
}

DarkTrapDrumsEditor::~DarkTrapDrumsEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DarkTrapDrumsEditor::addKnob (const juce::String& id, const juce::String& label, bool mix)
{
    auto k = std::make_unique<Knob>();
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    k->label.setText (label, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
    if (mix) k->slider.setDoubleClickReturnValue (true, 1.0);
    (mix ? mixKnobs : knobs).push_back (std::move (k));
}

void DarkTrapDrumsEditor::addCombo (const juce::String& id, const juce::String& label, int width, bool sound)
{
    auto c = std::make_unique<Combo>();
    c->width = width;
    if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c->box.addItemList (prm->choices, 1);
    c->label.setText (label, juce::dontSendNotification);
    c->label.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (c->box);
    addAndMakeVisible (c->label);
    c->att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, c->box);
    (sound ? soundCombos : combos).push_back (std::move (c));
}

void DarkTrapDrumsEditor::addToggle (const juce::String& id, const juce::String& label)
{
    auto t = std::make_unique<Toggle>();
    t->button.setButtonText (label);
    addAndMakeVisible (t->button);
    t->att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, t->button);
    toggles.push_back (std::move (t));
}

void DarkTrapDrumsEditor::exportMidi()
{
    auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile (proc.getPatternName() + ".mid");
    chooser = std::make_unique<juce::FileChooser> ("Exporter les drums en MIDI", start, "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              const auto base = f.getFileNameWithoutExtension();
                              bool ok = proc.writeMidiFile (f.withFileExtension ("mid"), DarkTrapDrumsProcessor::All);
                              const char* names[] = { "_kick", "_snare", "_hats", "_808", "_openhat", "_relances", "_percs", "_fx" };
                              for (int i = 0; i < 8; ++i)
                                  ok = ok && proc.writeMidiFile (f.getSiblingFile (base + names[i] + ".mid"),
                                                                 (DarkTrapDrumsProcessor::Part) i);
                              info.setText (ok ? "Exporte : " + base + " (+ 1 fichier par element)" : "Erreur d'export",
                                            juce::dontSendNotification);
                          });
}

void DarkTrapDrumsEditor::timerCallback()
{
    backBtn.setEnabled (proc.history.canBack());
    fwdBtn.setEnabled (proc.history.canForward());
    const int v = proc.getPatternVersion();
    if (v != lastVersion)
    {
        lastVersion = v;
        double L = 16.0;
        auto r = proc.getPatternCopy (L);
        juce::StringArray names;
        for (auto& c : r.chordNames) names.add (c);
        info.setText ("808 : " + names.joinIntoString (" - ") + "   |   " + juce::String ((int) r.hats.size()) + " hats, "
                      + juce::String ((int) r.kick.size()) + " kicks   |   " + proc.getPatternName(), juce::dontSendNotification);
        grid.setPattern (std::move (r), L);
    }
    const int fr = proc.getFollowedRoot();
    static const char* nn[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (fr >= 0) info.setText ("808 suit l'entree MIDI : " + juce::String (nn[fr]), juce::dontSendNotification);
    else if (info.getText().startsWith ("808 suit")) lastVersion = -1;
    grid.setPlayhead (proc.getPlayheadBeats());
}

void DarkTrapDrumsEditor::paint (juce::Graphics& g)
{
    g.fillAll (Col::bg);
    g.setGradientFill (juce::ColourGradient (Col::accent, 20.0f, 0.0f, Col::violet, 300.0f, 0.0f, false));
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("DARK TRAP DRUMS", 20, 12, 190, 32, juce::Justification::centredLeft);
}

void DarkTrapDrumsEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    auto top = area.removeFromTop (40);
    top.removeFromLeft (200);
    for (auto* d : { &dAll, &dFx, &d808, &dPerc, &dOpen, &dHats, &dFill, &dSnare, &dKick })
    {
        d->setBounds (top.removeFromRight (d == &dFill ? 80 : 64).reduced (2));
        top.removeFromRight (3);
    }
    top.removeFromRight (6);
    exportBtn.setBounds (top.removeFromRight (100).reduced (2));
    previewBtn.setBounds (top.removeFromRight (80).reduced (2));
    fwdBtn.setBounds (top.removeFromRight (30).reduced (2));
    generateBtn.setBounds (top.removeFromRight (95).reduced (2));
    backBtn.setBounds (top.removeFromRight (30).reduced (2));

    area.removeFromTop (10);
    auto row = area.removeFromTop (50);
    for (auto& c : combos)
    {
        auto col = row.removeFromLeft (c->width);
        c->label.setBounds (col.removeFromTop (18));
        c->box.setBounds (col.reduced (0, 2));
        row.removeFromLeft (12);
    }
    row.removeFromTop (18);
    const int tw[] = { 120, 200, 60 };
    for (size_t i = 0; i < toggles.size(); ++i)
        toggles[i]->button.setBounds (row.removeFromLeft (tw[i]));

    area.removeFromTop (6);
    auto srow = area.removeFromTop (46);
    for (auto& c : soundCombos)
    {
        auto col = srow.removeFromLeft (c->width);
        c->label.setBounds (col.removeFromTop (16));
        c->box.setBounds (col.reduced (0, 2));
        srow.removeFromLeft (12);
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
    area.removeFromTop (4);
    mixLabel.setBounds (area.removeFromTop (16));
    auto mixRow = area.removeFromTop (72);
    const int mw = juce::jmin (110, mixRow.getWidth() / (int) mixKnobs.size());
    for (auto& k : mixKnobs)
    {
        auto col = mixRow.removeFromLeft (mw);
        k->label.setBounds (col.removeFromTop (14));
        k->slider.setBounds (col);
    }
    area.removeFromTop (6);
    info.setBounds (area.removeFromBottom (22));
    grid.setBounds (area);
}

juce::AudioProcessorEditor* DarkTrapDrumsProcessor::createEditor()
{
    return new DarkTrapDrumsEditor (*this);
}
