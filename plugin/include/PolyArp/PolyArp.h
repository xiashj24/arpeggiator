//******************************************************************************
//	E3Sequencer.h
//	(c)2025 KORG Inc. / written by Shijie Xia
//
//	platform-agonistic polyphonic MIDI step sequencer
//******************************************************************************
// note : avoid JUCE API and cpp STL inside this class

#pragma once
#include "PolyArp/Arpeggiator.h"
#include "PolyArp/KeyboardState.h"
#include <juce_audio_devices/juce_audio_devices.h>  // juce::MidiMessageCollector

#define BPM_DEFAULT 120
#define BPM_MAX 240
#define BPM_MIN 30

#define TICKS_PER_16TH 24

namespace Sequencer {

// this classes is responsible for time translation and sending midi notes

class PolyArp {
public:
  PolyArp(juce::MidiMessageCollector& midiCollector)
      : bpm_(BPM_DEFAULT),
        timeSinceStart_(0.0),
        startTime_(0.0),
        arp_(1, keyboardState_),
        midiCollector_(midiCollector) {
    arp_.setEnabled(false);  // note: track should be disabled by default

    arp_.sendMidiMessage = [this](juce::MidiMessage msg) {
      // time translation
      int tick = static_cast<int>(msg.getTimeStamp());
      double real_time_stamp = startTime_ + getOneTickTime() * tick;
      // this time translation code is smelling....need some further thinking
      this->midiCollector_.addMessageToQueue(
          msg.withTimeStamp(real_time_stamp));
    };
  }
  ~PolyArp() = default;

  bool neverStarted() const { return startTime_ == 0.0; }

  void setBpm(double BPM) { bpm_ = BPM; }
  double getBpm() const { return bpm_; }

  void panic() {
    auto msg = juce::MidiMessage::allNotesOff(1);
    msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    midiCollector_.addMessageToQueue(msg);
  }

  void start(double startTime) {
    arp_.setEnabled(true);
    timeSinceStart_ = 0.0;
    startTime_ = startTime;
    arp_.reset();
  }

  void stop() {
    arp_.setEnabled(false);
    panic();
  }

  bool isRunning() const { return arp_.isEnabled(); }

  void handleNoteOn(juce::MidiMessage noteOn) {
    // maintain keyboard state
    keyboardState_.handleNoteOn(noteOn);

    if (!arp_.isEnabled()) {
      start(juce::Time::getMillisecondCounterHiRes() *
            0.001);  // is this the right place to call it??
    }
  }

  void handleNoteOff(juce::MidiMessage noteOff) {
    // maintain keyboard state
    keyboardState_.handleNoteOff(noteOff);

    if (keyboardState_.getNumNotesPressed() == 0) {
      stop();
    }
  }

  // deltaTime is in seconds, call this frequenctly, preferably over 1kHz
  void process(double deltaTime) {
    if (!arp_.isEnabled())
      return;

    timeSinceStart_ += deltaTime;
    double one_tick_time = getOneTickTime();

    if (timeSinceStart_ >= one_tick_time) {
      arp_.tick();
      timeSinceStart_ -= one_tick_time;
    }
  }

private:
  double bpm_;
  double getOneTickTime() const { return 15.0 / bpm_ / TICKS_PER_16TH; }

  // function-related variables
  // bool enabled_; // note: this is different from arp_.isEnabled()

  double timeSinceStart_;
  double startTime_;

  KeyboardState keyboardState_;
  Arpeggiator arp_;

  juce::MidiMessageCollector& midiCollector_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PolyArp)
};

}  // namespace Sequencer
