#include "RantiSpace.h"
#include "RantiEditor.h"
#include "RantiSamples.h"

using namespace ranti;
using namespace rtheme;

namespace
{
constexpr int W = 1180, H = 740;
const int numChips = 7;
const int chipPart[numChips] = { PMel, PChords, PExtra, PArp, PPad, PBass, RantiProcessor::allParts }; // MELO ACC CONTRE ARP PAD 808 TOUT

juce::Font font (float size, float kerning = 0.0f, bool bold = false)
{
    juce::Font f (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    return f.withExtraKerningFactor (kerning);
}

// case des MIDI a glisser (sous les pistes, a cote des boutons)
juce::Rectangle<float> dragBox() { return { 24.0f, 502.0f, 404.0f, 38.0f }; }

juce::Path curvePath (juce::Rectangle<float> r)
{
    juce::Path p;
    p.startNewSubPath (r.getX(), r.getBottom() - 2.0f);
    p.cubicTo (r.getX() + r.getWidth() * 0.3f, r.getBottom() - 2.0f,
               r.getX() + r.getWidth() * 0.5f, r.getY() + 3.0f,
               r.getRight(), r.getY() + 2.0f);
    return p;
}
}

// =============================================================================
//  LookAndFeel
// =============================================================================
void RantiLook::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                  juce::Slider::SliderStyle, juce::Slider&)
{
    const auto r = juce::Rectangle<float> ((float) x, (float) y + h * 0.15f, (float) w, h * 0.7f);
    const auto path = curvePath (r);
    g.setColour (goldDim);
    g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // partie active : du debut jusqu'au curseur
    const float t = juce::jlimit (0.0f, 1.0f, (pos - (float) x) / (float) juce::jmax (1, w));
    const float len = path.getLength();
    juce::Path active;
    const int steps = 48;
    for (int i = 0; i <= steps; ++i)
    {
        const auto pt = path.getPointAlongPath (len * t * (float) i / steps);
        if (i == 0) active.startNewSubPath (pt); else active.lineTo (pt);
    }
    g.setColour (gold.withAlpha (0.25f));
    g.strokePath (active, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (gold);
    g.strokePath (active, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto th = path.getPointAlongPath (len * t);
    g.setColour (gold.withAlpha (0.35f));
    g.fillEllipse (th.x - 9.0f, th.y - 9.0f, 18.0f, 18.0f);
    g.setColour (goldHi);
    g.fillEllipse (th.x - 5.5f, th.y - 5.5f, 11.0f, 11.0f);
}

void RantiLook::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (juce::Colour (0xf20d0b08));
    g.setColour (goldDim);
    g.drawRect (0, 0, w, h, 1);
}

void RantiLook::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                   bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String&,
                                   const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour (goldLow);
        g.fillRect (area.reduced (8, area.getHeight() / 2).withHeight (1));
        return;
    }
    if (isHighlighted && isActive)
    {
        g.setColour (gold.withAlpha (0.18f));
        g.fillRect (area);
    }
    g.setColour (isTicked ? goldHi : gold.withAlpha (isActive ? 0.9f : 0.35f));
    g.setFont (font (14.0f, 0.04f, isTicked));
    g.drawText ((isTicked ? juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa2 ")) : "  ") + text,
                area.reduced (12, 0), juce::Justification::centredLeft);
    if (hasSubMenu)
        g.drawText (">", area.reduced (10, 0), juce::Justification::centredRight);
}

// =============================================================================
//  Ligne de controle
// =============================================================================
ControlRow::ControlRow (RantiProcessor& p, const juce::String& t, const juce::String& volId) : proc (p), title (t)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    if (auto* prm = proc.apvts.getParameter (volId))
        att = std::make_unique<juce::SliderParameterAttachment> (*prm, slider);
}

void ControlRow::resized()
{
    slider.setBounds (0, 40, getWidth(), 34);
    subArea = { 0, 20, getWidth() - 60, 18 };
}

void ControlRow::paint (juce::Graphics& g)
{
    // piste vide dans ce morceau : tout en plus discret
    if (isEmpty && isEmpty()) g.beginTransparencyLayer (0.4f);
    g.setColour (gold);
    g.setFont (font (13.0f, 0.22f, true));
    g.drawText (title, 0, 0, getWidth(), 18, juce::Justification::centredLeft);
    g.setColour (gold.withAlpha (0.6f));
    g.setFont (font (12.5f, 0.05f));
    g.drawText (juce::String (juce::roundToInt (slider.getValue() * 100.0)) + "%", 0, 0, getWidth(), 18, juce::Justification::centredRight);
    if (subText)
    {
        g.setColour (gold.withAlpha (onSubClick ? 0.75f : 0.6f));
        g.setFont (font (12.5f, 0.06f));
        auto txt = subText();
        if (onSubClick) txt << juce::String (juce::CharPointer_UTF8 ("  \xe2\x96\xbe"));
        g.drawText (txt, subArea, juce::Justification::centredLeft);
    }
    if (isEmpty && isEmpty()) g.endTransparencyLayer();
}

void ControlRow::mouseUp (const juce::MouseEvent& e)
{
    if (onSubClick && subArea.contains (e.getPosition())) onSubClick();
}

// =============================================================================
//  Editeur
// =============================================================================
RantiEditor::RantiEditor (RantiProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      rowMel (p, juce::String (juce::CharPointer_UTF8 ("M\xc3\x89LODIE")), "volMel"),
      rowChords (p, "ACCORDS", "volChords"),
      rowExtra (p, juce::String (juce::CharPointer_UTF8 ("CONTRE-M\xc3\x89LO")), "volExtra"),
      rowArp (p, "ARP", "volArp"),
      rowPad (p, "PAD", "volPad"),
      rowBass (p, "808 / BASSE", "volBass"),
      rowVerb (p, "REVERB", "reverb")
{
    setLookAndFeel (&look);
    juce::LookAndFeel::setDefaultLookAndFeel (&look);
    int sz = 0;
    if (auto* d = RantiSamples::getNamedResource ("logo_jpg", sz))
        logo = juce::ImageFileFormat::loadFrom (d, (size_t) sz);

    // ciel etoile (fixe)
    // ciel : espace profond (nebuleuses, poussiere, etoiles scintillantes), dessine une seule fois
    stars = ranti::makeSpace (W, H, 7, true);
    // planete animee sur son propre calque
    planetLayer.drawDynamic = [this] (juce::Graphics& pg) { planetDynamic = true; drawPlanet (pg, { 590.0f, 285.0f }, 135.0f); planetDynamic = false; };
    planetLayer.needBase = [this] { refreshPlanetBase(); };
    planetLayer.setBounds (420, 128, 340, 284);
    addAndMakeVisible (planetLayer);

    auto& bank = SoundBank::get();
    auto instText = [this] (const char* id, bool bass)
    {
        return [this, id, bass]
        {
            const int i = (int) proc.apvts.getRawParameterValue (id)->load();
            return bass ? SoundBank::get().bassMenu()[i] : SoundBank::get().melodicMenu()[i];
        };
    };
    juce::ignoreUnused (bank);
    rowMel.subText = instText ("instMel", false);       rowMel.onSubClick = [this] { showInstMenu ("instMel", false); };
    rowChords.subText = instText ("instChords", false); rowChords.onSubClick = [this] { showInstMenu ("instChords", false); };
    rowExtra.subText = instText ("instExtra", false);   rowExtra.onSubClick = [this] { showInstMenu ("instExtra", false); };
    rowBass.subText = instText ("instBass", true);      rowBass.onSubClick = [this] { showInstMenu ("instBass", true); };
    rowArp.subText = instText ("instArp", false);       rowArp.onSubClick = [this] { showInstMenu ("instArp", false); };
    rowPad.subText = instText ("instPad", false);       rowPad.onSubClick = [this] { showInstMenu ("instPad", false); };
    {
        const std::pair<ControlRow*, int> rp[] = { { &rowMel, PMel }, { &rowChords, PChords }, { &rowExtra, PExtra }, { &rowArp, PArp }, { &rowPad, PPad } };
        for (auto& [row, part] : rp)
        {
            const int pt = part;
            row->isEmpty = [this, pt] { return proc.getPattern().parts[(size_t) pt].empty(); };
        }
    }
    rowVerb.subText = [] { return juce::String ("Room"); };

    for (auto* r : { &rowMel, &rowChords, &rowExtra, &rowArp, &rowPad, &rowBass, &rowVerb })
        addAndMakeVisible (r);

    master.setSliderStyle (juce::Slider::LinearHorizontal);
    master.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (master);
    if (auto* prm = proc.apvts.getParameter ("master"))
        masterAtt = std::make_unique<juce::SliderParameterAttachment> (*prm, master);

    setSize (W, H);
    startTimerHz (30);
}

RantiEditor::~RantiEditor()
{
    stopTimer();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void RantiEditor::resized()
{
    // colonne gauche : 5 pistes
    rowMel.setBounds (40, 116, 270, 76);
    rowChords.setBounds (40, 192, 270, 76);
    rowExtra.setBounds (40, 268, 270, 76);
    rowArp.setBounds (40, 344, 270, 76);
    rowPad.setBounds (40, 420, 270, 76);
    rowBass.setBounds (760, 120, 270, 76);
    rowVerb.setBounds (760, 316, 270, 76);
    chordBox = { 760.0f, 204.0f, 270.0f, 86.0f };
    previewBtn = { 590.0f - 100.0f, 414.0f, 200.0f, 30.0f };
    master.setBounds (W - 24 - 96, 146, 86, 34);

    planetArea = { 590.0f - 150.0f, 285.0f - 150.0f, 300.0f, 300.0f };
    genBtn = { 590.0f - 140.0f, 452.0f, 280.0f, 46.0f };
    prevBtn = { genBtn.getX() - 58.0f, 452.0f, 46.0f, 46.0f };
    nextBtn = { genBtn.getRight() + 12.0f, 452.0f, 46.0f, 46.0f };
    songBox = { (float) W - 24 - 330, 26.0f, 330.0f, 44.0f };
    keyBox = { 806.0f, 500.0f, 224.0f, 38.0f };
}

void RantiEditor::glass (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    g.setColour (gold.withAlpha (0.045f));
    g.fillRoundedRectangle (r, radius);
    g.setColour (gold.withAlpha (0.2f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

void RantiEditor::spaced (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float size, float kerning,
                          juce::Justification j, juce::Colour c, bool bold)
{
    g.setColour (c);
    g.setFont (font (size, kerning, bold));
    g.drawText (t, r, j);
}

void RantiEditor::drawPlanet (juce::Graphics& g, juce::Point<float> c, float r)
{
    const float k = r / 135.0f;
    const bool playing = proc.previewPlaying.load() || proc.getPlayhead() >= 0.0;
    juce::ignoreUnused (playing);
    if (! planetDynamic)
    {

    // nebuleuse doree
    {
        juce::ColourGradient n1 (juce::Colour (0x47b8853a), c.x - 60 * k, c.y - 40 * k, juce::Colours::transparentBlack, c.x - 60 * k + 300 * k, c.y - 40 * k, true);
        g.setGradientFill (n1);
        g.fillEllipse (c.x - 360 * k, c.y - 240 * k, 600 * k, 400 * k);
        juce::ColourGradient n2 (juce::Colour (0x2ee2b56a), c.x + 120 * k, c.y + 80 * k, juce::Colours::transparentBlack, c.x + 340 * k, c.y + 80 * k, true);
        g.setGradientFill (n2);
        g.fillEllipse (c.x - 100 * k, c.y - 80 * k, 440 * k, 320 * k);
    }
    // halo
    {
        juce::ColourGradient halo (gold.withAlpha (playing ? 0.5f : 0.35f), c.x, c.y, juce::Colours::transparentBlack, c.x + r * 1.45f, c.y, true);
        halo.addColour (0.62, gold.withAlpha (playing ? 0.38f : 0.25f));
        g.setGradientFill (halo);
        g.fillEllipse (c.x - r * 1.45f, c.y - r * 1.45f, r * 2.9f, r * 2.9f);
    }
    // orbite (moitie arriere)
    }
    juce::Path orbit;
    orbit.addEllipse (-162 * k, -42 * k, 324 * k, 84 * k);
    orbit.applyTransform (juce::AffineTransform::rotation (juce::degreesToRadians (-7.0f)).translated (c.x, c.y + 50 * k));
    if (! planetDynamic)
    {
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (juce::Rectangle<float> (c.x - 400, c.y - 400, 800, 400 + 50 * k).toNearestInt());
        g.setColour (juce::Colour (0xfff6dca0).withAlpha (0.4f));
        g.strokePath (orbit, juce::PathStrokeType (2.0f));
    }
    // sphere de verre noire, bord lumineux
    {
        juce::ColourGradient body (juce::Colour (0xff030303), c.x, c.y, juce::Colour (0xfffff6dc), c.x + r, c.y, true);
        body.addColour (0.62, juce::Colour (0xff070605));
        body.addColour (0.82, juce::Colour (0xff1c1509));
        body.addColour (0.93, juce::Colour (0xff7a5a24));
        body.addColour (0.985, juce::Colour (0xfff3d28c));
        g.setGradientFill (body);
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        juce::ColourGradient refl (juce::Colour (0x8c3a3020), c.x - 40 * k, c.y - 30 * k, juce::Colours::transparentBlack, c.x + 30 * k, c.y - 30 * k, true);
        g.setGradientFill (refl);
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        for (auto [w, a] : { std::pair<float, float> { 12.0f, 0.12f }, { 6.0f, 0.3f }, { 2.6f, 0.95f } })
        {
            g.setColour (goldHi.withAlpha (a));
            g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, w);
        }
        // fine trajectoire en haut
        juce::Path arc;
        arc.startNewSubPath (c.x - 128 * k, c.y - 30 * k);
        arc.cubicTo (c.x - 100 * k, c.y - 95 * k, c.x - 30 * k, c.y - 128 * k, c.x + 40 * k, c.y - 122 * k);
        arc.quadraticTo (c.x + 115 * k, c.y - 80 * k, c.x + 128 * k, c.y - 40 * k);
        g.setColour (gold.withAlpha (0.4f));
        g.strokePath (arc, juce::PathStrokeType (1.8f));
    }
    // R + perso en neon
    }
    if (planetDynamic)
    {
    if (! faceOnly) ranti::drawSpin (g, c, r, spin, juce::Colour (0xfff3d28c));
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
        const float glow = playing ? 1.0f : 0.8f;
        for (auto [w, a] : { std::pair<float, float> { 16.0f, 0.08f }, { 10.0f, 0.18f }, { 6.0f, 0.4f } })
        {
            g.setColour (gold.withAlpha (a * glow));
            g.strokePath (face, juce::PathStrokeType (w * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.strokePath (thin, juce::PathStrokeType (w * k * 0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (juce::Colour (0xffffe7ae));
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
    // orbite (moitie avant)
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (juce::Rectangle<float> (c.x - 400, c.y + 50 * k, 800, 400).toNearestInt());
        g.setColour (gold.withAlpha (0.3f));
        g.strokePath (orbit, juce::PathStrokeType (7.0f));
        g.setColour (juce::Colour (0xfffff1cc));
        g.strokePath (orbit, juce::PathStrokeType (2.6f));
    }
    ranti::drawOrbitSpark (g, c, k, spin, juce::Colour (0xfffff1cc));
    }
}

void RantiEditor::drawKeyboard (juce::Graphics& g, juce::Rectangle<float> area)
{
    const int lo = 48, hi = 96; // Do 3 -> Do 7
    std::vector<int> whites;
    for (int n = lo; n <= hi; ++n) { const int pc = n % 12; if (pc != 1 && pc != 3 && pc != 6 && pc != 8 && pc != 10) whites.push_back (n); }
    const float gap = 3.0f;
    const float ww = (area.getWidth() - gap * (float) (whites.size() - 1)) / (float) whites.size();
    for (size_t i = 0; i < whites.size(); ++i)
    {
        auto r = juce::Rectangle<float> (area.getX() + (float) i * (ww + gap), area.getY(), ww, area.getHeight());
        const bool on = keys[(size_t) whites[i]];
        if (on)
        {
            g.setColour (gold.withAlpha (0.45f));
            g.fillRoundedRectangle (r.expanded (4.0f), 6.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff8dd9a), r.getX(), r.getY(), juce::Colour (0xffd9a948), r.getX(), r.getBottom(), false));
        }
        else
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9e2d2), r.getX(), r.getY(), juce::Colour (0xffc9bfa8), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.0f);
    }
    for (size_t i = 0; i + 1 < whites.size(); ++i)
    {
        const int b = whites[i] + 1;
        const int pc = b % 12;
        if (pc != 1 && pc != 3 && pc != 6 && pc != 8 && pc != 10) continue;
        const float x = area.getX() + (float) (i + 1) * (ww + gap) - gap * 0.5f;
        auto r = juce::Rectangle<float> (x - ww * 0.32f, area.getY(), ww * 0.64f, area.getHeight() * 0.6f);
        const bool on = keys[(size_t) b];
        g.setGradientFill (on ? juce::ColourGradient (juce::Colour (0xfff0c86e), r.getX(), r.getY(), juce::Colour (0xff9a7228), r.getX(), r.getBottom(), false)
                              : juce::ColourGradient (juce::Colour (0xff1c1a16), r.getX(), r.getY(), juce::Colour (0xff0a0908), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colour (0xff2a261c));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }
}

void RantiEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff05050a));
    const auto frame = juce::Rectangle<float> (2.0f, 2.0f, W - 4.0f, H - 4.0f);
    {
        juce::ColourGradient bgGrad (juce::Colour (0xff1d1a12), 590.0f, 310.0f, juce::Colour (0xff050508), 590.0f + 760.0f, 310.0f, true);
        bgGrad.addColour (0.45, juce::Colour (0xff0b0a0d));
        g.setGradientFill (bgGrad);
        g.fillRoundedRectangle (frame, 22.0f);
    }
    g.drawImageAt (stars, 0, 0);
    g.setColour (gold.withAlpha (0.12f));
    g.drawRoundedRectangle (frame.reduced (1.0f), 22.0f, 6.0f);
    g.setColour (gold.withAlpha (0.55f));
    g.drawRoundedRectangle (frame.reduced (1.0f), 22.0f, 2.0f);

    const auto st = proc.getState();
    const auto pat = proc.getPattern();

    // --- en-tete
    if (logo.isValid())
    {
        juce::Graphics::ScopedSaveState s (g);
        juce::Path circ; circ.addEllipse (24.0f, 18.0f, 74.0f, 74.0f);
        g.reduceClipRegion (circ);
        g.drawImage (logo, juce::Rectangle<float> (24.0f, 18.0f, 74.0f, 74.0f), juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    }
    spaced (g, "MELODIE", { 114.0f, 26.0f, 300.0f, 36.0f }, 30.0f, 0.38f, juce::Justification::centredLeft, gold);
    spaced (g, "RANTIPOP", { 116.0f, 62.0f, 200.0f, 16.0f }, 10.5f, 0.55f, juce::Justification::centredLeft, gold.withAlpha (0.7f));

    // genres
    {
        // MELODIE : la SYNTHWAVE est regroupee dans la POP WAVE -> onglets R&B, AFRO, POP, POP WAVE
        const auto box = juce::Rectangle<float> (336.0f, 26.0f, 466.0f, 44.0f);
        glass (g, box);
        const float tw[5] = { 96.0f, 104.0f, 96.0f, 0.0f, 150.0f };
        float x = box.getX() + 5.0f;
        for (int i = 0; i < numGenres; ++i)
        {
            if (tw[i] <= 0.0f) { genreTabs[i] = {}; continue; }
            genreTabs[i] = { x, box.getY() + 5.0f, tw[i], box.getHeight() - 10.0f };
            const bool on = st.genre == i, avail = genreAvailable (i);
            if (on) { g.setColour (gold); g.fillRoundedRectangle (genreTabs[i], 8.0f); }
            spaced (g, genreNames[i], genreTabs[i], 13.0f, 0.2f, juce::Justification::centred,
                    on ? juce::Colour (0xff14110a) : gold.withAlpha (avail ? 0.65f : 0.25f), on);
            x += tw[i] + 2.0f;
        }
    }
    // gammes : juste sous les genres, toutes visibles (grisees s'il n'y a pas encore de MIDI dans cette gamme)
    {
        const int mc = proc.getModeChoice();
        if (st.genre != pillGenre)
        {
            pillGenre = st.genre;
            for (auto& c : pillCount) c = 0;
            for (int i = 0; i < numSongs; ++i) if (songs[i].genre == st.genre) ++pillCount[(size_t) songs[i].mode];
        }
        if (pillText.isEmpty())
        {
            auto add = [&] (int row, const char* t, int v) { pillText.add (t); pillVal.push_back (v); pillRow.push_back (row); };
            add (0, "TOUTES", -1); add (0, "MAJEURES", -2); add (0, "MINEURES", -3);
            for (int md : { 0, 3, 4, 9, 12, 14, 15, 18 }) add (0, modeShort[md], md);
            for (int md : { 5, 1, 2, 6, 7, 8, 10, 11, 13, 16, 17 }) add (1, modeShort[md], md);
            for (int row = 0; row < 2; ++row)
            {
                float fs = 10.0f;
                for (int pass = 0; pass < 8; ++pass)
                {
                    float total = 0.0f;
                    for (int k = 0; k < pillText.size(); ++k)
                        if (pillRow[(size_t) k] == row) total += juce::GlyphArrangement::getStringWidth (font (fs, 0.08f, true), pillText[k]) + 16.0f;
                    if (total <= 694.0f - 70.0f) break;
                    fs -= 0.5f;
                }
                pillFs[row] = fs;
            }
            for (int k = 0; k < pillText.size(); ++k)
                pillW.push_back (juce::GlyphArrangement::getStringWidth (font (pillFs[pillRow[(size_t) k]], 0.08f, true), pillText[k]) + 12.0f);
        }
        spaced (g, "GAMME", { 336.0f, 76.0f, 64.0f, 20.0f }, 10.5f, 0.25f, juce::Justification::centredLeft, gold.withAlpha (0.7f));
        float xr[2] = { 336.0f + 66.0f, 336.0f + 66.0f };
        numModePills = 0;
        for (size_t k = 0; k < pillW.size(); ++k)
        {
            const int row = pillRow[k], v = pillVal[k];
            const auto r = juce::Rectangle<float> (xr[row], 76.0f + (float) row * 23.0f, pillW[k], 20.0f);
            const bool on = mc == v, empty = v >= 0 && pillCount[(size_t) v] == 0, current = v >= 0 && mc < 0 && v == pat.mode;
            if (on) { g.setColour (gold); g.fillRoundedRectangle (r, 6.0f); }
            else { g.setColour (juce::Colour (0xd80a0910)); g.fillRoundedRectangle (r, 6.0f); g.setColour (gold.withAlpha (current ? 0.8f : 0.22f)); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, current ? 1.5f : 1.0f); }
            spaced (g, pillText[(int) k], r, pillFs[row], 0.08f, juce::Justification::centred,
                    on ? juce::Colour (0xff14110a) : gold.withAlpha (empty ? 0.28f : 0.85f), true);
            if (numModePills < 24) { modePills[numModePills] = r; modePillValue[numModePills] = v; modePillEmpty[numModePills] = empty; ++numModePills; }
            xr[row] += pillW[k] + 4.0f;
            if (v == -3) xr[row] += 6.0f;
        }
    }
    // morceau
    glass (g, songBox);
    spaced (g, "<", songBox.withWidth (34.0f), 15.0f, 0.0f, juce::Justification::centred, gold);
    spaced (g, ">", songBox.withTrimmedLeft (songBox.getWidth() - 34.0f), 15.0f, 0.0f, juce::Justification::centred, gold);
    spaced (g, pat.songName.toUpperCase(), songBox.reduced (36.0f, 0.0f), 14.0f, 0.12f, juce::Justification::centred, gold, true);

    // --- planete
    drawPlanet (g, { 590.0f, 285.0f }, 135.0f);
    const bool playing = proc.previewPlaying.load();
    glass (g, previewBtn, 10.0f);
    if (playing) { g.setColour (gold.withAlpha (0.2f)); g.fillRoundedRectangle (previewBtn, 10.0f); }
    spaced (g, playing ? juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xa0  STOP")) : juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6  PR\xc3\x89VISUALISER")),
            previewBtn, 11.0f, 0.25f, juce::Justification::centred, gold, true);

    // --- generer
    glass (g, prevBtn); glass (g, nextBtn);
    spaced (g, "<", prevBtn, 18.0f, 0.0f, juce::Justification::centred, gold.withAlpha (proc.canGoBack() ? 1.0f : 0.3f));
    spaced (g, ">", nextBtn, 18.0f, 0.0f, juce::Justification::centred, gold.withAlpha (proc.canGoForward() ? 1.0f : 0.3f));
    g.setColour (gold.withAlpha (0.35f));
    g.fillRoundedRectangle (genBtn.expanded (6.0f), 14.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (genDown ? 0xffd7ad5c : 0xfff5d995), genBtn.getX(), genBtn.getY(),
                                             juce::Colour (genDown ? 0xffb8903f : 0xffd7ad5c), genBtn.getX(), genBtn.getBottom(), false));
    g.fillRoundedRectangle (genBtn, 10.0f);
    spaced (g, juce::String (juce::CharPointer_UTF8 ("G\xc3\x89N\xc3\x89RER")), genBtn, 16.0f, 0.35f, juce::Justification::centred, juce::Colour (0xff15110a), true);

    // --- glisser le MIDI
    {
        const auto box = dragBox();
        glass (g, box, 10.0f);
        spaced (g, juce::String (juce::CharPointer_UTF8 ("\xe2\x87\xa3")), box.withWidth (30.0f).withTrimmedLeft (10.0f), 14.0f, 0.0f,
                juce::Justification::centredLeft, gold);
        static const char* chips[numChips] = { "MELO", "ACC", "CONTRE", "ARP", "PAD", "808", "TOUT" };
        static const float chipW[numChips] = { 52.0f, 44.0f, 64.0f, 44.0f, 44.0f, 44.0f, 52.0f };
        float x = box.getX() + 28.0f;
        for (int i = 0; i < numChips; ++i)
        {
            const float w = chipW[i];
            dragChips[i] = { x, box.getY() + 6.0f, w, box.getHeight() - 12.0f };
            g.setColour (gold.withAlpha (hoverChip == i ? 0.25f : 0.08f));
            g.fillRoundedRectangle (dragChips[i], 6.0f);
            g.setColour (gold.withAlpha (0.35f));
            g.drawRoundedRectangle (dragChips[i], 6.0f, 1.0f);
            const bool empty = chipPart[i] < NumParts && pat.parts[(size_t) chipPart[i]].empty();
            spaced (g, chips[i], dragChips[i], 10.5f, 0.15f, juce::Justification::centred, gold.withAlpha (empty ? 0.3f : 1.0f), true);
            x += w + 4.0f;
        }
    }
    // --- tonalite / bpm
    glass (g, keyBox, 10.0f);
    {
        const juce::String ton = juce::String (keyNames[pat.key]) + (pat.minor ? "m" : "") + (proc.getKeyChoice() == 0 ? " (auto)" : "");
        spaced (g, "TON", keyBox.withWidth (60.0f).withTrimmedLeft (14.0f), 11.0f, 0.2f, juce::Justification::centredLeft, gold.withAlpha (0.7f));
        spaced (g, ton + juce::String (juce::CharPointer_UTF8 (" \xe2\x96\xbe")), keyBox.withX (keyBox.getX() + 46.0f).withWidth (110.0f), 12.5f, 0.1f, juce::Justification::centredLeft, gold, true);
        spaced (g, pat.bpm > 0 ? "BPM " + juce::String (pat.bpm) : juce::String ("BPM -"), keyBox.withTrimmedLeft (150.0f).withTrimmedRight (12.0f), 11.0f, 0.2f, juce::Justification::centredRight, gold);
    }

    // --- master
    {
        const auto box = juce::Rectangle<float> ((float) W - 24 - 106, 110.0f, 106.0f, 424.0f);
        glass (g, box);
        spaced (g, "MASTER", box.withHeight (36.0f).withY (box.getY() + 8.0f), 11.0f, 0.25f, juce::Justification::centred, gold);
        spaced (g, juce::String (juce::roundToInt (master.getValue() * 100.0)) + "%", { box.getX(), 184.0f, box.getWidth(), 16.0f }, 11.0f, 0.05f,
                juce::Justification::centred, gold.withAlpha (0.6f));
    }

    // --- clavier
    glass (g, { 24.0f, 548.0f, (float) W - 48.0f, 170.0f }, 14.0f);
}

void RantiEditor::paintOverChildren (juce::Graphics& g)
{
    // vumetre + clavier (rafraichis par le timer)
    const auto box = juce::Rectangle<float> ((float) W - 24 - 106, 110.0f, 106.0f, 424.0f);
    const float m = juce::jlimit (0.0f, 1.0f, proc.getMeter());
    const float db = juce::Decibels::gainToDecibels (m, -60.0f);
    const float lvl = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
    const auto meterArea = juce::Rectangle<float> (box.getCentreX() - 14.0f, 220.0f, 28.0f, 260.0f);
    for (int ch = 0; ch < 2; ++ch)
    {
        const float x = meterArea.getX() + ch * 18.0f;
        const float lv = juce::jlimit (0.0f, 1.0f, lvl * (ch == 0 ? 1.0f : 0.94f));
        for (int s = 0; s < 52; ++s)
        {
            const float y = meterArea.getBottom() - (float) s * 5.0f - 3.0f;
            const bool lit = (float) s / 52.0f < lv;
            g.setColour (lit ? gold : gold.withAlpha (0.1f));
            g.fillRect (x, y, 10.0f, 3.0f);
        }
    }
    spaced (g, (m > 0.0001f ? juce::String (db, 1) : juce::String ("-inf")) + " dB", { box.getX(), 496.0f, box.getWidth(), 18.0f }, 11.0f, 0.05f,
            juce::Justification::centred, gold.withAlpha (0.75f));
    drawKeyboard (g, { 36.0f, 560.0f, (float) W - 72.0f, 146.0f });
    drawChords (g);
}

void RantiEditor::drawChords (juce::Graphics& g)
{
    const auto pat = proc.getPattern();
    const double ph = proc.getPlayhead();
    spaced (g, juce::String (juce::CharPointer_UTF8 ("ACCORDS G\xc3\x89N\xc3\x89R\xc3\x89S")), chordBox.withHeight (18.0f), 13.0f, 0.22f,
            juce::Justification::centredLeft, gold, true);
    int cur = -1;
    if (ph >= 0.0)
        for (int i = 0; i < (int) pat.chords.size(); ++i) if (pat.chords[(size_t) i].start <= ph + 1.0e-3) cur = i;
    float x = chordBox.getX(), y = chordBox.getY() + 26.0f;
    const auto f = font (11.5f, 0.03f, true);
    // morceaux longs : la liste suit la lecture (on montre 12 accords autour de l'accord en cours)
    const int first = cur >= 9 ? cur - 3 : 0;
    for (int i = first; i < (int) pat.chords.size() && i < first + 12; ++i)
    {
        const auto& nm = pat.chords[(size_t) i].name;
        const float w = juce::jmax (30.0f, juce::GlyphArrangement::getStringWidth (f, nm) + 12.0f);
        if (x + w > chordBox.getRight()) { x = chordBox.getX(); y += 28.0f; if (y + 22.0f > chordBox.getBottom()) break; }
        const auto r = juce::Rectangle<float> (x, y, w, 22.0f);
        if (i == cur) { g.setColour (gold.withAlpha (0.4f)); g.fillRoundedRectangle (r.expanded (3.0f), 8.0f); g.setColour (gold); g.fillRoundedRectangle (r, 6.0f); }
        else { g.setColour (gold.withAlpha (0.08f)); g.fillRoundedRectangle (r, 6.0f); g.setColour (gold.withAlpha (0.35f)); g.drawRoundedRectangle (r, 6.0f, 1.0f); }
        g.setFont (f);
        g.setColour (i == cur ? juce::Colour (0xff14110a) : gold);
        g.drawText (nm, r, juce::Justification::centred);
        x += w + 5.0f;
    }
    if (pat.chords.empty())
        spaced (g, "-", { chordBox.getX(), chordBox.getY() + 26.0f, 40.0f, 22.0f }, 12.0f, 0.0f, juce::Justification::centredLeft, gold.withAlpha (0.5f));
}

void RantiEditor::refreshPlanetBase()
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

void RantiEditor::timerCallback()
{
    ranti::useSoftwareRenderer (*this, rendererSet);
    const bool animFrame = (++animTick & 1) == 0;
    spin = (float) std::fmod (juce::Time::getMillisecondCounterHiRes() / 12000.0, 1.0);   // un tour en 12 s, meme vitesse partout
    {
        const bool pl = proc.previewPlaying.load() || proc.getPlayhead() >= 0.0;
        const int v = proc.getPatternVersion();
        if (! planetLayer.base.isValid() || v != planetVersion || pl != planetPlaying) { planetVersion = v; planetPlaying = pl; refreshPlanetBase(); }
        if (animFrame && planetLayer.readyForNextFrame()) planetLayer.repaint();
    }
    keys = proc.getActiveKeys();
    const int v = proc.getPatternVersion();
    if (v != lastVersion)
    {
        lastVersion = v;
        repaint();
        for (auto* r : { &rowMel, &rowChords, &rowExtra, &rowArp, &rowPad, &rowBass, &rowVerb }) r->repaint();
        return;
    }
    repaint (juce::Rectangle<int> (W - 24 - 106, 210, 106, 310));
    repaint (juce::Rectangle<int> (24, 548, W - 48, 170));
    repaint (chordBox.toNearestInt().expanded (2).withLeft (planetLayer.getRight()));
}

// ------------------------------------------------------------------ souris
void RantiEditor::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < numChips; ++i) if (dragChips[i].contains (e.position)) h = i;
    if (h != hoverChip) { hoverChip = h; repaint (dragBox().toNearestInt()); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void RantiEditor::mouseDown (const juce::MouseEvent& e)
{
    if (genBtn.contains (e.position)) { genDown = true; repaint (genBtn.expanded (8.0f).toNearestInt()); }
}

void RantiEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging) return;
    for (int i = 0; i < numChips; ++i)
        if (dragChips[i].contains (e.mouseDownPosition))
        {
            auto f = proc.writeTempMidi (chipPart[i]);
            if (f.existsAsFile())
            {
                dragging = true;
                juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
            }
            return;
        }
}

void RantiEditor::mouseUp (const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (genDown) { genDown = false; repaint(); }
    if (e.mouseWasDraggedSinceMouseDown()) return;
    if (genBtn.contains (p)) { proc.generate(); return; }
    if (prevBtn.contains (p)) { proc.goBack(); return; }
    if (nextBtn.contains (p)) { proc.goForward(); return; }
    for (int i = 0; i < numGenres; ++i)
        if (genreTabs[i].contains (p)) { proc.setGenre (i); return; }
    if (songBox.contains (p))
    {
        if (p.x < songBox.getX() + 40.0f) proc.stepSong (-1);
        else if (p.x > songBox.getRight() - 40.0f) proc.stepSong (1);
        return;
    }
    if (keyBox.contains (p) && p.x < keyBox.getX() + 150.0f) { showKeyMenu(); return; }
    for (int i = 0; i < numModePills; ++i)
        if (modePills[i].contains (p)) { if (! modePillEmpty[i]) { proc.setMode (modePillValue[i]); repaint(); } return; }
    if (previewBtn.contains (p) || p.getDistanceFrom ({ 590.0f, 285.0f }) < 140.0f)
    {
        proc.togglePreview();
        repaint();
    }
}

// ------------------------------------------------------------------ menus
void RantiEditor::showInstMenu (const char* id, bool bass)
{
    auto* prm = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id));
    if (prm == nullptr) return;
    juce::PopupMenu m;
    const auto& names = prm->choices;
    // POP WAVE : uniquement les sons crees pour la POP WAVE (PW ...)
    if (proc.getState().genre == 4)
    {
        // rangement par familles (Basses, Cordes, Leads, Claviers, Cuivres & bois, Orgues, Effets...)
        std::map<juce::String, juce::PopupMenu> fam;
        juce::StringArray order;
        for (int i = 0; i < names.size(); ++i)
        {
            if (! names[i].startsWith ("PW ")) continue;
            auto c = ranti::SoundBank::get().categoryOf (names[i]);
            if (c.isEmpty()) c = "Autres";
            if (! order.contains (c)) order.add (c);
            fam[c].addItem (i + 1, names[i].fromFirstOccurrenceOf ("PW ", false, false), true, prm->getIndex() == i);
        }
        order.sort (true);
        m.addSectionHeader ("POP WAVE");
        for (auto& c : order) m.addSubMenu (c, fam[c]);
        m.showMenuAsync (juce::PopupMenu::Options().withParentComponent (this), [this, prm] (int r)
        {
            if (r <= 0) return;
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (r - 1)));
            prm->endChangeGesture();
            repaint();
            for (auto* row : { &rowMel, &rowChords, &rowExtra, &rowArp, &rowPad, &rowBass }) row->repaint();
        });
        return;
    }
    juce::String lastGroup;
    for (int i = 0; i < names.size(); ++i)
    {
        if (names[i].startsWith ("PW ")) continue;   // sons POP WAVE : seulement dans la section POP WAVE
        if (! bass && i > 0)
        {
            const auto* in = SoundBank::get().melodic (i);
            if (in != nullptr && in->group != lastGroup) { m.addSectionHeader (in->group.toUpperCase()); lastGroup = in->group; }
        }
        m.addItem (i + 1, names[i], true, prm->getIndex() == i);
    }
    m.showMenuAsync (juce::PopupMenu::Options().withParentComponent (this), [this, prm] (int r)
    {
        if (r <= 0) return;
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (r - 1)));
        prm->endChangeGesture();
        repaint();
        for (auto* row : { &rowMel, &rowChords, &rowExtra, &rowArp, &rowPad, &rowBass }) row->repaint();
    });
}

void RantiEditor::showGrooveMenu()
{
    const auto st = proc.getState();
    juce::PopupMenu m;
    for (int i = 0; i < numGrooves; ++i)
        if (grooves[i].genre == st.genre || (grooves[i].genre == 0 && ! [&] { for (int j = 0; j < numGrooves; ++j) if (grooves[j].genre == st.genre) return true; return false; }()))
            m.addItem (i + 1, juce::String (grooves[i].name) + "  (" + juce::String (grooves[i].bpm) + " bpm)", true, st.groove == i);
    m.showMenuAsync (juce::PopupMenu::Options().withParentComponent (this), [this] (int r) { if (r > 0) proc.setGroove (r - 1); });
}

void RantiEditor::showKeyMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Auto (ton du MIDI)", true, proc.getKeyChoice() == 0);
    m.addSeparator();
    for (int i = 0; i < 12; ++i) m.addItem (i + 2, keyNames[i], true, proc.getKeyChoice() == i + 1);
    m.showMenuAsync (juce::PopupMenu::Options().withParentComponent (this), [this] (int r) { if (r > 0) proc.setKey (r - 1); });
}
