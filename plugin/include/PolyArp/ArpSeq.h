
#pragma once
#include "PolyArp/Arpeggiator.h"
#include "PolyArp/PolyTrack.h"
#include "PolyArp/KeyboardState.h"
#include "PolyArp/VoiceLimiter.h"
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
        // sequencerShouldPlay_(false),
        sequencerIsTicking_(false),
        sequencerArmed_(false),
        sequencerRecQuantized_(false),
        sequencerKeyTrigger_(false),
        seqStartTime_(0.0),
        seqPauseTime_(0.0),
        arpOn_(false),
        arpStartTime_(0.0),
        timeSinceStart_(0.0),
        hold_(false),
        arpeggiator_(1),
        sequencer_(1, voiceLimiter_, 16),
        voiceLimiter_(10),
        midiCollector_(midiCollector) {
    arpeggiator_.sendMidiMessage = [this](juce::MidiMessage msg) {
      // time translation
      // int tick = static_cast<int>(msg.getTimeStamp());
      // double real_time_stamp = arpStartTime_ + getOneTickTime() * tick;
      double real_time_stamp = GetSystemTime();
      sendMidiMessageToOuput(msg.withTimeStamp(real_time_stamp));
    };
    // MARK: seq out
    sequencer_.sendMidiMessage = [this](juce::MidiMessage msg) {
      // time translation (TODO: just use system time?)
      // int tick = static_cast<int>(msg.getTimeStamp());
      // double real_time_stamp = seqStartTime_ + getOneTickTime() * tick;
      double real_time_stamp = GetSystemTime();
      msg.setTimeStamp(real_time_stamp);

      if (msg.isNoteOn()) {
        auto& note_on = msg;
        int stolen_note = DUMMY_NOTE;
        if (voiceLimiter_.noteOn(note_on.getNoteNumber(), Priority::Sequencer,
                                 &stolen_note)) {
          if (stolen_note != DUMMY_NOTE) {
            auto note_off = juce::MidiMessage::noteOff(1, stolen_note);
            note_off.setTimeStamp(note_on.getTimeStamp());
            sendMidiMessageToArp(note_off);
            // DBG("note off stolen note: " << stolen_note);
          }
          sendMidiMessageToArp(note_on);
          // DBG("pass thru sequencer note on: " << note_on.getNoteNumber());
        } else {
          DBG("sequencer note on not triggered: " << note_on.getNoteNumber());
        }
      } else if (msg.isNoteOff()) {
        auto& note_off = msg;
        if (voiceLimiter_.noteOff(note_off.getNoteNumber(),
                                  Priority::Sequencer)) {
          sendMidiMessageToArp(note_off);
        } else {
          DBG("sequencer note off not triggered (stolen): "
              << note_off.getNoteNumber());
        }
      }

      DBG("Number Of active notes: " << voiceLimiter_.getNumActiveVoices());
    };

    // MARK: arp seq sync
    // sequencer_.onStep = [this](int) {
    //   if (arpOn_) {
    //     startArpeggiator();
    //   }
    // };

    // arpeggiator_.onStep = [this](int) {
    //   if (!sequencerIsTicking_ && sequencerShouldPlay_) {
    //     sequencerIsTicking_ = true;
    //   }
    // };
  }

  enum class KeytriggerMode { LastKey, Transpose, FirstKey };
  void setKeytriggerMode(KeytriggerMode mode) { keytriggerMode_ = mode; }
  // KeytriggerMode getKeyTriggerMode() const { return keytriggerMode_; }

  void setBpm(double BPM) { bpm_ = BPM; }
  double getBpm() const { return bpm_; }

  // sequencer
  // not effect if reset is false and seq is already running
  void startSequencer(bool reset) {
    if (!reset && sequencerIsTicking_) {  // sequencerShouldPlay_
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

    // sequencerShouldPlay_ = true;
    sequencerIsTicking_ = true;
    timeSinceStart_ = 0.0;

    // start ticking instantly if arp muted
    // if (arpeggiator_.isMuted()) {
    //   sequencerIsTicking_ = true;
    // }
  }

  void stopSequencer() {
    // sequencerShouldPlay_ = false;
    sequencerIsTicking_ = false;  // stop ticking immediately
    sequencer_.sendNoteOffNow();
    seqPauseTime_ = GetSystemTime();
    // sequencer_.moveToGrid();  // to avoid seq and arp out of sync
  }

  void setSequencerPlay(bool shouldPlay) {
    if (shouldPlay) {
      startSequencer(true);
    } else {
      startSequencer(true);
      stopSequencer();
    }

    // if (seqStartTime_ == 0.0) {
    //   startSequencer(true);
    // } else if (sequencerIsTicking_) {
    //   stopSequencer();
    // } else {
    //   startSequencer(false);
    // }
  }

  void setSequencerRest(bool enabled) { sequencer_.setRest(enabled); }

  void setSequencerArmed(bool enabled) {
    sequencerArmed_ = enabled;
    sequencer_.setOverdub(enabled);
  }
  // bool isSequencerArmed() const { return sequencerArmed_; }
  void setQuantizeRec(bool enabled) { sequencerRecQuantized_ = enabled; }

  void handleNoteOn(juce::MidiMessage noteOn) {
    // book keeping
    keyboard_.handleNoteOn(noteOn);
    noteToStepIndex_[noteOn.getNoteNumber()] = sequencer_.getCurrentStepIndex();

    bool note_muted = false;

    // MARK: note on
    if (sequencerKeyTrigger_) {
      if (keytriggerMode_ == KeytriggerMode::LastKey) {
        updateTransposeInterval();
        startSequencer(true);
        note_muted = true;
      } else if (keytriggerMode_ == KeytriggerMode::Transpose) {
        if (keyboard_.getNumNotesPressed() == 1) {  // first note on
          startSequencer(true);
        }
        updateTransposeInterval();
        note_muted = true;
      } else {                                      // LastKey
        if (keyboard_.getNumNotesPressed() == 1) {  // first note on
          startSequencer(true);
          note_muted = true;
          updateTransposeInterval();
        }
      }
    }

    // if (arpOn_) {
    //   note_muted = true;
    // }

    if (!note_muted) {
      // check max note limit
      int stolen_note = DUMMY_NOTE;

      if (voiceLimiter_.noteOn(noteOn.getNoteNumber(), Priority::Keyboard,
                               &stolen_note)) {
        if (stolen_note != DUMMY_NOTE) {
          auto note_off = juce::MidiMessage::noteOff(1, stolen_note);
          note_off.setTimeStamp(noteOn.getTimeStamp());
          sendMidiMessageToArp(note_off);
          DBG("note off stolen note: " << stolen_note);
        }
        sendMidiMessageToArp(noteOn);
        DBG("pass thru keyboard note on: " << noteOn.getNoteNumber());
      } else {
        DBG("no available voices: " << noteOn.getNoteNumber());
      }

      DBG("Number Of active notes: " << voiceLimiter_.getNumActiveVoices());
    }
  }

  std::function<void(int step_index, PolyStep<POLYPHONY> step)>
      notifyProcessorSeqUpdate;

  // automatically stopped when all notes are off
  void handleNoteOff(juce::MidiMessage noteOff, bool recordingOn = true) {
    if (hold_) {
      return;
    }

    // book keeping
    bool is_first_note =
        (keyboard_.getFirstNote().number == noteOff.getNoteNumber());
    bool is_last_note =
        (keyboard_.getLatestNote().number == noteOff.getNoteNumber());
    auto matched_note_on = keyboard_.handleNoteOff(noteOff);

    bool note_muted = false;

    // MARK: note off
    if (sequencerKeyTrigger_) {
      if (keytriggerMode_ == KeytriggerMode::FirstKey) {
        if (is_first_note) {
          stopSequencer();
          note_muted = true;
        }
      } else if (keytriggerMode_ == KeytriggerMode::LastKey) {
        updateTransposeInterval();
        if (is_last_note) {
          stopSequencer();
          note_muted = true;
        }
      } else {  // transpose
        updateTransposeInterval();
        if (keyboard_.isEmpty()) {  // all notes off
          stopSequencer();
          note_muted = true;
        }
      }
    }

    // MARK: realtime rec
    if (recordingOn && sequencerArmed_ && sequencerIsTicking_) {
      auto new_note = calculateNoteFromNoteOnAndOff(matched_note_on, noteOff);
      int step_index = noteToStepIndex_[noteOff.getNoteNumber()];

      auto step = sequencer_.getStepAtIndex(step_index);
      step.addNote(new_note, static_cast<int>(voiceLimiter_.getNumVoices()));
      sequencer_.setStepAtIndex(step_index, step);

      // notify AudioProcessor of parameter change
      notifyProcessorSeqUpdate(step_index, step);
    }

    // if (arpOn_) {
    //   note_muted = true;
    // }

    if (!note_muted) {
      if (voiceLimiter_.noteOff(noteOff.getNoteNumber(), Priority::Keyboard)) {
        sendMidiMessageToArp(noteOff);
        DBG("pass thru keyboard note off: " << noteOff.getNoteNumber());

      } else {
        DBG("note was not triggered or stolen: " << noteOff.getNoteNumber());
      }

      DBG("Number Of active notes: " << voiceLimiter_.getNumActiveVoices());
    }
  }

  void setArp(bool enabled) {
    arpOn_ = enabled;

    // TODO: disable note limit when arp enabled

    // transform held notes into arpeggio on rising edge
    if (arpOn_) {
      sendAllNotesOffToOutput();
      startArpeggiator();
    } else {
      stopArpeggiator();
    }
  }

  void setHold(bool enabled) {
    hold_ = enabled;  // caveat: do not place this before allNotesOff

    if (!hold_) {
      allNotesOff();

      // if (!arpOn_) {
      //   sendNoteOffs();
      // }
      // arpeggiator_.stop();

      // if (sequencerKeyTrigger_) {
      //   stopSequencer();
      // }
    }
  }

  void setKeyTrigger(bool enabled) {
    sequencerKeyTrigger_ = enabled;

    if (!sequencerKeyTrigger_) {
      stopSequencer();
      sequencer_.setTransposeInterval(0);
    }
  }

  auto& getArp() { return arpeggiator_; }
  auto& getSeq() { return sequencer_; }
  auto& getVoiceLimiter() { return voiceLimiter_; }

  void setSwing(double amount) { swing_ = amount; }

  // deltaTime is in seconds, call this frequently, preferably over 1kHz
  void process(double deltaTime) {
    timeSinceStart_ += deltaTime;
    double one_tick_time_with_swing = getOneTickTimeWithSwing();

    if (timeSinceStart_ >= one_tick_time_with_swing) {
      // worry: if seq is stopped when arp is running, they might be out of sync
      if (sequencerIsTicking_) {
        int current_index = sequencer_.getCurrentStepIndex();

        // MARK: rest
        // if (sequencerRest_) {
        //   if (sequencerArmed_) {
        //     sequencer_.resetStepAtIndex(current_index);
        //   }
        // }

        sequencer_.tick();  // overdub happens inside

        // in case overdub changes a step
        if (sequencerArmed_) {
          notifyProcessorSeqUpdate(current_index,
                                   sequencer_.getStepAtIndex(current_index));
        }
      }

      arpeggiator_.tick();  // warning: do not tick arp before seq

      // substraction is fine, but modulo feels safer
      timeSinceStart_ = std::fmod(timeSinceStart_, one_tick_time_with_swing);
    }
  }

private:
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
    return sequencer_.isOnOddStep();
  }

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

  // note: this function has no effect if no note is being pressed
  // or arp already started
  void startArpeggiator() {
    if (arpeggiator_.isMuted()) {
      arpStartTime_ = GetSystemTime();
      // timeSinceStart_ = 0.0;
      arpeggiator_.start();
    }
  }

  void stopArpeggiator() { arpeggiator_.stop(true); }

  // MARK: arp logic
  void sendMidiMessageToArp(juce::MidiMessage message) {
    if (message.isNoteOn()) {
      arpeggiator_.handleNoteOn(message);

      // start arp instantly if seq not running

      if (arpOn_) {  //  && !sequencerIsTicking_
        startArpeggiator();
      }

    } else if (message.isNoteOff()) {
      arpeggiator_.handleNoteOff(message);
    }
    if (arpeggiator_.isMuted()) {  // !arpOn_
      // if arp is not running, send thru note on and off
      // need to change for deferred start arp
      sendMidiMessageToOuput(message);
    }
  }

  void sendMidiMessageToOuput(juce::MidiMessage message) {
    message.setChannel(1);  // force channel 1
    midiCollector_.addMessageToQueue(message);
  }

  void sendAllNotesOffToOutput() {
    double now = GetSystemTime();
    auto active_notes = voiceLimiter_.getActiveNotes();
    for (int note : active_notes) {
      auto note_off = juce::MidiMessage::noteOff(1, note);
      note_off.setTimeStamp(now);
      sendMidiMessageToOuput(note_off);
    }
  }

  // from note limiter
  void allNotesOff() {
    double now = GetSystemTime();
    auto active_notes = keyboard_.getNoteStack();
    // auto active_notes = voiceLimiter_.getActiveNotes();

    for (int note : active_notes) {
      auto note_off = juce::MidiMessage::noteOff(1, note);
      note_off.setTimeStamp(now);
      handleNoteOff(note_off, false);
    }
    // keyboard_.reset();
  }

  // MARK: private vars
  double bpm_;
  double swing_;  // -0.75..0.75, move weak beats earlier/later

  // sequencer states
  // bool sequencerShouldPlay_;
  bool sequencerIsTicking_;
  bool sequencerArmed_;
  bool sequencerRecQuantized_;
  bool sequencerKeyTrigger_;
  KeytriggerMode keytriggerMode_;
  double seqStartTime_;
  double seqPauseTime_;

  // arpeggiator states
  bool arpOn_;
  double arpStartTime_;

  // for both arp and seq
  double timeSinceStart_;
  bool hold_;

  Arpeggiator arpeggiator_;
  PolyTrack<POLYPHONY> sequencer_;
  VoiceLimiter voiceLimiter_;

  // real-time recording
  KeyboardState keyboard_;
  int noteToStepIndex_[128];

  juce::MidiMessageCollector& midiCollector_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArpSeq)
};

}  // namespace Sequencer
