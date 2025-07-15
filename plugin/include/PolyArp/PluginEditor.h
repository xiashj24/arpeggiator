#pragma once

#include "PluginProcessor.h"
#include "PolyArp/TrackComponent.h"

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

  // arp
  juce::Label typeLabel;
  juce::Label octaveLabel;
  juce::Label gateLabel;
  juce::Label resolutionLabel;
  juce::Label transposeLabel;
  juce::Label euclidPatternLabel;
  juce::Label euclidLegatoLabel;
  juce::Label arpVelocityLabel;

  juce::Slider typeKnob;
  juce::Slider octaveKnob;
  juce::Slider gateKnob;
  juce::Slider resolutionKnob;
  juce::Slider transposeKnob;
  juce::Slider euclidPatternKnob;
  juce::ToggleButton euclidLagatoButton;

  // seq
  juce::Viewport sequencerViewport;
  PolyTrackComponent sequencerComponent;

  juce::Label seqLengthLabel;
  juce::Slider seqLengthKnob;

  // utility bar
  juce::TextButton playButton;
  juce::TextButton restButton;
  juce::TextButton recordButton;
  juce::TextButton quantizeButton;
  juce::TextButton keytriggerButton;
  juce::ComboBox keytriggerModeSelector;
  juce::ComboBox arpVelocityModeSelector;

  juce::TextButton arpButton;
  juce::TextButton holdButton;

  juce::Label bpmLabel;
  juce::Slider bpmSlider;

  juce::Label swingLabel;
  juce::Slider swingSlider;

  juce::Label polyphonyLabel;
  juce::Slider polyphonySlider;

  using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
  using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

  std::unique_ptr<SliderAttachment> typeAttachment, octaveAttachment,
      gateAttachment, transposeAttachment, resolutionAttachment,
      euclidPatternAttachment, seqLengthAttachment;

  std::unique_ptr<ButtonAttachment> bypassAttachment, euclidLegatoAttachment;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
}  // namespace audio_plugin
