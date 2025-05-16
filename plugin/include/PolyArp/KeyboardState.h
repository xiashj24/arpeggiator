#pragma once
#include <juce_audio_processors/juce_audio_processors.h>  //juce::MidiMessage
#include <algorithm>
#include <vector>

#define DEFAULT_VELOCITY 100

namespace Sequencer {
class KeyboardState {
public:
  KeyboardState() : lastChannel_(0) { noteStack_.reserve(128); }

  // reports the MIDI channel of the last handled MIDI message handled
  // if note midi message has been handled, it will return 0
  // for now, we can safely assume that a keyboard will not switch channel in
  // the middle of a note
  int getLastChannel() const { return lastChannel_; }

  void handleNoteOn(juce::MidiMessage noteOn) {
    // noteOns_[noteOn.getNoteNumber()].emplace(noteOn);
    noteStack_.push_back(noteOn);
    lastChannel_ = noteOn.getChannel();
  }

  // returns the matching NoteOn message
  juce::MidiMessage handleNoteOff(juce::MidiMessage noteOff) {
    // auto note_on = noteOns_[noteOff.getNoteNumber()].value();
    // noteOns_[noteOff.getNoteNumber()].reset();

    auto result =
        std::find_if(noteStack_.begin(), noteStack_.end(),
                     [noteOff](const juce::MidiMessage& noteOn) {
                       return noteOn.getNoteNumber() == noteOff.getNoteNumber();
                     });

    if (result == noteStack_.end()) {
      return juce::MidiMessage();
    }

    juce::MidiMessage matched_note_on = *result;
    noteStack_.erase(result);
    return matched_note_on;
  }

  bool isKeyDown(int noteNumber) const {
    return std::any_of(noteStack_.begin(), noteStack_.end(),
                       [noteNumber](const juce::MidiMessage& msg) {
                         return msg.getNoteNumber() == noteNumber;
                       });
  }

  // for arp
  const auto& getNoteStack() const { return noteStack_; }

  int getNumNotesPressed() const {
    return static_cast<int>(noteStack_.size());
  }

  int getAverageVelocity() const {
    if (noteStack_.size() == 0) {
      return DEFAULT_VELOCITY;
    } else if (noteStack_.size() == 1) {
      return noteStack_.front().getVelocity();
    }

    int velocity_sum = 0;
    for (const auto& note : noteStack_) {
      velocity_sum += note.getVelocity();
    }
    return velocity_sum / static_cast<int>(noteStack_.size());
  }

  // TODO: APIs for VoiceAssigner such as getLeastRecentUsedNote
private:
  // note: should probably use a simple struct as entry
  std::vector<juce::MidiMessage> noteStack_;
  // std::optional<juce::MidiMessage> noteOns_[128];

  int lastChannel_;
};
}  // namespace Sequencer
