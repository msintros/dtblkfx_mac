#pragma once

#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>

//==============================================================================
// Stereo-linked brickwall peak limiter for the plug-in output.
//
// Replaces juce::dsp::Limiter, which adds make-up gain so that its output always sits near
// 0 dBFS (+5.6 dB above it) whatever the threshold, so it could not act as a ceiling.
//
// Here the gain reduction tracks the peak envelope with instant attack and an exponential
// release, so the output never exceeds the ceiling and quiet signals pass unchanged.
class OutputLimiter {
public:
  void prepare(double newSampleRate)
  {
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    reset();
  }

  void reset()
  {
    envelope = 0.0f;
    gainReductionDb = 0.0f;
  }

  // inputGainDb: drive before limiting, ceilingDb: maximum output peak, releaseMs: recovery time
  void process(juce::AudioBuffer<float>& buffer, float inputGainDb, float ceilingDb, float releaseMs)
  {
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numChannels == 0 || numSamples == 0)
      return;

    const float drive = juce::Decibels::decibelsToGain(inputGainDb);
    const float ceiling = juce::Decibels::decibelsToGain(ceilingDb);
    const float releaseCoeff =
        std::exp(-1.0f / (juce::jmax(0.1f, releaseMs) * 0.001f * (float)sampleRate));

    auto* const* data = buffer.getArrayOfWritePointers();
    float minGain = 1.0f;

    for (int i = 0; i < numSamples; ++i) {
      // linked peak across channels; drop NaN/Inf so they can't poison the envelope
      float peak = 0.0f;
      for (int c = 0; c < numChannels; ++c) {
        float x = data[c][i];
        if (!std::isfinite(x)) {
          x = 0.0f;
          data[c][i] = 0.0f;
        }
        peak = juce::jmax(peak, std::abs(x * drive));
      }

      envelope = peak > envelope ? peak : peak + releaseCoeff * (envelope - peak);

      const float gain = envelope > ceiling ? ceiling / envelope : 1.0f;
      minGain = juce::jmin(minGain, gain);

      for (int c = 0; c < numChannels; ++c)
        data[c][i] = juce::jlimit(-ceiling, ceiling, data[c][i] * drive * gain);
    }

    gainReductionDb = juce::Decibels::gainToDecibels(minGain);
  }

  // most recent block's maximum gain reduction (<= 0 dB), for metering
  float getGainReductionDb() const { return gainReductionDb.load(); }

private:
  double sampleRate = 44100.0;
  float envelope = 0.0f;
  std::atomic<float> gainReductionDb{0.0f}; // written by the audio thread, read by the UI
};
