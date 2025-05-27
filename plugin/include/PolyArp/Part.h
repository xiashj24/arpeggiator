#pragma once
#include "PolyArp/Note.h"
#include "PolyArp/KeyboardState.h"
#include <juce_audio_basics/juce_audio_basics.h>  // juce::MidiMessageSequence

/*
  core functionality of a one track monophonic sequencer

  created by Shijie Xia in 2025/2
  maintained by {put your name here if you have to maintain this} in {date}

  Please allow me to rant a little about the coding culture in Korg
  that every file seems to be created and maintained by a single programmer
  and no one seems to care about writing good documentations or enforcing good
  coding styles, which leads to the bloated and cryptic codebase that Spark is
  :(
*/

// TODO: make this a static constexpr variable of TRACK(Part)
#define STEP_SEQ_MAX_LENGTH 16  // TODO: test as large as 128
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
        enabled_(true),
        tick_(0) {}

  virtual ~Part() = default;

  void setEnabled(bool enabled) { enabled_ = enabled; }
  bool isEnabled() const { return enabled_; }

  // [[deprecated]] void setChannel(int channel) { channel_ = channel; }
  int getChannel() const { return channel_; }

  // if enabled, change will be applied at the start of the next step
  void setLength(int length) {
    trackLengthNew_ = length;
    if (isEnabled()) {
      trackLength_ = length;
    }
  }
  int getLength() const { return trackLength_; }

  // if enabled, change will be applied at the start of the next loop
  void setResolution(Resolution resolution) {
    resolutionNew_ = resolution;

    if (!isEnabled()) {
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
    return static_cast<float>(tick_ - getTicksHalfStep()) /
           static_cast<float>(getTicksPerStep() * getLength());
  }

  // TODO: track utilities (randomize, humanize, rotate, etc.)

protected:
  void renderNote(int index, Note note);

  // timestamp in ticks (not seconds or samples)
  void renderMidiMessage(juce::MidiMessage message);

  int getTicksPerStep() const {
    static const int RESOLUTION_TICKS_TABLE[] = {12, 24, 48, 96, 64, 32, 16, 8};

    return RESOLUTION_TICKS_TABLE[static_cast<int>(resolution_)];
  }

  int getTicksHalfStep() const { return getTicksPerStep() / 2; }

  void sendNoteOffNow();

  int getCurrentStepIndex() const;

private:
  int channel_;

  // track parameters as seen by the user

  int trackLength_;
  int trackLengthNew_;
  Resolution resolution_;
  Resolution resolutionNew_;

  // TODO: implement swing (should not affect roll through)
  [[maybe_unused]] float swing_;
  [[maybe_unused]] int resetLength_;  // 0 means do not sync to resetLegnth

  bool enabled_;

  // function related variables
  int tick_;

  // derived class must implement renderStep and getStepRenderTick
  virtual void renderStep(int index) = 0;
  virtual int getStepRenderTick(int index) const = 0;

  /*
    invariant: MIDI messages are always sorted by timestamp
    note: when porting to Spark/Prologue, change from juce::MidiMessageSequence
    to a simpler data structure (FIFO queue)
  */
  juce::MidiMessageSequence midiQueue_, midiQueueNext_;
};

}  // namespace Sequencer