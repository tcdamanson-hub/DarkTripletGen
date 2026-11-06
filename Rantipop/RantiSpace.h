#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>

// Fond "espace" (nebuleuses, poussiere, etoiles scintillantes, vignette), dessine une seule fois.
// warm = true : touches dorees (MELODIE) ; false : bleu / violet (DRUMS)
namespace ranti
{
inline juce::Image makeSpace (int W, int H, int seed, bool warm)
{
    juce::Image img (juce::Image::ARGB, W, H, true, juce::SoftwareImageType());
    juce::Graphics g (img);
    juce::Random r (seed);
    auto cloud = [&] (float cx, float cy, float rx, float ry, juce::Colour c)
    {
        juce::ColourGradient gr (c, cx, cy, c.withAlpha (0.0f), cx + rx, cy, true);
        gr.addColour (0.45, c.withMultipliedAlpha (0.45f));
        g.setGradientFill (gr);
        g.fillEllipse (cx - rx, cy - ry, rx * 2.0f, ry * 2.0f);
    };
    const float w = (float) W, h = (float) H;
    // nebuleuses
    const juce::Colour deep (0xff1b2a6e), violet (0xff4a2a7a), teal (0xff1d5a7a);
    const juce::Colour accent = warm ? juce::Colour (0xffc9923a) : juce::Colour (0xff3a7bd5);
    cloud (w * 0.16f, h * 0.22f, w * 0.30f, h * 0.34f, deep.withAlpha (0.30f));
    cloud (w * 0.84f, h * 0.72f, w * 0.32f, h * 0.36f, violet.withAlpha (0.24f));
    cloud (w * 0.70f, h * 0.12f, w * 0.22f, h * 0.20f, teal.withAlpha (0.16f));
    cloud (w * 0.30f, h * 0.80f, w * 0.26f, h * 0.22f, violet.withAlpha (0.14f));
    cloud (w * 0.50f, h * 0.40f, w * 0.30f, h * 0.30f, accent.withAlpha (warm ? 0.10f : 0.12f));
    // grain / poussiere d'etoiles
    for (int i = 0; i < 2600; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (r.nextFloat() * 0.10f + 0.02f));
        g.fillRect (r.nextFloat() * w, r.nextFloat() * h, 1.0f, 1.0f);
    }
    // etoiles
    const juce::Colour starWarm = warm ? juce::Colour (0xffffe2a0) : juce::Colour (0xffa0c8ff);
    for (int i = 0; i < 520; ++i)
    {
        const float a = r.nextFloat() * 0.75f + 0.15f, s = r.nextFloat() * 1.8f + 0.5f;
        g.setColour ((r.nextFloat() < 0.3f ? starWarm : juce::Colour (0xfff4f8ff)).withAlpha (a));
        g.fillEllipse (r.nextFloat() * w, r.nextFloat() * h, s, s);
    }
    // etoiles brillantes avec halo
    for (int i = 0; i < 34; ++i)
    {
        const float x = r.nextFloat() * w, y = r.nextFloat() * h, rad = r.nextFloat() * 7.0f + 4.0f;
        const auto c = (r.nextFloat() < 0.5f ? juce::Colour (0xffcfe2ff) : starWarm);
        juce::ColourGradient gl (c.withAlpha (0.55f), x, y, c.withAlpha (0.0f), x + rad, y, true);
        g.setGradientFill (gl);
        g.fillEllipse (x - rad, y - rad, rad * 2.0f, rad * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.fillEllipse (x - 1.2f, y - 1.2f, 2.4f, 2.4f);
    }
    // quelques etoiles scintillantes (croix de lumiere)
    for (int i = 0; i < 9; ++i)
    {
        const float x = r.nextFloat() * w, y = r.nextFloat() * h, len = r.nextFloat() * 14.0f + 10.0f;
        const auto c = juce::Colour (0xffe6f0ff);
        for (int k = 0; k < 2; ++k)
        {
            juce::ColourGradient ray (c.withAlpha (0.7f), x, y, c.withAlpha (0.0f), k == 0 ? x + len : x, k == 0 ? y : y + len, true);
            g.setGradientFill (ray);
            if (k == 0) g.fillRect (x - len, y - 0.6f, len * 2.0f, 1.2f);
            else        g.fillRect (x - 0.6f, y - len, 1.2f, len * 2.0f);
        }
        juce::ColourGradient core (juce::Colours::white.withAlpha (0.9f), x, y, c.withAlpha (0.0f), x + 5.0f, y, true);
        g.setGradientFill (core);
        g.fillEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f);
    }
    // vignette
    {
        juce::ColourGradient v (juce::Colours::transparentBlack, w * 0.5f, h * 0.45f, juce::Colours::black.withAlpha (0.55f), w * 1.02f, h * 0.45f, true);
        v.addColour (0.6, juce::Colours::transparentBlack);
        g.setGradientFill (v);
        g.fillRect (0.0f, 0.0f, w, h);
    }
    return img;
}
} // namespace ranti

namespace ranti
{
// planete qui tourne : meridiens qui defilent sur la sphere (le visage reste fixe)
inline void drawSpin (juce::Graphics& g, juce::Point<float> c, float r, float phase, juce::Colour col)
{
    r -= 3.0f;
    const int N = 9;
    for (int i = 0; i < N; ++i)
    {
        const float lam = phase * juce::MathConstants<float>::twoPi + (float) i * juce::MathConstants<float>::pi / (float) N;
        for (float side : { 1.0f, -1.0f })
        {
            const float L = lam + (side < 0.0f ? juce::MathConstants<float>::pi : 0.0f);
            const float front = std::cos (L);
            if (front <= 0.0f) continue;
            juce::Path m;
            for (int k = 0; k <= 24; ++k)
            {
                const float phi = -juce::MathConstants<float>::halfPi + (float) k * juce::MathConstants<float>::pi / 24.0f;
                const juce::Point<float> p (c.x + r * std::sin (L) * std::cos (phi), c.y + r * std::sin (phi));
                if (k == 0) m.startNewSubPath (p); else m.lineTo (p);
            }
            g.setColour (col.withAlpha (0.04f + 0.12f * front));
            g.strokePath (m, juce::PathStrokeType (1.2f));
        }
    }
    // paralleles (fixes), seulement la partie avant
    for (float lat : { -0.55f, -0.2f, 0.2f, 0.55f })
    {
        const float y = c.y + r * std::sin (lat), rx = r * std::cos (lat);
        juce::Path pl;
        pl.addCentredArc (c.x, y, rx, rx * 0.12f, 0.0f, juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi + juce::MathConstants<float>::pi, true);
        g.setColour (col.withAlpha (0.06f));
        g.strokePath (pl, juce::PathStrokeType (1.0f));
    }
}

// petite lumiere qui file sur l'anneau (partie avant)
inline void drawOrbitSpark (juce::Graphics& g, juce::Point<float> c, float k, float phase, juce::Colour col)
{
    const float t = phase * juce::MathConstants<float>::twoPi * 2.0f;
    for (int i = 0; i < 6; ++i)
    {
        const float tt = t - (float) i * 0.05f;
        if (std::sin (tt) <= 0.05f) continue;
        juce::Point<float> p (162.0f * k * std::cos (tt), 42.0f * k * std::sin (tt));
        p = p.transformedBy (juce::AffineTransform::rotation (juce::degreesToRadians (-7.0f)).translated (c.x, c.y + 50.0f * k));
        const float a = 1.0f - (float) i / 6.0f, rad = 7.0f * a + 2.0f;
        juce::ColourGradient gl (col.withAlpha (0.8f * a), p.x, p.y, col.withAlpha (0.0f), p.x + rad, p.y, true);
        g.setGradientFill (gl);
        g.fillEllipse (p.x - rad, p.y - rad, rad * 2.0f, rad * 2.0f);
    }
}
} // namespace ranti

namespace ranti
{
// le visage tourne avec la planete : chaque point est place sur la sphere puis tourne autour de l'axe vertical ;
// ce qui passe derriere disparait. Renvoie la visibilite (0..1) pour attenuer la lueur pres du bord.
inline float spinPath (juce::Path& p, juce::Point<float> c, float r, float phase)
{
    const float th = phase * juce::MathConstants<float>::twoPi;
    juce::Path out;
    juce::PathFlatteningIterator it (p, {}, 0.6f);
    bool penDown = false;
    int guard = 0;
    while (it.next() && ++guard < 4000)
    {
        for (int e = 0; e < 2; ++e)
        {
            const float x = e == 0 ? it.x1 : it.x2, y = e == 0 ? it.y1 : it.y2;
            if (! std::isfinite (x) || ! std::isfinite (y)) { penDown = false; continue; }
            const float dy = juce::jlimit (-0.98f, 0.98f, (y - c.y) / r);
            const float lat = std::asin (dy), rc = r * std::cos (lat);
            const float lon = std::asin (juce::jlimit (-0.98f, 0.98f, (x - c.x) / rc)) + th;
            const bool vis = std::cos (lon) > 0.0f;
            const juce::Point<float> q (c.x + rc * std::sin (lon), y);
            if (! vis) { penDown = false; continue; }
            if (e == 0 && (it.subPathIndex == 0 || ! penDown)) { out.startNewSubPath (q); penDown = true; }
            else if (e == 1) { if (! penDown) { out.startNewSubPath (q); penDown = true; } else out.lineTo (q); }
        }
        if (it.closesSubPath) penDown = false;
    }
    p = out;
    return juce::jmax (0.0f, std::cos (th));
}
} // namespace ranti

namespace ranti
{
// Calque de la planete : le decor fixe est photographie une fois (image), seule la rotation est redessinee.
// Le calque est opaque : l'animation ne redessine jamais le reste de l'interface (CPU tres bas).
struct PlanetLayer : juce::Component
{
    juce::Image base;
    std::function<void (juce::Graphics&)> drawDynamic;   // dessine en coordonnees de la fenetre parente
    std::function<void()> needBase;                       // demande la photo du decor fixe
    PlanetLayer() { setOpaque (true); setInterceptsMouseClicks (false, false); }
    double lastPaintMs = 0.0, lastPaintEnd = 0.0;
    // l'animation ne prend jamais plus d'environ 20 % du temps : si le dessin est lent, on espace les images
    bool readyForNextFrame() const
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        return now - lastPaintEnd > juce::jmax (55.0, juce::jmin (lastPaintMs * 3.0, 250.0));
    }
    void paint (juce::Graphics& g) override
    {
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        // seule une image complete compte : un petit bout redessine par la fenetre (ex. la case des accords
        // qui touche le calque) ne doit pas bloquer l'animation (c'est ce qui figeait la planete jaune)
        const bool full = g.getClipBounds().contains (getLocalBounds().reduced (4));
        struct Done { PlanetLayer& p; double t0; bool full; ~Done() { if (! full) return; p.lastPaintEnd = juce::Time::getMillisecondCounterHiRes(); p.lastPaintMs = p.lastPaintEnd - t0; } } done { *this, t0, full };
        if (! base.isValid() && needBase) needBase();
        if (base.isValid()) g.drawImage (base, getLocalBounds().toFloat());
        if (drawDynamic)
        {
            g.addTransform (juce::AffineTransform::translation ((float) -getX(), (float) -getY()));
            drawDynamic (g);
        }
    }
};
} // namespace ranti

namespace ranti
{
// Sous Windows, certains hotes (FL) affichent mal le rendu Direct2D quand une zone est animee :
// on force le rendu logiciel, stable partout.
inline void useSoftwareRenderer (juce::Component& c, bool& done)
{
    if (done) return;   // une seule fois : changer de moteur d'affichage en boucle figerait la fenetre
    if (auto* peer = c.getPeer())
    {
        done = true;
        const int sw = peer->getAvailableRenderingEngines().indexOf ("Software Renderer");
        if (sw >= 0 && peer->getCurrentRenderingEngine() != sw) peer->setCurrentRenderingEngine (sw);
    }
}
} // namespace ranti
