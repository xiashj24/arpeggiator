#pragma once
#include "PolyArp/PluginProcessor.h"

namespace audio_plugin {

#define BUTTON_WIDTH 60
#define BUTTON_HEIGHT 30
#define KNOB_SPACING 30

#define KNOB_HEIGHT 90
#define KNOB_WIDTH 70
#define KNOB_TEXT_HEIGHT 20
#define BUTTON_SPACING 10

// MARK: poly track
class PolyTrackComponent : public juce::Component, private juce::Timer {
public:
  PolyTrackComponent(AudioPluginAudioProcessor& p)
      : processorRef(p), trackRef(p.arpseq.getSeq()) {
    startTimer(10);

    setCollapsed(true);
    addAndMakeVisible(trackCollapseButton);
    trackCollapseButton.onClick = [this] {
      toggleCollapsed();
      if (auto* parent = findParentComponentOfClass<juce::Component>()) {
        parent->resized();
      }
    };

    // step buttons
    for (int i = 0; i < STEP_SEQ_MAX_LENGTH; ++i) {
      stepButtons[i].setButtonText(juce::String(i + 1));
      stepButtons[i].setClickingTogglesState(true);
      stepButtons[i].setColour(juce::TextButton::ColourIds::buttonOnColourId,
                               juce::Colours::orangered);

      stepButtons[i].onStateChange = [this, i] {
        bool visible = stepButtons[i].getToggleState();

        for (int j = 0; j < POLYPHONY; ++j) {
          noteKnobs[i][j].setVisible(visible);
        }

        velocityKnobs[i].setVisible(visible);
        offsetKnobs[i].setVisible(visible);
        lengthKnobs[i].setVisible(visible);
      };

      addAndMakeVisible(stepButtons[i]);
    }

    // note
    for (int j = 0; j < POLYPHONY; ++j) {
      noteLabel[j].setText("note " + juce::String(j + 1),
                           juce::NotificationType::dontSendNotification);
      addAndMakeVisible(noteLabel[j]);

      for (int i = 0; i < STEP_SEQ_MAX_LENGTH; ++i) {
        noteKnobs[i][j].setSliderStyle(juce::Slider::RotaryVerticalDrag);
        noteKnobs[i][j].setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                                        BUTTON_WIDTH, KNOB_TEXT_HEIGHT);
        addChildComponent(noteKnobs[i][j]);
      }
    }

    // velocity
    velocityLabel.setText("velocity",
                          juce::NotificationType::dontSendNotification);
    addAndMakeVisible(velocityLabel);

    for (int i = 0; i < STEP_SEQ_MAX_LENGTH; ++i) {
      velocityKnobs[i].setSliderStyle(juce::Slider::LinearVertical);
      velocityKnobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                                       BUTTON_WIDTH, KNOB_TEXT_HEIGHT);
      // set velocity of all notes inside the step
      velocityKnobs[i].onValueChange = [this, i]() {
        juce::String prefix = "S" + juce::String(i) + "_";

        for (int j = 1; j < POLYPHONY; ++j) {
          juce::String note_signifier = "N" + juce::String(j) + "_";
          auto other_velocity = processorRef.parameters.getParameter(
              prefix + note_signifier + "VELOCITY");
          other_velocity->setValueNotifyingHost(other_velocity->convertTo0to1(
              static_cast<float>(velocityKnobs[i].getValue())));
        }
      };
      addChildComponent(velocityKnobs[i]);
    }

    // offset
    offsetLabel.setText("offset", juce::NotificationType::dontSendNotification);
    addAndMakeVisible(offsetLabel);

    for (int i = 0; i < STEP_SEQ_MAX_LENGTH; ++i) {
      offsetKnobs[i].setSliderStyle(juce::Slider::LinearHorizontal);
      offsetKnobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                                     BUTTON_WIDTH, KNOB_TEXT_HEIGHT);
      offsetKnobs[i].onValueChange = [this, i]() {
        juce::String prefix = "S" + juce::String(i) + "_";

        for (int j = 1; j < POLYPHONY; ++j) {
          juce::String note_signifier = "N" + juce::String(j) + "_";
          auto other_offset = processorRef.parameters.getParameter(
              prefix + note_signifier + "OFFSET");
          other_offset->setValueNotifyingHost(other_offset->convertTo0to1(
              static_cast<float>(offsetKnobs[i].getValue())));
        }
      };

      addChildComponent(offsetKnobs[i]);
    }

    // length
    lengthLabel.setText("length", juce::NotificationType::dontSendNotification);
    addAndMakeVisible(lengthLabel);

    for (int i = 0; i < STEP_SEQ_MAX_LENGTH; ++i) {
      lengthKnobs[i].setSliderStyle(juce::Slider::LinearHorizontal);
      lengthKnobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                                     BUTTON_WIDTH, KNOB_TEXT_HEIGHT);

      lengthKnobs[i].onValueChange = [this, i]() {
        juce::String prefix = "S" + juce::String(i) + "_";

        for (int j = 1; j < POLYPHONY; ++j) {
          juce::String note_signifier = "N" + juce::String(j) + "_";
          auto other_offset = processorRef.parameters.getParameter(
              prefix + note_signifier + "LENGTH");
          other_offset->setValueNotifyingHost(other_offset->convertTo0to1(
              static_cast<float>(lengthKnobs[i].getValue())));
        }
      };
      addChildComponent(lengthKnobs[i]);
    }

    // MARK: attachments
    for (int i = 0; i < STEP_SEQ_MAX_LENGTH; ++i) {
      juce::String prefix = "S" + juce::String(i) + "_";

      enableAttachments[i] = std::make_unique<ButtonAttachment>(
          processorRef.parameters, prefix + "ENABLED", stepButtons[i]);

      for (int j = 0; j < POLYPHONY; ++j) {
        juce::String note_signifier = "N" + juce::String(j) + "_";

        noteAttachments[i][j] = std::make_unique<SliderAttachment>(
            processorRef.parameters, prefix + note_signifier + "NOTE",
            noteKnobs[i][j]);
      }

      // note: bind to note 0 parameter, and use onValueChange to update other
      // note parameters
      velocityAttachments[i] = std::make_unique<SliderAttachment>(
          processorRef.parameters, prefix + "N0_VELOCITY", velocityKnobs[i]);

      offsetAttachments[i] = std::make_unique<SliderAttachment>(
          processorRef.parameters, prefix + "N0_OFFSET", offsetKnobs[i]);

      lengthAttachments[i] = std::make_unique<SliderAttachment>(
          processorRef.parameters, prefix + "N0_LENGTH", lengthKnobs[i]);
    }
  }

  void timerCallback() override final {
    int playhead_index = trackRef.getCurrentStepIndex();

    for (int i = 0; i < STEP_SEQ_MAX_LENGTH; ++i) {
      if (i == playhead_index) {
        stepButtons[i].setAlpha(1.f);
      } else {
        stepButtons[i].setAlpha(0.7f);
      }
    }
  }

  void resized() override final {
    // MARK: layout
    trackCollapseButton.setBounds(0, 0, BUTTON_WIDTH, BUTTON_HEIGHT);

    offsetLabel.setBounds(0, BUTTON_HEIGHT, BUTTON_WIDTH, KNOB_HEIGHT);

    lengthLabel.setBounds(0, BUTTON_HEIGHT + KNOB_HEIGHT, BUTTON_WIDTH,
                          KNOB_HEIGHT);

    velocityLabel.setBounds(0, BUTTON_HEIGHT + KNOB_HEIGHT * 2, BUTTON_WIDTH,
                            KNOB_HEIGHT);

    for (int j = 0; j < POLYPHONY; ++j) {
      noteLabel[j].setBounds(0, BUTTON_HEIGHT + KNOB_HEIGHT * (3 + j),
                             BUTTON_WIDTH, KNOB_HEIGHT);
    }

    for (int i = 0; i < STEP_SEQ_MAX_LENGTH; ++i) {
      int x = (i + 1) * (BUTTON_WIDTH + BUTTON_SPACING);

      stepButtons[i].setBounds(x, 0, BUTTON_WIDTH, BUTTON_HEIGHT);

      offsetKnobs[i].setBounds(x, BUTTON_HEIGHT, BUTTON_WIDTH, KNOB_HEIGHT);

      lengthKnobs[i].setBounds(x, BUTTON_HEIGHT + KNOB_HEIGHT * 1, BUTTON_WIDTH,
                               KNOB_HEIGHT);

      velocityKnobs[i].setBounds(x, BUTTON_HEIGHT + KNOB_HEIGHT * 2,
                                 BUTTON_WIDTH, KNOB_HEIGHT);

      for (int j = 0; j < POLYPHONY; ++j) {
        noteKnobs[i][j].setBounds(x, BUTTON_HEIGHT + KNOB_HEIGHT * (3 + j),
                                  BUTTON_WIDTH, KNOB_HEIGHT);
      }
    }
  }

  void toggleCollapsed() {
    setCollapsed(!collapsed_);
    getParentComponent()->resized();
  }

private:
  AudioPluginAudioProcessor& processorRef;
  Sequencer::Part& trackRef;
  bool collapsed_;

  void setCollapsed(bool collapsed) {
    collapsed_ = collapsed;
    if (collapsed) {
      setSize(BUTTON_WIDTH * (STEP_SEQ_MAX_LENGTH + 1) +
                  BUTTON_SPACING * STEP_SEQ_MAX_LENGTH,
              BUTTON_HEIGHT);
      trackCollapseButton.setButtonText(juce::String::fromUTF8("Sequencer ▶"));
    } else {
      setSize(BUTTON_WIDTH * (STEP_SEQ_MAX_LENGTH + 1) +
                  BUTTON_SPACING * STEP_SEQ_MAX_LENGTH,
              BUTTON_HEIGHT + KNOB_HEIGHT * 13);
      trackCollapseButton.setButtonText(juce::String::fromUTF8("Sequencer ▼"));
    }
  }

  juce::TextButton trackCollapseButton;
  juce::Label noteLabel[POLYPHONY];
  juce::Label velocityLabel;
  juce::Label offsetLabel;
  juce::Label lengthLabel;

  juce::TextButton stepButtons[STEP_SEQ_MAX_LENGTH];
  juce::Slider noteKnobs[STEP_SEQ_MAX_LENGTH][POLYPHONY];
  juce::Slider velocityKnobs[STEP_SEQ_MAX_LENGTH];
  juce::Slider offsetKnobs[STEP_SEQ_MAX_LENGTH];
  juce::Slider lengthKnobs[STEP_SEQ_MAX_LENGTH];

  using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
  using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

  // Parameter attachments
  std::unique_ptr<ButtonAttachment> enableAttachments[STEP_SEQ_MAX_LENGTH];
  std::unique_ptr<SliderAttachment> noteAttachments[STEP_SEQ_MAX_LENGTH]
                                                   [POLYPHONY],
      velocityAttachments[STEP_SEQ_MAX_LENGTH],
      offsetAttachments[STEP_SEQ_MAX_LENGTH],
      lengthAttachments[STEP_SEQ_MAX_LENGTH];
};

}  // namespace audio_plugin