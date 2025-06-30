#pragma once
#include "PolyArp/Note.h"
#include <juce_audio_basics/juce_audio_basics.h>  // juce::MidiMessageSequence

/*
  sequencer/arpeggiator base class

  created by Shijie Xia in 2025/2
  maintained by {put your name here if you have to maintain this} in {date}

  Please allow me to rant a little about the coding culture in Korg
  that every file seems to be created and maintained by a single programmer
  and no one seems to care about writing good documentations or enforcing good
  coding styles, which leads to the bloated and cryptic codebase that Spark is
  :(
*/

// TODO: make this a static constexpr variable of TRACK(Part)
#define STEP_SEQ_MAX_LENGTH 64
#define STEP_SEQ_DEFAULT_LENGTH 16

namespace Sequencer {

class Part {
public:
  // mapped to ticks per step
  enum Resolution {
    _32th,
    _16th,
    _8th,
    _4th,
    _6th,   // 1/2T
    _12th,  // 1/4T
    _24th,  // 1/8T
    _48th,  // 1/16T
  };

  Part(int channel, int length, Resolution resolution)
      : channel_(channel),
        trackLength_(length),
        trackLengthNew_(length),
        resolution_(resolution),
        resolutionNew_(resolution),
        muted_(false),
        tick_(0) {}

  virtual ~Part() = default;

  void setMuted(bool enabled) { muted_ = enabled; }
  bool isMuted() const { return muted_; }

  // void setChannel(int channel) { channel_ = channel; }
  int getChannel() const { return channel_; }

  // if not muted, change will be effective on next loop start
  void setLength(int length) {
    trackLengthNew_ = length;
    if (isMuted()) {
      trackLength_ = length;
    }
  }

  int getLength() const { return trackLength_; }

  // if not muted, change will be effective on next loop start
  void setResolution(Resolution resolution) {
    resolutionNew_ = resolution;
    if (isMuted()) {
      resolution_ = resolution;
    }
  }

  // callback to transfer MIDI messages (timestamp in ticks)
  std::function<void(juce::MidiMessage msg)> sendMidiMessage;

  // the manager of this class (and derived classes) is responsible to call this
  // function getTicksPerStep() times per step
  void tick();

  void reset(float start_index = 0.f);

  // for GUI
  float getProgress() const {
    return static_cast<float>(tick_ + getTicksHalfStep()) /
           static_cast<float>(getTicksPerStep() * getLength());
  }

  int getCurrentStepIndex() const;

  bool isOnOddStep() const {
    if (tick_ < 0) {
      return (getLength() - 1) % 2 == 0;
    } else {
      return (tick_ / getTicksPerStep()) % 2 == 0;
    }
  }

  int getTicksPerStep() const {
    static const int RESOLUTION_TICKS_TABLE[] = {12, 24, 48, 96, 64, 32, 16, 8};
    return RESOLUTION_TICKS_TABLE[static_cast<int>(resolution_)];
  }

  void moveToGrid() { tick_ = getCurrentStepIndex() * getTicksPerStep(); }

  void sendNoteOffNow();

  // callback fired on step grid
  std::function<void(int)> onStep;

protected:
  void renderNote(int index, Note note);

  // timestamp in ticks (not seconds or samples)
  void renderMidiMessage(juce::MidiMessage message);

  int getTicksHalfStep() const { return getTicksPerStep() / 2; }

private:
  int channel_;

  // track parameters as seen by the user
  int trackLength_;
  int trackLengthNew_;
  Resolution resolution_;
  Resolution resolutionNew_;

  // int ApplySwingToTick(int tick) const;

  bool muted_;

  // function related variables
  int tick_;

  // derived class must implement renderStep and getStepRenderTick
  virtual void renderStep(int index) = 0;
  virtual int getStepRenderTick(int index) const = 0;

  // helpers
  bool isOnGrid() const { return tick_ % getTicksPerStep() == 0; }

  /*
    invariant: MIDI messages are always sorted by timestamp
    note: when porting to Spark/Prologue, change from juce::MidiMessageSequence
    to a simpler data structure (something like FIFO queue)
  */
  juce::MidiMessageSequence midiQueue_, midiQueueNext_;
};

}  // namespace Sequencer