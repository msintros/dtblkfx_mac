// Offline smoke test: runs the plug-in's processor on a test signal and checks the output.
//   DtBlkFxProcessTest                    exit code 0 = all checks passed
//   DtBlkFxProcessTest --screenshot x.png also renders the editor while audio plays
#include "DtBlkFxEditor.h"
#include "DtBlkFxProcessor.h"
#include <cstdio>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace {

struct Stats {
  float peak = 0.0f;
  double rms = 0.0;
  int nonFinite = 0;
};

Stats run(DtBlkFxAudioProcessor& p, bool limiterOn, float inputGain, int seconds = 2)
{
  const double sr = 44100.0;
  const int block = 512;
  p.setRateAndBufferSizeDetails(sr, block);
  p.prepareToPlay(sr, block);

  p.apvts.getParameter(DtBlkFxAudioProcessor::limiterEnabledId)->setValueNotifyingHost(limiterOn ? 1.0f : 0.0f);

  juce::AudioBuffer<float> buf(2, block);
  juce::MidiBuffer midi;
  Stats s;
  long n = 0, total = 0;
  double phase = 0.0;
  juce::Random rng(1);

  for (long done = 0; done < (long)(sr * seconds); done += block) {
    for (int i = 0; i < block; ++i) {
      float v = inputGain * (0.5f * (float)std::sin(phase) + 0.1f * (rng.nextFloat() * 2.0f - 1.0f));
      phase += 2.0 * juce::MathConstants<double>::pi * 440.0 / sr;
      buf.setSample(0, i, v);
      buf.setSample(1, i, v);
    }
    p.processBlock(buf, midi);

    // skip the first half second (FFT latency / limiter attack)
    bool measure = done > (long)(sr / 2);
    for (int c = 0; c < 2; ++c)
      for (int i = 0; i < block; ++i) {
        float v = buf.getSample(c, i);
        if (!std::isfinite(v)) {
          ++s.nonFinite;
          continue;
        }
        if (measure) {
          s.peak = std::max(s.peak, std::abs(v));
          s.rms += (double)v * v;
          ++n;
        }
      }
    total += block;
  }
  p.releaseResources();
  s.rms = n ? std::sqrt(s.rms / (double)n) : 0.0;
  return s;
}

} // namespace

// render the editor after a few seconds of a test signal, so the spectrograms have content
int screenshot(const juce::File& file)
{
  std::unique_ptr<DtBlkFxAudioProcessor> p(dynamic_cast<DtBlkFxAudioProcessor*>(createPluginFilter()));
  const double sr = 44100.0;
  const int block = 512;
  p->setRateAndBufferSizeDetails(sr, block);
  p->prepareToPlay(sr, block);
  // give the effect rows some spread so the read-outs show real values
  for (int row = 0; row < 8; ++row) {
    int base = BlkFxParam::NUM_GLOBAL_PARAMS + row * BlkFxParam::NUM_FX_PARAMS;
    p->apvts.getParameter("param_" + juce::String(base + 0))->setValueNotifyingHost(0.35f + 0.05f * row);
    p->apvts.getParameter("param_" + juce::String(base + 1))->setValueNotifyingHost(0.55f + 0.05f * row);
  }
  std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
  auto* editor = dynamic_cast<DtBlkFxEditor*>(ed.get());

  juce::AudioBuffer<float> buf(2, block);
  juce::MidiBuffer midi;
  juce::Random rng(2);
  double t = 0.0;
  const int blocksPerTick = (int)(sr / 60.0 / block) + 1;

  for (int tick = 0; tick < 500; ++tick) {
    for (int b = 0; b < blocksPerTick; ++b) {
      for (int i = 0; i < block; ++i, t += 1.0 / sr) {
        // a melody of harmonic tones with a drum-like noise burst every half second
        double f0 = 110.0 * std::pow(2.0, (double)((int)(t * 2.0) % 5) * 3.0 / 12.0);
        double v = 0.0;
        for (int h = 1; h <= 12; ++h)
          v += std::sin(2.0 * juce::MathConstants<double>::pi * f0 * h * t) / h;
        double env = std::exp(-std::fmod(t, 0.5) * 12.0);
        v = 0.15 * v + 0.3 * env * (rng.nextFloat() * 2.0 - 1.0);
        buf.setSample(0, i, (float)v);
        buf.setSample(1, i, (float)v);
      }
      p->processBlock(buf, midi);
    }
    editor->timerCallback();
  }

  auto image = editor->createComponentSnapshot(editor->getLocalBounds());
  juce::FileOutputStream out(file);
  out.setPosition(0);
  out.truncate();
  juce::PNGImageFormat().writeImageToStream(image, out);
  std::printf("wrote %s (%dx%d)\n", file.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
  ed.reset();
  return 0;
}

int main(int argc, char** argv)
{
  juce::ScopedJuceInitialiser_GUI juce;
  if (argc == 3 && juce::String(argv[1]) == "--screenshot")
    return screenshot(juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]));

  std::unique_ptr<DtBlkFxAudioProcessor> p(dynamic_cast<DtBlkFxAudioProcessor*>(createPluginFilter()));
  int failures = 0;

  auto report = [&](const char* name, const Stats& s, bool ok) {
    std::printf("%-40s peak %.4f  rms %.4f  non-finite %d  %s\n", name, s.peak, s.rms, s.nonFinite, ok ? "ok" : "FAIL");
    if (!ok)
      ++failures;
  };

  auto off = run(*p, false, 1.0f);
  report("limiter off", off, off.nonFinite == 0 && off.rms > 0.05);

  // below the ceiling the limiter must be transparent
  auto on = run(*p, true, 1.0f);
  report("limiter on, default settings", on,
         on.nonFinite == 0 && std::abs(on.rms - off.rms) < 0.01 * off.rms);

  // a signal far above 0 dBFS must come out at the ceiling, not silent
  auto loud = run(*p, true, 8.0f);
  float defaultCeiling = juce::Decibels::decibelsToGain(-0.1f);
  report("limiter on, input +18 dB", loud,
         loud.nonFinite == 0 && loud.rms > 0.2 && loud.peak <= defaultCeiling + 1e-4f);

  auto setLim = [&](const char* id, float v) {
    auto* prm = p->apvts.getParameter(id);
    prm->setValueNotifyingHost(prm->convertTo0to1(v));
  };

  setLim(DtBlkFxAudioProcessor::limiterGainId, 12.0f);
  setLim(DtBlkFxAudioProcessor::limiterCeilingId, -6.0f);
  auto hot = run(*p, true, 1.0f);
  float ceiling = juce::Decibels::decibelsToGain(-6.0f);
  report("limiter on, +12 dB gain, -6 dB ceiling", hot,
         hot.nonFinite == 0 && hot.rms > 0.05 && hot.peak <= ceiling + 1e-4f);

  return failures == 0 ? 0 : 1;
}
