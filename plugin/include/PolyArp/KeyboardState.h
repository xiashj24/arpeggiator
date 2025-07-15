#pragma once
#include <juce_audio_processors/juce_audio_processors.h>  //juce::MidiMessage
#include <algorithm>
#include <vector>
// TODO: remove dependency on juce::MidiMessage

#define DUMMY_NOTE -1

namespace Sequencer {

inline bool IsDummyNote(int note) {
  return note == -1;
}

class KeyboardState {
public:
  KeyboardState() : lastChannel_(1) {}

  // let's pray that the user will not switch channel in the middle of a note
  int getLastChannel() const { return lastChannel_; }

  void handleNoteOn(juce::MidiMessage noteOn) {
    int note_number = static_cast<int>(noteOn.getNoteNumber());
    toggleNoteOff(noteOn.getNoteNumber());

    if (activeNoteStack_.size() == 0) {  // first note
      firstNote_ = static_cast<int>(noteOn.getNoteNumber());
    }

    activeNoteStack_.push_back(note_number);
    noteOns_[note_number] = noteOn;
    lastChannel_ = noteOn.getChannel();
  }

  // return true if successful
  bool toggleNoteOff(int note_number) {
    auto it = std::find(activeNoteStack_.begin(), activeNoteStack_.end(),
                        note_number);

    if (it == activeNoteStack_.end()) {
      return false;
    } else {
      activeNoteStack_.erase(it);
      return true;
    }
  }

  // returns the matched note on message
  juce::MidiMessage handleNoteOff(juce::MidiMessage noteOff) {
    auto note_number = noteOff.getNoteNumber();

    if (toggleNoteOff(note_number)) {
      return noteOns_[note_number];
    } else {
      DBG("note on and note off mismatch (KeyboardState)");
      return juce::MidiMessage();
    }
  }

  void reset() {
    activeNoteStack_.clear();
    // no need to clear noteOns_
  }

  bool isKeyDown(int noteNumber) const {
    return std::find(activeNoteStack_.begin(), activeNoteStack_.end(),
                     noteNumber) != activeNoteStack_.end();
  }

  int getNumNotesPressed() const {
    return static_cast<int>(activeNoteStack_.size());
  }

  bool empty() const { return getNumNotesPressed() == 0; }

  const auto& getNoteStack() const { return activeNoteStack_; }

  int getFirstNote() const { return firstNote_; }

  // caller is responsible to check getNumNotesPressed() > 0
  int getLowestNote() const {
    jassert(!activeNoteStack_.empty());

    auto result =
        std::min_element(activeNoteStack_.begin(), activeNoteStack_.end());

    return *result;
  }

  int getHighestNote() const {
    jassert(!activeNoteStack_.empty());

    auto result =
        std::max_element(activeNoteStack_.begin(), activeNoteStack_.end());

    return *result;
  }

  int getEarliestNote() const {
    jassert(!activeNoteStack_.empty());

    auto earliest_note = activeNoteStack_.front();
    return earliest_note;
  }

  // the caller is responsible to make sure stack is not empty
  int getLatestNote() const {
    jassert(!activeNoteStack_.empty());

    auto latest_note = activeNoteStack_.back();
    return latest_note;
  }

  int getNextNote(int noteNumber) const {
    jassert(!activeNoteStack_.empty());

    auto it =
        std::find(activeNoteStack_.begin(), activeNoteStack_.end(), noteNumber);
    if (it == activeNoteStack_.end()) {
      // if note already removed, use midi time stamp to find the next note
      auto note_on_time = noteOns_[noteNumber].getTimeStamp();

      double best_time = std::numeric_limits<double>::max();
      int best_note_number = DUMMY_NOTE;

      for (int n : activeNoteStack_) {
        auto active_note_on_time = noteOns_[n].getTimeStamp();
        if (active_note_on_time > note_on_time) {
          if (best_note_number == DUMMY_NOTE ||
              best_time > active_note_on_time) {
            best_time = active_note_on_time;
            best_note_number = n;
          }
        }
      }

      return best_note_number;
    }

    ++it;
    if (it == activeNoteStack_.end()) {
      return DUMMY_NOTE;
    }

    int nextNoteNumber = *it;
    return nextNoteNumber;
  }

  int getHigherNote(int noteNumber) const {
    jassert(!activeNoteStack_.empty());

    int best_note_number = DUMMY_NOTE;
    for (int n : activeNoteStack_) {
      if (n > noteNumber) {
        if (best_note_number == DUMMY_NOTE || n < best_note_number) {
          best_note_number = n;
        }
      }
    }

    return best_note_number;
  }

  int getLowerNote(int noteNumber) const {
    jassert(!activeNoteStack_.empty());

    int best_note_number = DUMMY_NOTE;
    for (int n : activeNoteStack_) {
      if (n < noteNumber) {
        if (best_note_number == DUMMY_NOTE || n > best_note_number) {
          best_note_number = n;
        }
      }
    }

    return best_note_number;
  }

  int getRandomNote() const {
    jassert(!activeNoteStack_.empty());

    size_t index = static_cast<size_t>(juce::Random::getSystemRandom().nextInt(
        static_cast<int>(activeNoteStack_.size())));
    int note_number = activeNoteStack_[index];

    return note_number;
  }

  int getAverageVelocity() const {
    if (activeNoteStack_.size() == 0) {
      return DEFAULT_VELOCITY;
    } else if (activeNoteStack_.size() == 1) {
      return noteOns_[activeNoteStack_.front()].getVelocity();
    }

    int velocity_sum = 0;
    for (const auto& note_number : activeNoteStack_) {
      velocity_sum += noteOns_[note_number].getVelocity();
    }
    return velocity_sum / static_cast<int>(activeNoteStack_.size());
  }

  // will return DEFAULT_VELOCITY if no note is being pressed
  int getLatestVelocity() const {
    if (getNumNotesPressed() > 0) {
      return noteOns_[getLatestNote()].getVelocity();
    } else {
      return DEFAULT_VELOCITY;
    }
  }

  // will return DEFAULT_VELOCITY for notes that are currently not pressed
  int getVelocityForNote(int noteNumber) const {
    if (isKeyDown(noteNumber)) {
      return noteOns_[noteNumber].getVelocity();
    } else {
      return DEFAULT_VELOCITY;
    }
  }

private:
  std::vector<int> activeNoteStack_;  // keep track of pressed notes
  juce::MidiMessage noteOns_[128];    // hold actual note on messages
  int firstNote_;
  int lastChannel_;
};
}  // namespace Sequencer
