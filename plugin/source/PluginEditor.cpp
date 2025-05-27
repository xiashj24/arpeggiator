#include "PolyArp/PluginEditor.h"
#include "PolyArp/PluginProcessor.h"

#define BUTTON_WIDTH 60
#define BUTTON_HEIGHT 30
#define KNOB_SPACING 30

#define KNOB_HEIGHT 90
#define KNOB_WIDTH 70
#define KNOB_TEXT_HEIGHT 20

namespace audio_plugin {
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(
    AudioPluginAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      onScreenKeyboard(p.keyboardState,
                       juce::MidiKeyboardComponent::horizontalKeyboard) {
  juce::ignoreUnused(processorRef);
  // Make sure that before the constructor has finished, you've set the
  // editor's size to whatever you need it to be.
  setSize(800, 300);
  setResizable(true, true);

  bypassButton.setButtonText("Bypass");
  bypassButton.setClickingTogglesState(true);
  bypassButton.addShortcut(juce::KeyPress('b'));
  bypassButton.setTooltip("bypass arpeggiator (b)");
  bypassButton.setColour(juce::TextButton::ColourIds::buttonOnColourId,
                         juce::Colours::orangered);
  // bypassButton.onClick = [this]() {
  //   processorRef.setBypassed(bypassButton.getToggleState());
  //   if (bypassButton.getToggleState() == true) {
  //     processorRef.polyarp.stop();
  //   } else {
  //     processorRef.keyboardState.allNotesOff(0);
  //   }
  // };
  addAndMakeVisible(bypassButton);
  bypassAttachment = std::make_unique<ButtonAttachment>(
      processorRef.parameters, "ARP_BYPASS", bypassButton);

  latchButton.setButtonText("Latch");
  latchButton.setClickingTogglesState(true);
  latchButton.addShortcut(juce::KeyPress('l'));
  latchButton.setTooltip("toggle latch on/off (l)");
  latchButton.setColour(juce::TextButton::ColourIds::buttonOnColourId,
                        juce::Colours::orangered);
  latchButton.onClick = [this]() {
    processorRef.polyarp.setLatch(latchButton.getToggleState());
  };
  addAndMakeVisible(latchButton);

  typeLabel.setText("Arp Type", juce::NotificationType::dontSendNotification);
  typeLabel.setJustificationType(juce::Justification::centredBottom);
  typeLabel.attachToComponent(&typeKnob, false);

  typeKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  typeKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, KNOB_WIDTH,
                           KNOB_TEXT_HEIGHT);
  addAndMakeVisible(typeKnob);
  typeAttachment = std::make_unique<SliderAttachment>(processorRef.parameters,
                                                      "ARP_TYPE", typeKnob);

  octaveLabel.setText("Octave", juce::NotificationType::dontSendNotification);
  octaveLabel.setJustificationType(juce::Justification::centredBottom);
  octaveLabel.attachToComponent(&octaveKnob, false);

  octaveKnob.setSliderStyle(juce::Slider::LinearVertical);
  octaveKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, KNOB_WIDTH,
                             KNOB_TEXT_HEIGHT);
  addAndMakeVisible(octaveKnob);
  octaveAttachment = std::make_unique<SliderAttachment>(
      processorRef.parameters, "ARP_OCTAVE", octaveKnob);

  gateLabel.setText("Gate", juce::NotificationType::dontSendNotification);
  gateLabel.setJustificationType(juce::Justification::centredBottom);
  gateLabel.attachToComponent(&gateKnob, false);

  gateKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  gateKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, KNOB_WIDTH,
                           KNOB_TEXT_HEIGHT);
  addAndMakeVisible(gateKnob);
  gateAttachment = std::make_unique<SliderAttachment>(processorRef.parameters,
                                                      "ARP_GATE", gateKnob);

  resolutionLabel.setText("Resolution",
                          juce::NotificationType::dontSendNotification);
  resolutionLabel.setJustificationType(juce::Justification::centredBottom);
  resolutionLabel.attachToComponent(&resolutionKnob, false);

  resolutionKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  resolutionKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, KNOB_WIDTH,
                                 KNOB_TEXT_HEIGHT);
  addAndMakeVisible(resolutionKnob);
  resolutionAttachment = std::make_unique<SliderAttachment>(
      processorRef.parameters, "ARP_RESOLUTION", resolutionKnob);

  // Gate Pattern
  euclidPatternLabel.setText("Euclid Pattern",
                             juce::NotificationType::dontSendNotification);
  euclidPatternLabel.setJustificationType(juce::Justification::centredBottom);
  euclidPatternLabel.attachToComponent(&euclidPatternKnob, false);
  euclidPatternKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  euclidPatternKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                                    KNOB_WIDTH, KNOB_TEXT_HEIGHT);
  addAndMakeVisible(euclidPatternKnob);

  euclidPatternAttachment = std::make_unique<SliderAttachment>(
      processorRef.parameters, "EUCLID_PATTERN", euclidPatternKnob);

  euclidLegatoLabel.setText("Euclid Legato",
                            juce::NotificationType::dontSendNotification);
  euclidLegatoLabel.setJustificationType(juce::Justification::centredBottom);
  euclidLegatoLabel.attachToComponent(&euclidLagatoButton, false);

  euclidLagatoButton.setClickingTogglesState(true);
  euclidLagatoButton.setColour(juce::TextButton::ColourIds::buttonOnColourId,
                               juce::Colours::orangered);
  addAndMakeVisible(euclidLagatoButton);
  euclidLegatoAttachment = std::make_unique<ButtonAttachment>(
      processorRef.parameters, "EUCLID_LEGATO", euclidLagatoButton);

  if (processorRef.wrapperType ==
      juce::AudioProcessor::WrapperType::wrapperType_Standalone) {
    bpmLabel.setText("BPM: ", juce::NotificationType::dontSendNotification);
    bpmLabel.attachToComponent(&bpmSlider, true);
    bpmSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40,
                              BUTTON_HEIGHT);
    bpmSlider.setRange(BPM_MIN, BPM_MAX, 1);
    bpmSlider.setValue(BPM_DEFAULT);
    bpmSlider.setDoubleClickReturnValue(true, BPM_DEFAULT);
    bpmSlider.onValueChange = [this] {
      processorRef.polyarp.setBpm(bpmSlider.getValue());
    };
    addAndMakeVisible(bpmSlider);
  }

  addAndMakeVisible(onScreenKeyboard);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {}

void AudioPluginAudioProcessorEditor::paint(juce::Graphics& g) {
  // (Our component is opaque, so we must completely fill the background with a
  // solid colour)
  g.fillAll(
      getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void AudioPluginAudioProcessorEditor::resized() {
  // This is generally where you'll want to lay out the positions of any
  // subcomponents in your editor..
  auto bounds = getBounds();
  onScreenKeyboard.setBounds(bounds.removeFromBottom(90));

  auto utility_bar = bounds.removeFromBottom(BUTTON_HEIGHT + 20).reduced(10);

  bypassButton.setBounds(utility_bar.removeFromLeft(BUTTON_WIDTH));
  utility_bar.removeFromLeft(10);
  latchButton.setBounds(utility_bar.removeFromLeft(BUTTON_WIDTH));
  utility_bar.removeFromLeft(50);
  bpmSlider.setBounds(utility_bar.removeFromLeft(200));

  auto knob_bar = bounds.reduced(30).removeFromTop(KNOB_HEIGHT);
  typeKnob.setBounds(knob_bar.removeFromLeft(KNOB_WIDTH));
  knob_bar.removeFromLeft(KNOB_SPACING);
  octaveKnob.setBounds(knob_bar.removeFromLeft(KNOB_WIDTH));
  knob_bar.removeFromLeft(KNOB_SPACING);
  gateKnob.setBounds(knob_bar.removeFromLeft(KNOB_WIDTH));
  knob_bar.removeFromLeft(KNOB_SPACING);
  resolutionKnob.setBounds(knob_bar.removeFromLeft(KNOB_WIDTH));
  knob_bar.removeFromLeft(KNOB_SPACING);
  euclidPatternKnob.setBounds(knob_bar.removeFromLeft(KNOB_WIDTH));
  knob_bar.removeFromLeft(KNOB_SPACING);
  euclidLagatoButton.setBounds(knob_bar.removeFromLeft(KNOB_WIDTH));
  euclidLagatoButton.setSize(KNOB_WIDTH, KNOB_WIDTH);
}
}  // namespace audio_plugin
