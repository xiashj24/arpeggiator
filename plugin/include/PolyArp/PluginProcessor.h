#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PolyArp/PolyArp.h"

namespace audio_plugin {
class AudioPluginAudioProcessor : public juce::AudioProcessor,
                                  private juce::HighResolutionTimer {
public:
  AudioPluginAudioProcessor();
  ~AudioPluginAudioProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  using AudioProcessor::processBlock;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  void setBypassed(bool shouldBypass) {
    if (shouldBypass && !bypassed) {
      polyarp.getArp().stop();
    }
    if (!shouldBypass && bypassed) {
      keyboardState.allNotesOff(0);
    }
    bypassed = shouldBypass;
  }

  bool isBypassed() const { return bypassed.load(); }

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String& newName) override;

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  void hiResTimerCallback() override final;

  Sequencer::PolyArp polyarp;

  juce::MidiKeyboardState keyboardState;  // MIDI visualizer

  juce::AudioProcessorValueTreeState parameters;
  juce::UndoManager undoManager;

private:
  juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

  std::atomic<float>* arpTypeParam = nullptr;
  std::atomic<float>* arpOctaveParam = nullptr;
  std::atomic<float>* arpGateParam = nullptr;
  std::atomic<float>* arpResolutionParam = nullptr;

  std::atomic<float>* arpBypassParam = nullptr;

  std::atomic<float>* euclidPatternParam = nullptr;
  std::atomic<float>* euclidLegatoParam = nullptr;

  juce::MidiMessageCollector arpMidiCollector;
  double lastCallbackTime;
  std::atomic<bool> bypassed;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
}  // namespace audio_plugin
