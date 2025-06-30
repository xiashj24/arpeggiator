
#pragma once
#include "PolyArp/Arpeggiator.h"
#include "PolyArp/PolyTrack.h"
#include "PolyArp/KeyboardState.h"
#include <juce_audio_devices/juce_audio_devices.h>  // juce::MidiMessageCollector

#define BPM_DEFAULT 120
#define BPM_MAX 240
#define BPM_MIN 30

#define TICKS_PER_16TH 24  // 96 ppqn, TODO: constexpr
#define SWING_MAX 0.75

#define POLYPHONY 10

namespace Sequencer {

// this classes is responsible for time translation and sending midi messages to
// upper level

// TODO: make sure all private variables properly initialized

static double GetSystemTime() {
  return juce::Time::getMillisecondCounterHiRes() * 0.001;
}

class ArpSeq {
public:
  ArpSeq(juce::MidiMessageCollector& midiCollector)
      : bpm_(BPM_DEFAULT),
        swing_(0.0),
        sequencerShouldPlay_(false),
        sequencerIsTicking_(false),
        sequencerArmed_(false),
        sequencerRecQuantized_(false),
        sequencerKeyTrigger_(false),
        seqStartTime_(0.0),
        seqPauseTime_(0.0),
        arpEnabled_(false),
        arpStartTime_(0.0),
        timeSinceStart_(0.0),
        hold_(false),
        arpeggiator_(1),
        sequencer_(1, keyboard_, 16),
        midiCollector_(midiCollector) {
    arpeggiator_.sendMidiMessage = [this](juce::MidiMessage msg) {
      // time translation
      int tick = static_cast<int>(msg.getTimeStamp());
      double real_time_stamp = arpStartTime_ + getOneTickTime() * tick;
      sendMidiMessageThru(msg.withTimeStamp(real_time_stamp));
    };
    sequencer_.sendMidiMessage = [this](juce::MidiMessage msg) {
      // time translation
      int tick = static_cast<int>(msg.getTimeStamp());
      double real_time_stamp = seqStartTime_ + getOneTickTime() * tick;
      sendMidiMessageThru(msg.withTimeStamp(real_time_stamp));
      // TODO: route to arpeggiator
    };

    // sequencer_.onStep = [this](int) {
    //   if (arpEnabled_) {
    //     startArpeggiator();
    //   }
    // };

    // arpeggiator_.onStep = [this](int) {
    //   if (!sequencerIsTicking_ && sequencerShouldPlay_) {
    //     sequencerIsTicking_ = true;
    //   }
    // };
  }
  // ~ArpSeq() = default;

  enum class KeytriggerMode { LastKey, Transpose, FirstKey };
  void setKeytriggerMode(KeytriggerMode mode) { keytriggerMode_ = mode; }
  // KeytriggerMode getKeyTriggerMode() const { return keytriggerMode_; }

  void setBpm(double BPM) { bpm_ = BPM; }
  double getBpm() const { return bpm_; }

  // sequencer
  // not effect if reset is false and seq is already running
  void startSequencer(bool reset) {
    if (!reset && sequencerShouldPlay_) {
      return;
    }

    if (reset) {
      sequencer_.sendNoteOffNow();
      sequencer_.reset();
      seqStartTime_ = GetSystemTime();
    } else {
      // compensate for pause time
      seqStartTime_ += (GetSystemTime() - seqPauseTime_);
    }

    sequencerShouldPlay_ = true;

    // start ticking instantly if arp muted
    if (arpeggiator_.isMuted()) {
      sequencerIsTicking_ = true;
      timeSinceStart_ = 0.0;  // necessary?
    }
  }

  void stopSequencer() {
    sequencerShouldPlay_ = false;
    sequencerIsTicking_ = false;  // stop ticking immediately
    sequencer_.sendNoteOffNow();
    seqPauseTime_ = GetSystemTime();
    // sequencer_.moveToGrid();  // to avoid seq and arp out of sync
  }

  void toggleSequencerPlayback() {
    if (seqStartTime_ == 0.0) {
      startSequencer(true);
    } else if (sequencerIsTicking_) {
      stopSequencer();
    } else {
      startSequencer(false);
    }
  }

  void setSequencerArmed(bool enabled) { sequencerArmed_ = enabled; }
  // bool isSequencerArmed() const { return sequencerArmed_; }
  void setQuantizeRec(bool enabled) { sequencerRecQuantized_ = enabled; }

  void handleNoteOn(juce::MidiMessage noteOn) {
    // book keeping
    keyboard_.handleNoteOn(noteOn);
    arpeggiator_.handleNoteOn(noteOn);
    noteToStepIndex_[noteOn.getNoteNumber()] = sequencer_.getCurrentStepIndex();

    bool note_muted = false;

    if (keyboard_.getNumNotesPressed() == 1) {  // MARK: first note on
      if (sequencerKeyTrigger_ && keytriggerMode_ != KeytriggerMode::LastKey) {
        updateTransposeInterval();
        startSequencer(true);
        note_muted = true;
      }
    }

    // MARK: note on
    if (sequencerKeyTrigger_ && keytriggerMode_ == KeytriggerMode::LastKey) {
      updateTransposeInterval();
      startSequencer(true);
      note_muted = true;
    }

    if (sequencerKeyTrigger_ && keytriggerMode_ == KeytriggerMode::Transpose) {
      updateTransposeInterval();
      note_muted = true;
    }

    // if sequencer is not running, start arp immediately if not already
    // if (!sequencerIsTicking_) {
    //   if (arpEnabled_) {
    //     startArpeggiator();
    //   }

    // }

    if (arpEnabled_) {
      note_muted = true;
    }

    if (!note_muted) {
      sendMidiMessageThru(noteOn);
    }
  }

  std::function<void(int step_index, PolyStep<POLYPHONY> step)>
      notifyProcessorSeqUpdate;

  // automatically stopped when all notes are off
  void handleNoteOff(juce::MidiMessage noteOff) {
    if (hold_) {
      return;
    }

    // book keeping
    bool is_first_note =
        (keyboard_.getEarliestNote().number == noteOff.getNoteNumber());
    auto matching_note_on = keyboard_.handleNoteOff(noteOff);
    arpeggiator_.handleNoteOff(noteOff);

    bool note_muted = false;

    if (keyboard_.isEmpty()) {  // MARK: all notes off
      if (sequencerKeyTrigger_) {
        stopSequencer();
        // note_muted = true;
      }
    }

    // MARK: note off
    if (sequencerKeyTrigger_) {
      if (keytriggerMode_ != KeytriggerMode::FirstKey) {
        updateTransposeInterval();
      } else {
        if (is_first_note) {
          stopSequencer();
        }
      }
    }

    // realtime rec
    if (sequencerArmed_ && sequencerIsTicking_) {
      auto new_note = calculateNoteFromNoteOnAndOff(matching_note_on, noteOff);
      int step_index = noteToStepIndex_[noteOff.getNoteNumber()];

      auto step = sequencer_.getStepAtIndex(step_index);
      step.addNote(new_note);
      sequencer_.setStepAtIndex(step_index, step);

      // notify AudioProcessor of parameter change
      notifyProcessorSeqUpdate(step_index, step);
    }

    if (arpEnabled_) {
      note_muted = true;
    }

    if (!note_muted) {
      sendMidiMessageThru(noteOff);
    }
  }

  void setArp(bool enabled) {
    arpEnabled_ = enabled;

    // if (arpEnabled_) {
    //   // transform held notes into arpeggio
    //   sendNoteOffs();
    //   // handleAllNotesOff();
    //   startArpeggiator();
    // } else {
    //   arpeggiator_.stop();
    // }
  }

  void setHold(bool enabled) {
    hold_ = enabled;

    if (!hold_) {
      // if (!arpEnabled_) {
      //   sendNoteOffs();
      // }
      // arpeggiator_.stop();

      // if (sequencerKeyTrigger_) {
      //   stopSequencer();
      // }

      handleAllNotesOff();
    }
  }

  void setKeyTrigger(bool enabled) {
    sequencerKeyTrigger_ = enabled;

    if (!sequencerKeyTrigger_) {
      stopSequencer();
      sequencer_.setTransposeInterval(0);
    }
  }

  Arpeggiator& getArp() { return arpeggiator_; }
  auto& getSeq() { return sequencer_; }

  void setSwing(double amount) { swing_ = amount; }

  // deltaTime is in seconds, call this frequently, preferably over 1kHz
  void process(double deltaTime) {
    timeSinceStart_ += deltaTime;
    double one_tick_time_with_swing = getOneTickTimeWithSwing();

    if (timeSinceStart_ >= one_tick_time_with_swing) {
      arpeggiator_.tick();

      // worry: if seq is stopped when arp is running, they might be out of sync
      if (sequencerIsTicking_) {
        sequencer_.tick();
      }

      // substraction is fine, but modulo feels safer
      timeSinceStart_ = std::fmod(timeSinceStart_, one_tick_time_with_swing);
    }
  }

private:
  double bpm_;
  double getOneTickTime() const { return 15.0 / bpm_ / TICKS_PER_16TH; }

  double getOneTickTimeWithSwing() const {
    if (isOnStrongBeat()) {
      return getOneTickTime() * (1 + swing_);
    } else {
      return getOneTickTime() * (1 - swing_);
    }
  }

  // 0.0 (no swing) .. 1.0 (maximum swing)
  bool isOnStrongBeat() const {
    // TODO: rework this function to take into consideration both arp and seq
    return sequencer_.isOnOddStep() % 2;
  }

  // function-related variables
  // note: can also use per track swing amount
  double swing_;  // -0.75..0.75, move weak beats earlier/later

  // sequencer states
  bool sequencerShouldPlay_;
  bool sequencerIsTicking_;
  bool sequencerArmed_;
  bool sequencerRecQuantized_;
  bool sequencerKeyTrigger_;
  KeytriggerMode keytriggerMode_;
  double seqStartTime_;
  double seqPauseTime_;

  // warning: be careful when you call this function!
  // must be called after keyboard book-keeping and startSequencer
  void updateTransposeInterval() {
    if (keyboard_.isEmpty()) {
      sequencer_.setTransposeInterval(0);
    } else {
      switch (keytriggerMode_) {
        case KeytriggerMode::LastKey:
          [[fallthrough]];

        case KeytriggerMode::Transpose:
          if (!keyboard_.isEmpty()) {
            sequencer_.setTransposeInterval(keyboard_.getLatestNote().number -
                                            sequencer_.getRootNoteNumber());
          }
          break;

        case KeytriggerMode::FirstKey:
          if (!keyboard_.isEmpty()) {
            sequencer_.setTransposeInterval(keyboard_.getEarliestNote().number -
                                            sequencer_.getRootNoteNumber());
          }
          break;
      }
    }
  }

  // arpeggiator states
  bool arpEnabled_;
  double arpStartTime_;

  // for both arp and seq
  double timeSinceStart_;
  bool hold_;

  Arpeggiator arpeggiator_;
  PolyTrack<POLYPHONY> sequencer_;

  // real time recording
  KeyboardState keyboard_;
  int noteToStepIndex_[128];

  Note calculateNoteFromNoteOnAndOff(juce::MidiMessage noteOn,
                                     juce::MidiMessage noteOff) {
#ifdef JUCE_DEBUG
    jassert(noteOn.getNoteNumber() == noteOff.getNoteNumber());
    // jassert(noteOn.getChannel() == noteOff.getChannel());
#endif
    int note_number = noteOn.getNoteNumber();
    int velocity = noteOn.getVelocity();
    // int channel = noteOn.getChannel();

    double one_step_time = sequencer_.getTicksPerStep() * getOneTickTime();

    double offset = 0.0;
    if (!sequencerRecQuantized_) {
      double steps_since_start =
          (noteOn.getTimeStamp() - seqStartTime_) / one_step_time;
      offset = steps_since_start -
               std::round(steps_since_start);  // wrap in [-0.5, 0.5)
    }

    auto length =
        (noteOff.getTimeStamp() - noteOn.getTimeStamp()) / one_step_time;
    length = std::min(
        length,
        static_cast<double>(sequencer_.getLength()));  // clip to track length

    return {.number = note_number,
            .velocity = velocity,
            .offset = static_cast<float>(offset),
            .length = static_cast<float>(length)};
  }

  juce::MidiMessageCollector& midiCollector_;

  // note: this function has no effect if no note is being pressed
  // void startArpeggiator() {
  //   if (!arpeggiator_.isEnabled()) {
  //     arpStartTime_ = GetSystemTime();
  //     // timeSinceStart_ = 0.0;
  //     arpeggiator_.start();
  //   }
  // }

  void sendMidiMessageThru(juce::MidiMessage message) {
    message.setChannel(1);  // force channel 1
    midiCollector_.addMessageToQueue(message);
  }

  void sendNoteOffs() {
    double now = GetSystemTime();
    const auto& note_stack = keyboard_.getNoteStack();
    for (int note_number : note_stack) {
      auto note_off = juce::MidiMessage::noteOff(1, note_number);
      note_off.setTimeStamp(now);
      sendMidiMessageThru(note_off);
    }
  }

  void handleAllNotesOff() {
    double now = GetSystemTime();
    auto note_stack = keyboard_.getNoteStack();
    for (int note_number : note_stack) {
      auto note_off = juce::MidiMessage::noteOff(1, note_number);
      note_off.setTimeStamp(now);
      handleNoteOff(note_off);
    }
    // keyboard_.reset();
  }

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArpSeq)
};

}  // namespace Sequencer
