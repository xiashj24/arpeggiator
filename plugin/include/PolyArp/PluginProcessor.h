#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PolyArp/ArpSeq.h"

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

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String& newName) override;

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  void hiResTimerCallback() override final;

  Sequencer::ArpSeq arpseq;

  juce::MidiKeyboardState keyboardState;  // MIDI visualizer

  juce::AudioProcessorValueTreeState parameters;
  juce::UndoManager undoManager;

private:
  juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

  // arp parameters
  std::atomic<float>* arpTypeParam;
  std::atomic<float>* arpOctaveParam;
  std::atomic<float>* arpGateParam;
  std::atomic<float>* arpResolutionParam;
  std::atomic<float>* euclidPatternParam;
  std::atomic<float>* euclidLegatoParam;
  std::atomic<float>* arpTransposeParam;

  // seq parameters
  std::atomic<float>* seqLengthParam;
  std::atomic<float>* seqStepEnabledParam[STEP_SEQ_MAX_LENGTH];
  std::atomic<float>* seqStepNoteParam[STEP_SEQ_MAX_LENGTH][POLYPHONY];
  std::atomic<float>* seqStepVelocityParam[STEP_SEQ_MAX_LENGTH][POLYPHONY];
  std::atomic<float>* seqStepOffsetParam[STEP_SEQ_MAX_LENGTH][POLYPHONY];
  std::atomic<float>* seqStepLengthParam[STEP_SEQ_MAX_LENGTH][POLYPHONY];

  juce::MidiMessageCollector arpMidiCollector;
  double lastCallbackTime;
  // std::atomic<bool> bypassed;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
}  // namespace audio_plugin
