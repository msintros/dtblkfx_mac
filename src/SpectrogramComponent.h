#pragma once

#include "core/BlkFxParam.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Scrolling spectrogram ("waterfall") in the style of the original DtBlkFx GUI: frequency runs
// left to right on the same exponential scale as the Freq A/B parameters, time scrolls
// downwards with the newest block at the top, and power is shown with a dark purple -> red ->
// yellow -> green colour map.
//
// The editor feeds it with pushSpectrum() whenever the processor has a new FFT block and calls
// advance() once per timer tick, so the scroll speed doesn't depend on FFT length or overlap.
class SpectrogramComponent : public juce::Component {
public:
  explicit SpectrogramComponent(const juce::String& labelText)
      : label(labelText)
  {
    setOpaque(true);
    buildColourMap();
  }

  void setLabel(const juce::String& newLabel)
  {
    label = newLabel;
    repaint();
  }

  void setSampleRate(double newSampleRate)
  {
    if (newSampleRate > 0.0 && newSampleRate != sampleRate) {
      sampleRate = newSampleRate;
      pixelBins.clear();
    }
  }

  // floorDb: power shown as the darkest colour, rangeDb: span from darkest to brightest
  void setLevels(float newFloorDb, float newRangeDb)
  {
    floorDb = newFloorDb;
    rangeDb = juce::jmax(1.0f, newRangeDb);
  }

  // power per FFT bin (DC..Nyquist), already normalised so a full scale sine is about 0.25
  void pushSpectrum(const std::vector<float>& power)
  {
    if ((int)power.size() != (int)latest.size())
      pixelBins.clear();
    latest = power;
  }

  // scroll by one row, drawing the most recent spectrum (repeated if no new block arrived)
  void advance()
  {
    if (image.isNull())
      return;

    if (pixelBins.empty() && !latest.empty())
      buildPixelBins((int)latest.size());

    topRow = (topRow + image.getHeight() - 1) % image.getHeight();

    juce::Image::BitmapData bd(image, 0, topRow, image.getWidth(), 1,
                               juce::Image::BitmapData::writeOnly);
    for (int x = 0; x < bd.width; ++x)
      bd.setPixelColour(x, 0, colourFor(powerAtPixel(x)));

    repaint();
  }

  void clear()
  {
    if (image.isValid())
      image.clear(image.getBounds(), colourMap.front());
    repaint();
  }

  void paint(juce::Graphics& g) override
  {
    g.fillAll(juce::Colours::black);
    if (image.isNull())
      return;

    const int w = image.getWidth();
    const int h = image.getHeight();

    // circular buffer: rows topRow..end are the newest, drawn first
    const int firstChunk = h - topRow;
    g.drawImage(image, 0, 0, w, firstChunk, 0, topRow, w, firstChunk);
    if (topRow > 0)
      g.drawImage(image, 0, firstChunk, w, topRow, 0, 0, w, topRow);

    drawFrequencyGrid(g);

    // lane name
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    auto nameArea = juce::Rectangle<int>(6, 4, 120, 18);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(nameArea.toFloat().withWidth(
                               (float)juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), label) + 10.0f),
                           3.0f);
    g.setColour(juce::Colours::white);
    g.drawText(label, nameArea.reduced(5, 0), juce::Justification::centredLeft, false);

    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawRect(getLocalBounds(), 1);
  }

  void resized() override
  {
    image = juce::Image(juce::Image::RGB, juce::jmax(1, getWidth()), juce::jmax(1, getHeight()), false);
    image.clear(image.getBounds(), colourMap.front());
    topRow = 0;
    pixelBins.clear();
  }

private:
  struct BinSpan {
    float first; // fractional bin at the pixel's left edge
    float last;  // fractional bin at the pixel's right edge
  };

  juce::Image image;
  int topRow = 0;
  juce::String label;

  double sampleRate = 44100.0;
  float floorDb = -80.0f;
  float rangeDb = 66.0f;

  std::vector<float> latest;
  std::vector<BinSpan> pixelBins;
  std::array<juce::Colour, 256> colourMap;

  static float hzAtFraction(float frac) { return BlkFxParam::getHzNonZero(frac); }

  static float fractionAtHz(float hz)
  {
    const float lowest = BlkFxParam::getHzNonZero(0.0f);
    return std::log2(hz / lowest) * 12.0f / BlkFxParam::noteSpan();
  }

  void buildColourMap()
  {
    // matches the colours of the original Windows plug-in's spectrogram
    juce::ColourGradient grad(juce::Colour::fromFloatRGBA(0.02f, 0.0f, 0.05f, 1.0f), 0.0f, 0.0f,
                              juce::Colour::fromFloatRGBA(0.35f, 1.0f, 0.35f, 1.0f), 1.0f, 0.0f,
                              false);
    grad.addColour(0.18, juce::Colour::fromFloatRGBA(0.30f, 0.05f, 0.35f, 1.0f));
    grad.addColour(0.38, juce::Colour::fromFloatRGBA(0.62f, 0.10f, 0.35f, 1.0f));
    grad.addColour(0.58, juce::Colour::fromFloatRGBA(0.92f, 0.25f, 0.12f, 1.0f));
    grad.addColour(0.76, juce::Colour::fromFloatRGBA(1.00f, 0.62f, 0.10f, 1.0f));
    grad.addColour(0.90, juce::Colour::fromFloatRGBA(1.00f, 0.95f, 0.30f, 1.0f));

    for (size_t i = 0; i < colourMap.size(); ++i)
      colourMap[i] = grad.getColourAtPosition((double)i / (double)(colourMap.size() - 1));
  }

  void buildPixelBins(int numBins)
  {
    const int w = image.getWidth();
    pixelBins.resize((size_t)w);
    const float binPerHz = (float)(numBins - 1) / (float)(sampleRate * 0.5);

    for (int x = 0; x < w; ++x) {
      pixelBins[(size_t)x].first = hzAtFraction((float)x / (float)w) * binPerHz;
      pixelBins[(size_t)x].last = hzAtFraction((float)(x + 1) / (float)w) * binPerHz;
    }
  }

  float powerAtPixel(int x) const
  {
    if (latest.empty() || x >= (int)pixelBins.size())
      return 0.0f;

    const auto span = pixelBins[(size_t)x];
    const int maxBin = (int)latest.size() - 1;
    if (span.first >= (float)maxBin)
      return 0.0f;

    // pixel narrower than a bin: interpolate, otherwise show the strongest bin it covers
    if (span.last - span.first < 1.0f) {
      const int b = (int)span.first;
      const float frac = span.first - (float)b;
      return latest[(size_t)b] + frac * (latest[(size_t)juce::jmin(b + 1, maxBin)] - latest[(size_t)b]);
    }

    float peak = 0.0f;
    for (int b = (int)span.first, end = juce::jmin((int)span.last, maxBin); b <= end; ++b)
      peak = juce::jmax(peak, latest[(size_t)b]);
    return peak;
  }

  juce::Colour colourFor(float power) const
  {
    const float db = 10.0f * std::log10(power + 1e-20f);
    const float t = juce::jlimit(0.0f, 1.0f, (db - floorDb) / rangeDb);
    return colourMap[(size_t)(t * (float)(colourMap.size() - 1) + 0.5f)];
  }

  void drawFrequencyGrid(juce::Graphics& g)
  {
    static const float marks[] = {50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
    const float w = (float)getWidth();
    const float h = (float)getHeight();
    const float nyquist = (float)(sampleRate * 0.5);

    g.setFont(juce::FontOptions(10.0f));
    for (float hz : marks) {
      if (hz >= nyquist)
        break;
      const float x = fractionAtHz(hz) * w;
      g.setColour(juce::Colours::white.withAlpha(0.18f));
      g.drawVerticalLine(juce::roundToInt(x), 0.0f, h);

      const juce::String text = hz >= 1000.0f ? juce::String(hz / 1000.0f, 0) + "k" : juce::String(hz, 0);
      auto box = juce::Rectangle<float>(x + 2.0f, h - 14.0f, 30.0f, 12.0f);
      g.setColour(juce::Colours::white.withAlpha(0.7f));
      g.drawText(text, box, juce::Justification::centredLeft, false);
    }
  }

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramComponent)
};
