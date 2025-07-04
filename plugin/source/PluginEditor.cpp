#include "PolyArp/PluginEditor.h"
#include "PolyArp/PluginProcessor.h"

namespace audio_plugin {
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(
    AudioPluginAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      onScreenKeyboard(p.keyboardState,
                       juce::MidiKeyboardComponent::horizontalKeyboard),
      sequencerComponent(p) {
  juce::ignoreUnused(processorRef);

  setSize(1280, 780);
  setResizable(true, true);

  playButton.setButtonText(juce::String::fromUTF8("⏯Play"));
  playButton.setClickingTogglesState(true);
  playButton.setColour(juce::TextButton::ColourIds::buttonOnColourId,
                       juce::Colours::orangered);
  playButton.addShortcut(juce::KeyPress(juce::KeyPress::spaceKey));
  playButton.setTooltip("toggle play and pause (space)");
  playButton.onClick = [this] {
    processorRef.arpseq.setSequencerPlay(playButton.getToggleState());
  };
  addAndMakeVisible(playButton);

  restButton.setButtonText("resT");  // juce::String::fromUTF8("⏹Stop")
  restButton.setClickingTogglesState(true);
  restButton.setColour(juce::TextButton::ColourIds::buttonOnColourId,
                       juce::Colours::orangered);
  restButton.addShortcut(juce::KeyPress('t'));
  // restButton.setTooltip("stop playback and move to start position (s)");
  restButton.onClick = [this] {
    processorRef.arpseq.setSequencerRest(restButton.getToggleState());
    // processorRef.arpseq.startSequencer(true);
    // processorRef.arpseq.stopSequencer();
    // playButton.setToggleState(false,
    //                           juce::NotificationType::dontSendNotification);
  };
  addAndMakeVisible(restButton);

  recordButton.setButtonText(juce::String::fromUTF8("⏺Rec"));
  recordButton.setClickingTogglesState(true);
  recordButton.addShortcut(juce::KeyPress('r'));
  recordButton.setTooltip("toggle real-time recording (r)");
  recordButton.setColour(juce::TextButton::ColourIds::buttonOnColourId,
                         juce::Colours::orangered);
  recordButton.onClick = [this] {
    const bool should_be_recording = recordButton.getToggleState();
    processorRef.arpseq.setSequencerArmed(should_be_recording);
    if (should_be_recording) {
      keytriggerButton.setToggleState(false,
                                      juce::NotificationType::sendNotification);
    }
  };
  addAndMakeVisible(recordButton);

  quantizeButton.setButtonText("Quantize");
  quantizeButton.setClickingTogglesState(true);
  quantizeButton.setTooltip(
      "quantize note-on timing for real-time recording (q)");
  quantizeButton.addShortcut(juce::KeyPress('q'));
  quantizeButton.setColour(juce::TextButton::ColourIds::buttonOnColourId,
                           juce::Colours::orangered);
  quantizeButton.onClick = [this] {
    processorRef.arpseq.setQuantizeRec(quantizeButton.getToggleState());
  };
  addAndMakeVisible(quantizeButton);

  keytriggerButton.setButtonText("Key Trigger");
  keytriggerButton.setClickingTogglesState(true);
  keytriggerButton.setTooltip("start sequencer playback on first key on (k)");
  keytriggerButton.addShortcut(juce::KeyPress('k'));
  keytriggerButton.setColour(juce::TextButton::ColourIds::buttonOnColourId,
                             juce::Colours::orangered);

  keytriggerButton.onClick = [this] {
    bool should_keytrigger = keytriggerButton.getToggleState();
    processorRef.arpseq.setKeyTrigger(should_keytrigger);
    if (should_keytrigger) {
      recordButton.setToggleState(false, juce::sendNotification);
      playButton.setToggleState(false, juce::sendNotification);
    }
  };

  addAndMakeVisible(keytriggerButton);

  keytriggerModeSelector.addItem("Retrigger (Mono)", 1);
  keytriggerModeSelector.addItem("Transpose (Mono Legato)", 2);
  keytriggerModeSelector.addItem("First Key (Poly)", 3);
  keytriggerModeSelector.onChange = [this] {
    processorRef.arpseq.setKeytriggerMode(
        static_cast<Sequencer::ArpSeq::KeytriggerMode>(
            keytriggerModeSelector.getSelectedId() - 1));
  };
  addAndMakeVisible(keytriggerModeSelector);
  keytriggerModeSelector.setSelectedId(
      1, juce::NotificationType::sendNotification);

  holdButton.setButtonText("Hold");
  holdButton.setClickingTogglesState(true);
  holdButton.addShortcut(juce::KeyPress('h'));
  holdButton.setTooltip(
      "act as sustain pedal when arp is off, act as latch control when arp is "
      "on (h)");
  holdButton.setColour(juce::TextButton::ColourIds::buttonOnColourId,
                       juce::Colours::orangered);
  holdButton.onClick = [this]() {
    processorRef.arpseq.setHold(holdButton.getToggleState());
  };
  addAndMakeVisible(holdButton);

  arpButton.setButtonText("Arp");
  arpButton.setClickingTogglesState(true);
  arpButton.addShortcut(juce::KeyPress('a'));
  arpButton.setTooltip("Toggle arpeggiator on/off (a)");
  arpButton.setColour(juce::TextButton::ColourIds::buttonOnColourId,
                      juce::Colours::orangered);
  arpButton.onClick = [this] {
    processorRef.arpseq.setArp(arpButton.getToggleState());
  };
  addAndMakeVisible(arpButton);

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
  euclidPatternLabel.setText("Density",
                             juce::NotificationType::dontSendNotification);
  euclidPatternLabel.setJustificationType(juce::Justification::centredBottom);
  euclidPatternLabel.attachToComponent(&euclidPatternKnob, false);
  euclidPatternKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  euclidPatternKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                                    KNOB_WIDTH, KNOB_TEXT_HEIGHT);
  addAndMakeVisible(euclidPatternKnob);

  euclidPatternAttachment = std::make_unique<SliderAttachment>(
      processorRef.parameters, "EUCLID_PATTERN", euclidPatternKnob);

  euclidLegatoLabel.setText("Rest/Tie",
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
      processorRef.arpseq.setBpm(bpmSlider.getValue());
    };
    addAndMakeVisible(bpmSlider);
  }

  swingLabel.setText("Swing: ", juce::NotificationType::dontSendNotification);
  swingLabel.attachToComponent(&swingSlider, true);
  swingSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
  swingSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40,
                              BUTTON_HEIGHT);
  swingSlider.setRange(-SWING_MAX, SWING_MAX, 0.01);
  swingSlider.setValue(0.0);
  swingSlider.setDoubleClickReturnValue(true, 0.0);
  swingSlider.onValueChange = [this] {
    processorRef.arpseq.setSwing(swingSlider.getValue());
  };
  addAndMakeVisible(swingSlider);

  polyphonyLabel.setText("Voice: ",
                         juce::NotificationType::dontSendNotification);
  polyphonyLabel.attachToComponent(&polyphonySlider, true);
  polyphonySlider.setSliderStyle(juce::Slider::SliderStyle::IncDecButtons);
  polyphonySlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 30,
                                  BUTTON_HEIGHT);
  polyphonySlider.setRange(1, 10, 1);
  polyphonySlider.setValue(10);
  polyphonySlider.onValueChange = [this] {
    processorRef.arpseq.getVoiceLimiter().setNumVoices(
        static_cast<size_t>(polyphonySlider.getValue()));
  };
  addAndMakeVisible(polyphonySlider);

  addAndMakeVisible(sequencerViewport);
  sequencerViewport.setViewedComponent(&sequencerComponent, false);

  onScreenKeyboard.setWantsKeyboardFocus(false);  // disable keypress
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
  // MARK: layout
  auto bounds = getBounds();
  onScreenKeyboard.setBounds(bounds.removeFromBottom(90));

  auto utility_bar = bounds.removeFromBottom(BUTTON_HEIGHT + 20).reduced(10);

  playButton.setBounds(utility_bar.removeFromLeft(BUTTON_WIDTH));
  utility_bar.removeFromLeft(10);
  restButton.setBounds(utility_bar.removeFromLeft(BUTTON_WIDTH));
  utility_bar.removeFromLeft(10);
  recordButton.setBounds(utility_bar.removeFromLeft(BUTTON_WIDTH));
  utility_bar.removeFromLeft(10);
  quantizeButton.setBounds(utility_bar.removeFromLeft(BUTTON_WIDTH));
  utility_bar.removeFromLeft(10);
  keytriggerButton.setBounds(utility_bar.removeFromLeft(BUTTON_WIDTH));
  utility_bar.removeFromLeft(10);
  keytriggerModeSelector.setBounds(utility_bar.removeFromLeft(180));
  utility_bar.removeFromLeft(30);
  arpButton.setBounds(utility_bar.removeFromLeft(BUTTON_WIDTH));
  utility_bar.removeFromLeft(10);
  holdButton.setBounds(utility_bar.removeFromLeft(BUTTON_WIDTH));

  utility_bar.removeFromLeft(50);
  bpmSlider.setBounds(utility_bar.removeFromLeft(180));
  utility_bar.removeFromLeft(60);
  swingSlider.setBounds(utility_bar.removeFromLeft(140));
  utility_bar.removeFromLeft(10);
  polyphonySlider.setBounds(utility_bar.removeFromRight(80));

  auto knob_bar = bounds.removeFromTop(KNOB_HEIGHT + 80).reduced(30);
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

  sequencerViewport.setBounds(bounds.reduced(30));
}
}  // namespace audio_plugin
