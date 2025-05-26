#pragma once
#include <juce_audio_processors/juce_audio_processors.h>  //juce::MidiMessage
#include <algorithm>
#include <vector>
#include "PolyArp/Note.h"

// note: this class is appropriating the Note struct from the sequencer but
// using only half of the fields, maybe there is a more graceful way to do that

// API for arp feels weird, maybe they should just return note number or
// std::pair

namespace Sequencer {

class KeyboardState {
public:
  KeyboardState() : lastChannel_(1) {}

  // let's pray that the user will not switch channel in the middle of a note
  int getLastChannel() const { return lastChannel_; }

  void handleNoteOn(juce::MidiMessage noteOn) {
    int note_number = noteOn.getNoteNumber();
    activeNoteStack_.push_back(note_number);
    noteOns_[note_number] = noteOn;
    lastChannel_ = noteOn.getChannel();
  }

  // returns the matching note on message
  juce::MidiMessage handleNoteOff(juce::MidiMessage noteOff) {
    int note_number = noteOff.getNoteNumber();

    auto it = std::find(activeNoteStack_.begin(), activeNoteStack_.end(),
                        note_number);

    if (it == activeNoteStack_.end()) {
      DBG("note on and note off mismatch!");
      return juce::MidiMessage();
    } else {
      activeNoteStack_.erase(it);
      return noteOns_[note_number];
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

  // APIs for arpeggiator
  const auto& getNoteStack() const { return activeNoteStack_; }

  Note getLowestNote() const {
    jassert(!activeNoteStack_.empty());

    auto lowest =
        std::min_element(activeNoteStack_.begin(), activeNoteStack_.end());

    return {.number = *lowest, .velocity = noteOns_[*lowest].getVelocity()};
  }

  Note getHighestNote() const {
    jassert(!activeNoteStack_.empty());

    auto highest =
        std::max_element(activeNoteStack_.begin(), activeNoteStack_.end());

    return {.number = *highest, .velocity = noteOns_[*highest].getVelocity()};
  }

  Note getEarliestNote() const {
    jassert(!activeNoteStack_.empty());

    auto earliest = activeNoteStack_.front();
    return {.number = earliest, .velocity = noteOns_[earliest].getVelocity()};
  }

  // the caller is responsible to make sure stack is not empty
  Note getLatestNote() const {
    jassert(!activeNoteStack_.empty());

    auto latest = activeNoteStack_.back();
    return {.number = latest, .velocity = noteOns_[latest].getVelocity()};
  }

  Note getNextNote(int noteNumber) const {
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

      if (best_note_number == DUMMY_NOTE) {
        return Note::dummy();
      }

      return {.number = best_note_number,
              .velocity = noteOns_[best_note_number].getVelocity()};
    }

    ++it;
    if (it == activeNoteStack_.end()) {
      return Note::dummy();
    }

    int nextNoteNumber = *it;

    return {.number = nextNoteNumber,
            .velocity = noteOns_[nextNoteNumber].getVelocity()};
  }

  Note getHigherNote(int noteNumber) const {
    jassert(!activeNoteStack_.empty());

    int best_note_number = DUMMY_NOTE;
    for (int n : activeNoteStack_) {
      if (n > noteNumber) {
        if (best_note_number == DUMMY_NOTE || n < best_note_number) {
          best_note_number = n;
        }
      }
    }

    if (best_note_number == DUMMY_NOTE) {
      return Note::dummy();
    }

    return {.number = best_note_number,
            .velocity = noteOns_[best_note_number].getVelocity()};
  }

  Note getLowerNote(int noteNumber) const {
    jassert(!activeNoteStack_.empty());

    int best_note_number = DUMMY_NOTE;
    for (int n : activeNoteStack_) {
      if (n < noteNumber) {
        if (best_note_number == DUMMY_NOTE || n > best_note_number) {
          best_note_number = n;
        }
      }
    }

    if (best_note_number == DUMMY_NOTE) {
      return Note::dummy();
    }

    return {.number = best_note_number,
            .velocity = noteOns_[best_note_number].getVelocity()};
  }

  Note getRandomNote() const {
    jassert(!activeNoteStack_.empty());

    size_t index = static_cast<size_t>(juce::Random::getSystemRandom().nextInt(
        static_cast<int>(activeNoteStack_.size())));
    int note_number = activeNoteStack_[index];

    return {.number = note_number,
            .velocity = noteOns_[note_number].getVelocity()};
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

  // TODO: APIs for VoiceAssigner such as getLeastRecentUsedNote
private:
  std::vector<int> activeNoteStack_;  // keep track of pressed notes
  juce::MidiMessage noteOns_[128];    // hold actual note on messages

  int lastChannel_;
};
}  // namespace Sequencer
