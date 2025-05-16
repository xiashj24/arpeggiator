#pragma once
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

// TODO: make this a static const variable of TRACK
#define STEP_SEQ_MAX_LENGTH 16  // TODO: test as large as 128
#define STEP_SEQ_DEFAULT_LENGTH 16
#define MAX_MOTION_SLOTS 8  // not used now

#define DEFAULT_NOTE 60  // C4
#define DISABLED_NOTE 20
#define DEFAULT_VELOCITY 100  // 1..127 since 0 is the same as NoteOff
#define DEFAULT_LENGTH 0.75f

namespace Sequencer {

struct Note {
  int number = DEFAULT_NOTE;  // for now we use the convention that note number
                              // <= 20 indicates disabled
  int velocity = DEFAULT_VELOCITY;
  float offset = 0.f;             // relative the step index
  float length = DEFAULT_LENGTH;  // in (0, TrackLength]

  void reset() {
    number = DISABLED_NOTE;
    velocity = DEFAULT_VELOCITY;
    offset = 0.f;  // relative the step index
    length = DEFAULT_LENGTH;
  }
};

class Track {
public:
  // mapped to ticks per step
  enum Resolution {
    _32th = 12,
    _16th = 24,  // default
    _8th = 48,
    _4th = 96,
    _48th = 8,   // 1/16T
    _24th = 16,  // 1/8T
    _12th = 32,  // 1/4T
    _6th = 64    // 1/2T
  };

  Track(int channel,
        const KeyboardState& keyboard,
        int length = STEP_SEQ_DEFAULT_LENGTH,
        Resolution resolution = _16th)
      : keyboardRef(keyboard),
        channel_(channel),
        trackLength_(length),
        trackLengthDeferred_(length),
        resolution_(resolution),
        enabled_(true),
        tick_(0) {}

  virtual ~Track() = default;

  void setEnabled(bool enabled) { enabled_ = enabled; }

  // what's the use of this method. for runtime channel swithing?
  void setChannel(int channel) { channel_ = channel; }

  void setLengthDeferred(int length) { trackLengthDeferred_ = length; }
  int getChannel() const { return channel_; }
  bool isEnabled() const { return enabled_; }
  int getLength() const { return trackLength_; }

  void setResolution(Resolution resolution) { resolution_ = resolution; }

  // caller should register a callback to receive MIDI messages
  std::function<void(juce::MidiMessage msg)> sendMidiMessage;

  // this function should be called (on average) {TICKS_PER_16TH} times per step
  // some amount of time jittering should be fine
  void tick();

  void reset();  // for resync

  int getTicksPerStep() const { return static_cast<int>(resolution_); }

  int getTicksHalfStep() const { return getTicksPerStep() / 2; }

  // for GUI to show playhead position, TODO: make return value fractional
  int getCurrentStepIndex() const;

  // TODO: track utilities (randomize, humanize, rotate, Euclidean, Grids,
  // etc.)

protected:
  void renderNote(int index, Note note);

  // timestamp in ticks (not seconds or samples)
  void renderMidiMessage(juce::MidiMessage message);

  const KeyboardState& keyboardRef;

private:
  int channel_;

  // track parameters as seen by the user

  int trackLength_;
  int trackLengthDeferred_;
  Resolution resolution_;

  // TODO: implement swing (should not affect roll)
  [[maybe_unused]] float swing_;
  [[maybe_unused]] bool resync_to_longest_track_;  // or master length?

  bool enabled_;

  // function related variables
  int tick_;

  // derived class must implement renderStep and getStepNoteRenderTick
  virtual void renderStep(int index) = 0;
  virtual int getStepRenderTick(int index) const = 0;

  /*
    MIDI buffer inspired by the endless scrolling background technique in
    early arcade games
    invariant: MIDI messages are always sorted by timestamp
    note: when porting to Spark/Prologue, change from juce::MidiMessageSequence
    to a simpler data structure
  */

  juce::MidiMessageSequence midiQueue_, midiQueueNext_;
};

}  // namespace Sequencer