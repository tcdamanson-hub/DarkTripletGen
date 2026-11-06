#include "RantiSpace.h"
#include "RantiDrumsEditor.h"
#include "RantiSamples.h"

using namespace ranti;
using namespace dtheme;

namespace
{
constexpr int W = 1180, H = 740;
// pads du bas : piste(s) jouee(s)
const int padLane[10] = { LKick, LSnare, LClap, LHat, LOpen, LRim, LPercA, LPercB, LHat2, LCrash };

juce::Font font (float size, float kerning = 0.0f, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (kerning);
}

void glass (juce::Graphics& g, juce::Rectangle<float> r, float radius = 12.0f)
{
    g.setColour (juce::Colour (0x0f78a0ff));
    g.fillRoundedRectangle (r, radius);
    g.setColour (juce::Colour (0x33a0beff));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

void spaced (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float size, float kerning,
             juce::Justification j, juce::Colour c, bool bold = false)
{
    g.setColour (c);
    g.setFont (font (size, kerning, bold));
    g.drawText (t, r, j);
}

juce::String sampleName (int genre, int lane, int clapVariant, int idx)
{
    const auto* set = SoundBank::get().drums (drumCategory (genre, lane, clapVariant));
    if (set == nullptr || set->empty()) return "-";
    return (*set)[(size_t) juce::jlimit (0, (int) set->size() - 1, idx)].name;
}
}

// =============================================================================
void DrumLook::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                 juce::Slider::SliderStyle, juce::Slider&)
{
    const auto r = juce::Rectangle<float> ((float) x, (float) y + h * 0.15f, (float) w, h * 0.7f);
    juce::Path path;
    path.startNewSubPath (r.getX(), r.getBottom() - 2.0f);
    path.cubicTo (r.getX() + r.getWidth() * 0.3f, r.getBottom() - 2.0f, r.getX() + r.getWidth() * 0.5f, r.getY() + 3.0f, r.getRight(), r.getY() + 2.0f);
    g.setColour (dim);
    g.strokePath (path, juce::PathStrokeType (2.0f));
    const float t = juce::jlimit (0.0f, 1.0f, (pos - (float) x) / (float) juce::jmax (1, w));
    const float len = path.getLength();
    juce::Path active;
    for (int i = 0; i <= 48; ++i)
    {
        const auto pt = path.getPointAlongPath (len * t * (float) i / 48.0f);
        if (i == 0) active.startNewSubPath (pt); else active.lineTo (pt);
    }
    g.setColour (cyan.withAlpha (0.3f));
    g.strokePath (active, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (cyan, r.getX(), 0, violet, r.getRight(), 0, false));
    g.strokePath (active, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto th = path.getPointAlongPath (len * t);
    g.setColour (cyan.withAlpha (0.4f));
    g.fillEllipse (th.x - 9.0f, th.y - 9.0f, 18.0f, 18.0f);
    g.setColour (hi);
    g.fillEllipse (th.x - 5.5f, th.y - 5.5f, 11.0f, 11.0f);
}

void DrumLook::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (juce::Colour (0xf2070b22));
    g.setColour (dim);
    g.drawRect (0, 0, w, h, 1);
}

void DrumLook::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                  bool isTicked, bool, const juce::String& txt, const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator) { g.setColour (low); g.fillRect (area.reduced (8, area.getHeight() / 2).withHeight (1)); return; }
    if (isHighlighted && isActive) { g.setColour (cyan.withAlpha (0.18f)); g.fillRect (area); }
    g.setColour (isTicked ? hi : text.withAlpha (isActive ? 0.9f : 0.35f));
    g.setFont (font (14.0f, 0.04f, isTicked));
    g.drawText ((isTicked ? juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa2 ")) : "  ") + txt, area.reduced (12, 0), juce::Justification::centredLeft);
}

// =============================================================================
DrumRow::DrumRow (RantiDrumsProcessor& p, int l, const juce::String& t, const char* volId) : lane (l), title (t), proc (p)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    if (auto* prm = proc.apvts.getParameter (volId))
        att = std::make_unique<juce::SliderParameterAttachment> (*prm, slider);
}

void DrumRow::resized()
{
    slider.setBounds (0, 40, getWidth(), 34);
    subArea = { 0, 20, getWidth() - 60, 18 };
}

void DrumRow::paint (juce::Graphics& g)
{
    const auto st = proc.getState();
    juce::String t = title;
    if (lane == LHat2) t = st.genre != 3 ? "SHAKER" : "HAT 2";
    spaced (g, t, { 0.0f, 0.0f, (float) getWidth(), 18.0f }, 13.0f, 0.22f, juce::Justification::centredLeft, text, true);
    spaced (g, juce::String (juce::roundToInt (slider.getValue() * 100.0)) + "%", { 0.0f, 0.0f, (float) getWidth(), 18.0f }, 12.5f, 0.05f,
            juce::Justification::centredRight, text.withAlpha (0.6f));
    auto nm = sampleName (st.genre, lane, st.clapVariant, st.kit[(size_t) lane]);
    if (lane == LPercA) nm << "  +  " << sampleName (st.genre, LPercB, st.clapVariant, st.kit[LPercB]);
    nm << juce::String (juce::CharPointer_UTF8 ("  \xe2\x96\xbe"));
    spaced (g, nm, subArea.toFloat(), 12.5f, 0.05f, juce::Justification::centredLeft, text.withAlpha (0.72f));
}

void DrumRow::mouseUp (const juce::MouseEvent& e)
{
    if (! subArea.contains (e.getPosition())) return;
    const auto st = proc.getState();
    const auto* set = SoundBank::get().drums (drumCategory (st.genre, lane, st.clapVariant));
    if (set == nullptr || set->empty()) return;
    juce::PopupMenu m;
    m.addItem (1, juce::String (juce::CharPointer_UTF8 ("Au hasard")));
    m.addSeparator();
    for (int i = 0; i < (int) set->size(); ++i)
        m.addItem (i + 2, (*set)[(size_t) i].name, true, st.kit[(size_t) lane] == i);
    const int n = (int) set->size();
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, n] (int r)
    {
        if (r <= 0) return;
        const int idx = r == 1 ? juce::Random::getSystemRandom().nextInt (n) : r - 2;
        proc.setSample (lane, idx);
        if (auto* p = getParentComponent()) p->repaint();
    });
}

// =============================================================================
RantiDrumsEditor::RantiDrumsEditor (RantiDrumsProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    juce::LookAndFeel::setDefaultLookAndFeel (&look);
    int sz = 0;
    if (auto* d = RantiSamples::getNamedResource ("logo_jpg", sz))
        logo = juce::ImageFileFormat::loadFrom (d, (size_t) sz);

    // ciel : espace profond (nebuleuses, poussiere, etoiles scintillantes), dessine une seule fois
    stars = ranti::makeSpace (W, H, 11, false);
    // planete animee sur son propre calque
    planetLayer.drawDynamic = [this] (juce::Graphics& pg) { planetDynamic = true; drawPlanet (pg, { 590.0f, 285.0f }, 135.0f); planetDynamic = false; };
    planetLayer.needBase = [this] { refreshPlanetBase(); };
    planetLayer.setBounds (420, 128, 340, 284);
    addAndMakeVisible (planetLayer);

    struct R { int lane; const char* title; const char* id; };
    const R list[8] = { { LKick, "KICK", "volKick" }, { LSnare, "SNARE", "volSnare" }, { LClap, "CLAP", "volClap" }, { LRim, "RIM", "volRim" },
                        { LHat, "HI-HAT", "volHat" }, { LOpen, "OPEN HAT", "volOpen" }, { LPercA, "PERCUS", "volPerc" }, { LHat2, "SHAKER", "volShaker" } };
    for (auto& r : list)
    {
        rows.push_back (std::make_unique<DrumRow> (proc, r.lane, r.title, r.id));
        addAndMakeVisible (*rows.back());
    }
    master.setSliderStyle (juce::Slider::LinearHorizontal);
    master.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (master);
    if (auto* prm = proc.apvts.getParameter ("master"))
        masterAtt = std::make_unique<juce::SliderParameterAttachment> (*prm, master);

    setSize (W, H);
    startTimerHz (30);
}

RantiDrumsEditor::~RantiDrumsEditor()
{
    stopTimer();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void RantiDrumsEditor::resized()
{
    for (int i = 0; i < 8; ++i)
        rows[(size_t) i]->setBounds (i < 4 ? 40 : 760, 108 + (i % 4) * 82, 270, 76);
    master.setBounds (W - 24 - 96, 146, 86, 34);
    genBtn = { 590.0f - 140.0f, 452.0f, 280.0f, 46.0f };
    prevBtn = { genBtn.getX() - 58.0f, 452.0f, 46.0f, 46.0f };
    nextBtn = { genBtn.getRight() + 12.0f, 452.0f, 46.0f, 46.0f };
    previewBtn = { 590.0f - 100.0f, 414.0f, 200.0f, 30.0f };
    grooveBox = { (float) W - 24 - 330, 26.0f, 330.0f, 44.0f };
    dragBox = { 40.0f, 500.0f, 346.0f, 38.0f };
    infoBox = { 806.0f, 500.0f, 224.0f, 38.0f };
    const float pw = ((float) W - 48.0f - 24.0f - 9 * 12.0f) / 10.0f;
    for (int i = 0; i < 10; ++i) pads[i] = { 36.0f + (float) i * (pw + 12.0f), 560.0f, pw, 146.0f };
}

void RantiDrumsEditor::drawPlanet (juce::Graphics& g, juce::Point<float> c, float r)
{
    const float k = r / 135.0f;
    const bool playing = proc.previewPlaying.load() || proc.getPlayhead() >= 0.0;
    juce::ignoreUnused (playing);
    if (! planetDynamic)
    {
    {
        juce::ColourGradient n1 (juce::Colour (0x473a6dff), c.x - 60 * k, c.y - 40 * k, juce::Colours::transparentBlack, c.x + 240 * k, c.y - 40 * k, true);
        g.setGradientFill (n1); g.fillEllipse (c.x - 360 * k, c.y - 240 * k, 600 * k, 400 * k);
        juce::ColourGradient n2 (juce::Colour (0x38a066ff), c.x + 120 * k, c.y + 80 * k, juce::Colours::transparentBlack, c.x + 340 * k, c.y + 80 * k, true);
        g.setGradientFill (n2); g.fillEllipse (c.x - 100 * k, c.y - 80 * k, 440 * k, 320 * k);
        juce::ColourGradient halo (cyan.withAlpha (playing ? 0.5f : 0.35f), c.x, c.y, juce::Colours::transparentBlack, c.x + r * 1.45f, c.y, true);
        halo.addColour (0.62, cyan.withAlpha (playing ? 0.38f : 0.25f));
        g.setGradientFill (halo); g.fillEllipse (c.x - r * 1.45f, c.y - r * 1.45f, r * 2.9f, r * 2.9f);
    }
    }
    juce::Path orbit;
    orbit.addEllipse (-162 * k, -42 * k, 324 * k, 84 * k);
    orbit.applyTransform (juce::AffineTransform::rotation (juce::degreesToRadians (-7.0f)).translated (c.x, c.y + 50 * k));
    if (! planetDynamic)
    {
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (juce::Rectangle<float> (c.x - 400, c.y - 400, 800, 400 + 50 * k).toNearestInt());
        g.setColour (juce::Colour (0xffc8b4ff).withAlpha (0.4f));
        g.strokePath (orbit, juce::PathStrokeType (2.0f));
    }
    {
        juce::ColourGradient body (juce::Colour (0xff030306), c.x, c.y, juce::Colour (0xfff2fbff), c.x + r, c.y, true);
        body.addColour (0.62, juce::Colour (0xff05070f));
        body.addColour (0.82, juce::Colour (0xff0d1a3a));
        body.addColour (0.93, juce::Colour (0xff2c5fb8));
        body.addColour (0.985, juce::Colour (0xff9fdcff));
        g.setGradientFill (body); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        juce::ColourGradient refl (juce::Colour (0x8c1a2a55), c.x - 40 * k, c.y - 30 * k, juce::Colours::transparentBlack, c.x + 30 * k, c.y - 30 * k, true);
        g.setGradientFill (refl); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        for (auto [w, a] : { std::pair<float, float> { 12.0f, 0.12f }, { 6.0f, 0.3f }, { 2.6f, 0.95f } })
        {
            g.setColour ((w > 5 ? violet : hi).withAlpha (a));
            g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, w);
        }
        juce::Path arc;
        arc.startNewSubPath (c.x - 128 * k, c.y - 30 * k);
        arc.cubicTo (c.x - 100 * k, c.y - 95 * k, c.x - 30 * k, c.y - 128 * k, c.x + 40 * k, c.y - 122 * k);
        arc.quadraticTo (c.x + 115 * k, c.y - 80 * k, c.x + 128 * k, c.y - 40 * k);
        g.setColour (violet.withAlpha (0.45f));
        g.strokePath (arc, juce::PathStrokeType (1.8f));
    }
    }
    if (planetDynamic)
    {
    if (! faceOnly) ranti::drawSpin (g, c, r, spin, juce::Colour (0xff9fd0ff));
    if (faceOnly)
    {
        auto P = [&] (float x, float y) { return juce::Point<float> (c.x + (x - 125.0f) * k, c.y + (y - 125.0f) * k); };
        juce::Path face, thin;
        face.startNewSubPath (P (100, 42)); face.lineTo (P (100, 112));
        face.startNewSubPath (P (100, 44)); face.lineTo (P (124, 44));
        face.quadraticTo (P (146, 44), P (146, 64)); face.quadraticTo (P (146, 82), P (124, 82)); face.lineTo (P (102, 82));
        face.startNewSubPath (P (120, 82)); face.lineTo (P (148, 114));
        face.startNewSubPath (P (96, 128)); face.lineTo (P (112, 144));
        face.startNewSubPath (P (112, 128)); face.lineTo (P (96, 144));
        face.startNewSubPath (P (138, 128)); face.lineTo (P (154, 144));
        face.startNewSubPath (P (154, 128)); face.lineTo (P (138, 144));
        face.startNewSubPath (P (74, 152)); face.quadraticTo (P (125, 222), P (176, 152));
        thin.startNewSubPath (P (66, 162)); thin.lineTo (P (74, 150)); thin.lineTo (P (84, 158));
        thin.startNewSubPath (P (184, 158)); thin.lineTo (P (176, 150)); thin.lineTo (P (168, 160));
        thin.startNewSubPath (P (146, 178)); thin.quadraticTo (P (150, 202), P (162, 196)); thin.quadraticTo (P (172, 190), P (160, 170));
        for (auto [w, a] : { std::pair<float, float> { 16.0f, 0.08f }, { 10.0f, 0.18f }, { 6.0f, 0.4f } })
        {
            g.setColour (cyan.withAlpha (a));
            g.strokePath (face, juce::PathStrokeType (w * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.strokePath (thin, juce::PathStrokeType (w * k * 0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (juce::Colour (0xffd6f1ff));
        g.strokePath (face, juce::PathStrokeType (4.0f * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath (thin, juce::PathStrokeType (3.2f * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    if (faceOnly) return;
    // le visage (dessine une fois en image) tourne avec la planete : il glisse, se resserre et passe derriere
    if (faceImg.isValid())
    {
        const float th = spin * juce::MathConstants<float>::twoPi, cs = std::cos (th);
        if (cs > 0.02f)
        {
            juce::Graphics::ScopedSaveState s (g);
            g.setOpacity (juce::jmin (1.0f, cs * 1.4f));
            const auto tr = juce::AffineTransform::scale (1.0f / faceScale)
                              .translated (c.x - r, c.y - r)
                              .scaled (cs, 1.0f, c.x, c.y)
                              .translated (r * std::sin (th), 0.0f);
            g.drawImageTransformed (faceImg, tr);
        }
    }
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (juce::Rectangle<float> (c.x - 400, c.y + 50 * k, 800, 400).toNearestInt());
        g.setColour (cyan.withAlpha (0.3f));
        g.strokePath (orbit, juce::PathStrokeType (7.0f));
        g.setColour (juce::Colour (0xffe8f4ff));
        g.strokePath (orbit, juce::PathStrokeType (2.6f));
    }
    ranti::drawOrbitSpark (g, c, k, spin, juce::Colour (0xffe6f4ff));
    }
}

void RantiDrumsEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff03050f));
    const auto frame = juce::Rectangle<float> (2.0f, 2.0f, W - 4.0f, H - 4.0f);
    {
        juce::ColourGradient bg (juce::Colour (0xff13204a), 590.0f, 310.0f, juce::Colour (0xff03050f), 590.0f + 760.0f, 310.0f, true);
        bg.addColour (0.5, juce::Colour (0xff070b22));
        g.setGradientFill (bg);
        g.fillRoundedRectangle (frame, 22.0f);
    }
    g.drawImageAt (stars, 0, 0);
    g.setColour (cyan.withAlpha (0.15f));
    g.drawRoundedRectangle (frame.reduced (1.0f), 22.0f, 6.0f);
    g.setColour (cyan.withAlpha (0.55f));
    g.drawRoundedRectangle (frame.reduced (1.0f), 22.0f, 2.0f);

    const auto st = proc.getState();
    if (logo.isValid())
    {
        juce::Graphics::ScopedSaveState s (g);
        juce::Path circ; circ.addEllipse (24.0f, 18.0f, 74.0f, 74.0f);
        g.reduceClipRegion (circ);
        g.drawImage (logo, juce::Rectangle<float> (24.0f, 18.0f, 74.0f, 74.0f), juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    }
    spaced (g, "DRUMS", { 114.0f, 26.0f, 220.0f, 36.0f }, 30.0f, 0.38f, juce::Justification::centredLeft, text);
    spaced (g, "RANTIPOP", { 116.0f, 62.0f, 200.0f, 16.0f }, 10.5f, 0.55f, juce::Justification::centredLeft, text.withAlpha (0.7f));

    {
        const auto box = juce::Rectangle<float> (300.0f, 26.0f, 400.0f, 44.0f);
        glass (g, box);
        const float tw[4] = { 72.0f, 86.0f, 72.0f, 150.0f };
        float x = box.getX() + 5.0f;
        for (int i = 0; i < numGenres; ++i)
        {
            genreTabs[i] = { x, box.getY() + 5.0f, tw[i], box.getHeight() - 10.0f };
            bool avail = i == 0;
            for (int j = 0; j < numGrooves; ++j) if (grooves[j].genre == i) avail = true;
            const bool on = st.genre == i;
            if (on) { g.setGradientFill (juce::ColourGradient (cyan, genreTabs[i].getX(), 0, violet, genreTabs[i].getRight(), 0, false)); g.fillRoundedRectangle (genreTabs[i], 8.0f); }
            spaced (g, genreNames[i], genreTabs[i], 13.0f, 0.2f, juce::Justification::centred, on ? juce::Colour (0xff060a1c) : text.withAlpha (avail ? 0.65f : 0.25f), on);
            x += tw[i] + 2.0f;
        }
    }
    glass (g, grooveBox);
    spaced (g, "<", grooveBox.withWidth (34.0f), 15.0f, 0.0f, juce::Justification::centred, text);
    spaced (g, ">", grooveBox.withTrimmedLeft (grooveBox.getWidth() - 34.0f), 15.0f, 0.0f, juce::Justification::centred, text);
    juce::String gname = "-";
    if (st.genre == 0) { double L = 16.0; buildRnbDrums (st, L, &gname); }
    else if (st.groove >= 0) gname = juce::String (grooves[st.groove].name).toUpperCase();
    spaced (g, gname, grooveBox.reduced (36.0f, 0.0f), st.genre == 0 ? 12.0f : 14.0f, 0.12f,
            juce::Justification::centred, text, true);

    drawPlanet (g, { 590.0f, 285.0f }, 135.0f);
    const bool playing = proc.previewPlaying.load();
    glass (g, previewBtn, 10.0f);
    if (playing) { g.setColour (cyan.withAlpha (0.22f)); g.fillRoundedRectangle (previewBtn, 10.0f); }
    spaced (g, playing ? juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xa0  STOP")) : juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6  PR\xc3\x89VISUALISER")),
            previewBtn, 11.0f, 0.25f, juce::Justification::centred, text, true);

    glass (g, prevBtn); glass (g, nextBtn);
    spaced (g, "<", prevBtn, 18.0f, 0.0f, juce::Justification::centred, text.withAlpha (proc.canGoBack() ? 1.0f : 0.3f));
    spaced (g, ">", nextBtn, 18.0f, 0.0f, juce::Justification::centred, text.withAlpha (proc.canGoForward() ? 1.0f : 0.3f));
    g.setColour (cyan.withAlpha (0.35f));
    g.fillRoundedRectangle (genBtn.expanded (6.0f), 14.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (genDown ? 0xff7fc0ef : 0xff9fe0ff), genBtn.getX(), genBtn.getY(),
                                             juce::Colour (genDown ? 0xff5470d8 : 0xff6d8dff), genBtn.getX(), genBtn.getBottom(), false));
    g.fillRoundedRectangle (genBtn, 10.0f);
    spaced (g, juce::String (juce::CharPointer_UTF8 ("G\xc3\x89N\xc3\x89RER")), genBtn, 16.0f, 0.35f, juce::Justification::centred, juce::Colour (0xff050a1e), true);

    glass (g, dragBox, 10.0f);
    spaced (g, juce::String (juce::CharPointer_UTF8 ("\xe2\x87\xa3  GLISSER LE MIDI DANS FL")), dragBox, 11.0f, 0.2f, juce::Justification::centred, text, true);

    glass (g, infoBox, 10.0f);
    {
        const double L = proc.getLength();
        juce::String bpmTxt = st.groove >= 0 ? juce::String (grooves[st.groove].bpm) : juce::String ("-");
        if (st.genre == 0 && st.rnb[0] >= 0) bpmTxt = juce::String (parts[st.rnb[0]].lo) + "-" + juce::String (parts[st.rnb[0]].hi);
        spaced (g, juce::String (juce::CharPointer_UTF8 ("KIT \xe2\x96\xbe")), infoBox.withWidth (70.0f).withTrimmedLeft (14.0f), 11.5f, 0.2f, juce::Justification::centredLeft, text, true);
        spaced (g, "BPM " + bpmTxt + "    " + juce::String (juce::roundToInt (L / 4.0)) + " MES", infoBox.withTrimmedLeft (70.0f).withTrimmedRight (12.0f),
                11.0f, 0.15f, juce::Justification::centredRight, text);
    }

    {
        const auto box = juce::Rectangle<float> ((float) W - 24 - 106, 110.0f, 106.0f, 424.0f);
        glass (g, box);
        spaced (g, "MASTER", box.withHeight (36.0f).withY (box.getY() + 8.0f), 11.0f, 0.25f, juce::Justification::centred, text);
        spaced (g, juce::String (juce::roundToInt (master.getValue() * 100.0)) + "%", { box.getX(), 184.0f, box.getWidth(), 16.0f }, 11.0f, 0.05f,
                juce::Justification::centred, text.withAlpha (0.6f));
    }
    glass (g, { 24.0f, 548.0f, (float) W - 48.0f, 170.0f }, 14.0f);
}

void RantiDrumsEditor::paintOverChildren (juce::Graphics& g)
{
    const auto box = juce::Rectangle<float> ((float) W - 24 - 106, 110.0f, 106.0f, 424.0f);
    const float m = juce::jlimit (0.0f, 1.0f, proc.getMeter());
    const float db = juce::Decibels::gainToDecibels (m, -60.0f);
    const float lvl = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
    for (int ch = 0; ch < 2; ++ch)
    {
        const float x = box.getCentreX() - 14.0f + ch * 18.0f;
        const float lv = lvl * (ch == 0 ? 1.0f : 0.94f);
        for (int s = 0; s < 52; ++s)
        {
            g.setColour ((float) s / 52.0f < lv ? cyan : cyan.withAlpha (0.1f));
            g.fillRect (x, 480.0f - (float) s * 5.0f - 3.0f, 10.0f, 3.0f);
        }
    }
    spaced (g, (m > 0.0001f ? juce::String (db, 1) : juce::String ("-inf")) + " dB", { box.getX(), 496.0f, box.getWidth(), 18.0f }, 11.0f, 0.05f,
            juce::Justification::centred, text.withAlpha (0.75f));

    static const char* names[10] = { "KICK", "SNARE", "CLAP", "HI-HAT", "OPEN", "RIM", "PERC 1", "PERC 2", "SHAKER", "CRASH" };
    const auto st = proc.getState();
    for (int i = 0; i < 10; ++i)
    {
        const auto r = pads[i];
        const float a = padGlow[(size_t) i];
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141d3e), r.getX(), r.getY(), juce::Colour (0xff0a1028), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 12.0f);
        if (a > 0.01f)
        {
            g.setColour (cyan.withAlpha (0.45f * a));
            g.fillRoundedRectangle (r.expanded (4.0f), 14.0f);
            juce::ColourGradient lit (juce::Colour (0xff9fe0ff).withAlpha (a), r.getCentreX(), r.getY() + r.getHeight() * 0.4f,
                                      juce::Colour (0xff2a2f8a).withAlpha (a), r.getCentreX(), r.getBottom() + 20.0f, true);
            lit.addColour (0.55, juce::Colour (0xff5b7dff).withAlpha (a));
            g.setGradientFill (lit);
            g.fillRoundedRectangle (r, 12.0f);
        }
        g.setColour (juce::Colour (0x40a0beff));
        g.drawRoundedRectangle (r, 12.0f, 1.0f);
        juce::String nm = names[i];
        if (i == 8 && st.genre == 3) nm = "HAT 2";
        spaced (g, nm, r.withTrimmedBottom (14.0f).withTrimmedTop (r.getHeight() - 40.0f), 11.0f, 0.2f, juce::Justification::centred,
                a > 0.5f ? juce::Colour (0xff050a1e) : text, true);
    }
}

void RantiDrumsEditor::refreshPlanetBase()
{
    const auto b = planetLayer.getBounds();
    const float sc = 2.0f;
    juce::Image img (juce::Image::ARGB, juce::roundToInt (b.getWidth() * sc), juce::roundToInt (b.getHeight() * sc), true, juce::SoftwareImageType());
    {
        juce::Graphics ig (img);
        ig.addTransform (juce::AffineTransform::translation ((float) -b.getX(), (float) -b.getY()).scaled (sc));
        planetDynamic = false;
        paint (ig);
    }
    planetLayer.base = img;
    {
        const juce::Point<float> c (590.0f, 285.0f);
        const float r = 135.0f;
        juce::Image fi (juce::Image::ARGB, juce::roundToInt (2.0f * r * faceScale), juce::roundToInt (2.0f * r * faceScale), true, juce::SoftwareImageType());
        juce::Graphics fg (fi);
        fg.addTransform (juce::AffineTransform::translation (-(c.x - r), -(c.y - r)).scaled (faceScale));
        planetDynamic = true; faceOnly = true;
        drawPlanet (fg, c, r);
        planetDynamic = false; faceOnly = false;
        faceImg = fi;
    }
}

void RantiDrumsEditor::timerCallback()
{
    ranti::useSoftwareRenderer (*this);
    const bool animFrame = (++animTick & 1) == 0;
    if (animFrame) spin = std::fmod (spin + 1.0f / 300.0f, 1.0f);
    {
        const bool pl = proc.previewPlaying.load() || proc.getPlayhead() >= 0.0;
        const int v = proc.getVersion();
        if (! planetLayer.base.isValid() || v != planetVersion || pl != planetPlaying) { planetVersion = v; planetPlaying = pl; refreshPlanetBase(); }
        if (animFrame) planetLayer.repaint();
    }
    const int v = proc.getVersion();
    if (v != lastVersion) { lastVersion = v; repaint(); for (auto& r : rows) r->repaint(); }
    for (int i = 0; i < 10; ++i)
    {
        bool fired = false;
        for (int l : { padLane[i], i == 7 ? (int) LPercC : -1 })
        {
            if (l < 0) continue;
            const auto h = proc.getLaneHit (l);
            if (h != seenHit[(size_t) l]) { seenHit[(size_t) l] = h; fired = true; }
        }
        padGlow[(size_t) i] = fired ? 1.0f : padGlow[(size_t) i] * 0.78f;
    }
    repaint (juce::Rectangle<int> (W - 24 - 106, 210, 106, 310));
    repaint (juce::Rectangle<int> (24, 548, W - 48, 170));
}

void RantiDrumsEditor::mouseDown (const juce::MouseEvent& e)
{
    if (genBtn.contains (e.position)) { genDown = true; repaint (genBtn.expanded (8.0f).toNearestInt()); }
    for (int i = 0; i < 10; ++i)
        if (pads[i].contains (e.position)) proc.audition (padLane[i]);
}

void RantiDrumsEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || ! dragBox.contains (e.mouseDownPosition)) return;
    auto f = proc.writeTempMidi();
    if (f.existsAsFile())
    {
        dragging = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
    }
}

void RantiDrumsEditor::mouseUp (const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (genDown) { genDown = false; repaint(); }
    if (e.mouseWasDraggedSinceMouseDown()) return;
    if (genBtn.contains (p)) { proc.generate(); return; }
    if (prevBtn.contains (p)) { proc.goBack(); return; }
    if (nextBtn.contains (p)) { proc.goForward(); return; }
    for (int i = 0; i < numGenres; ++i)
        if (genreTabs[i].contains (p))
        {
            bool avail = i == 0;
            for (int j = 0; j < numGrooves; ++j) if (grooves[j].genre == i) avail = true;
            if (avail) proc.setGenre (i);
            return;
        }
    if (grooveBox.contains (p))
    {
        if (p.x < grooveBox.getX() + 40.0f) proc.stepGroove (-1);
        else if (p.x > grooveBox.getRight() - 40.0f) proc.stepGroove (1);
        else if (proc.getState().genre == 0) proc.stepGroove (1);
        else
        {
            const auto st = proc.getState();
            juce::PopupMenu m;
            for (int i = 0; i < numGrooves; ++i)
                if (grooves[i].genre == st.genre)
                    m.addItem (i + 1, juce::String (grooves[i].name) + "  (" + juce::String (grooves[i].bpm) + " bpm)", true, st.groove == i);
            m.showMenuAsync (juce::PopupMenu::Options().withParentComponent (this), [this] (int r) { if (r > 0) proc.setGroove (r - 1); });
        }
        return;
    }
    if (infoBox.contains (p) && p.x < infoBox.getX() + 80.0f) { showKitMenu(); return; }
    if (previewBtn.contains (p) || p.getDistanceFrom ({ 590.0f, 285.0f }) < 140.0f)
    {
        proc.togglePreview();
        repaint();
    }
}

void RantiDrumsEditor::showKitMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Nouveau kit au hasard");
    m.showMenuAsync (juce::PopupMenu::Options().withParentComponent (this), [this] (int r) { if (r == 1) proc.newKit(); });
}
