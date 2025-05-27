#pragma once

#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>  // juce::MidiKeyboardComponent

namespace audio_plugin {

class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
  explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor&);
  ~AudioPluginAudioProcessorEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

private:
  // This reference is provided as a quick way for your editor to
  // access the processor object that created it.
  AudioPluginAudioProcessor& processorRef;

  juce::MidiKeyboardComponent onScreenKeyboard;

  juce::TextButton bypassButton;
  juce::TextButton latchButton;

  juce::Label typeLabel;
  juce::Label octaveLabel;
  juce::Label gateLabel;
  juce::Label resolutionLabel;
  juce::Label euclidPatternLabel;
  juce::Label euclidLegatoLabel;

  juce::Slider typeKnob;
  juce::Slider octaveKnob;
  juce::Slider gateKnob;
  juce::Slider resolutionKnob;

  juce::Slider euclidPatternKnob;
  juce::TextButton euclidLagatoButton;

  juce::Label bpmLabel;
  juce::Slider bpmSlider;

  using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
  using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

  std::unique_ptr<SliderAttachment> typeAttachment, octaveAttachment,
      gateAttachment, resolutionAttachment, euclidPatternAttachment;

  std::unique_ptr<ButtonAttachment> bypassAttachment, euclidLegatoAttachment;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
}  // namespace audio_plugin
