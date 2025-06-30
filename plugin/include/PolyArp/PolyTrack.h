#pragma once
#include "PolyArp/Part.h"
#include "PolyArp/KeyboardState.h"

// this class serve as a data management layer between the core sequencer logic
// (Part.cpp) and global seq/arp business logic

// TODO: track utilities (randomize, humanize, rotate, etc.)

namespace Sequencer {

template <int POLYPHONY>
struct PolyStep {
  bool enabled = false;
  Note notes[POLYPHONY];

  void reset() {
    enabled = false;
    // probability = 1.0;
    for (auto& note : notes) {
      note.reset();
    }
    notes[0].number = DEFAULT_NOTE;
  }

  void sort() {
    std::sort(&notes[0], &notes[POLYPHONY],
              [](const Note& a, const Note& b) { return a.number > b.number; });
  }

  void align(int velocity = DEFAULT_VELOCITY,
             float offset = 0.f,
             float length = DEFAULT_LENGTH) {
    for (auto& note : notes) {
      note.velocity = velocity;
      note.offset = offset;
      note.length = length;
    }
  }

  bool isEmpty() const {
    bool is_empty = true;
    for (auto note : notes) {
      if (note.number != DISABLED_NOTE) {
        is_empty = false;
      }
    }
    return is_empty;
  }

  void addNote(Note new_note) {
    if (!enabled) {  // when enabled through live rec
      reset();
      notes[0] = new_note;
      align(new_note.velocity, new_note.offset, new_note.length);
      enabled = true;
      return;
    };

    // note: this should be consistent with the synth's note stealing algorithm
    for (auto& note : notes) {
      if (note.number == new_note.number) {
        note = new_note;
        return;
      }
    }

    for (auto& note : notes) {
      if (note.number <= DISABLED_NOTE) {
        note = new_note;
        sort();
        return;
      }
    }

    int closest_index = 0;
    int closest_distance = 127;
    for (int i = 0; i < POLYPHONY; ++i) {
      int distance = std::abs(notes[i].number - new_note.number);
      if (distance <= closest_distance) {
        closest_distance = distance;
        closest_index = i;
      }
    }

    notes[closest_index] = new_note;
    // no need to sort in this case?
  }

  int getLowestNoteNumber() const {
    int lowest = 128;
    for (int i = 0; i < POLYPHONY; ++i) {
      if (notes[i].number < lowest && notes[i].number > DISABLED_NOTE) {
        lowest = notes[i].number;
      }
    }
    return lowest;
  }

  // void removeNote(int noteNumber) {
  //   for (int i = 0; i < POLYPHONY; ++i) {
  //     if (notes[i].number == noteNumber) {
  //       notes[i].number = DISABLED_NOTE;
  //     }
  //   }

  //   if (isEmpty()) {
  //     reset();
  //   } else {
  //     sort();
  //   }
  // }

  PolyStep() { reset(); }
};

template <int POLYPHONY>
class PolyTrack : public Part {
public:
  using StepType = PolyStep<POLYPHONY>;

  PolyTrack(int channel,
            const KeyboardState& keyboard,
            int length = STEP_SEQ_DEFAULT_LENGTH,
            Resolution resolution = _16th)
      : Part(channel, length, resolution),
        keyboardRef(keyboard),
        interval_(0) {}

  StepType getStepAtIndex(int index) const { return steps_[index]; }

  void setStepAtIndex(int index, StepType step) { steps_[index] = step; }

  // returns default note if there is not data in the track
  int getRootNoteNumber() const {
    // idea: some kind of fancy key detection algorithm?
    int root = DEFAULT_NOTE;
    for (int i = 0; i < getLength(); ++i) {
      const auto& step = steps_[i];
      if (step.enabled) {
        root = step.getLowestNoteNumber();
        break;
      }
    }
    return root;
  }

  void setTransposeInterval(int semitones) { interval_ = semitones; }

private:
  StepType steps_[STEP_SEQ_MAX_LENGTH];

  const KeyboardState& keyboardRef;

  int interval_;

  int getStepRenderTick(int index) const override final {
    float offset_min = 0.0f;
    for (int i = 0; i < POLYPHONY; ++i) {
      offset_min = std::min(offset_min, steps_[index].notes[i].offset);
    }
    return static_cast<int>((index + offset_min) * getTicksPerStep());
  }

  void renderStep(int index) override final {
    auto& step = steps_[index];
    if (step.enabled) {
      // smart overdub (remove notes held by keyboard)
      // note: this is a shortcut for proper assigner, will rework later)
      if (keyboardRef.getLastChannel() == this->getChannel()) {
        for (int i = 0; i < POLYPHONY; ++i) {
          if (keyboardRef.isKeyDown(step.notes[i].number)) {
            step.notes[i].number = DISABLED_NOTE;
          }
        }
      }

      if (step.isEmpty()) {
        step.reset();
      }

      // in case note stealing disables the step
      if (!step.enabled) {
        return;
      }

      // probability check
      // if (juce::Random::getSystemRandom().nextFloat() >= step.probability) {
      //   return;
      // }

      // render all notes in the step
      for (int j = 0; j < POLYPHONY; ++j) {
        // render note
        renderNote(index,
                   steps_[index].notes[j].transposed(interval_));
      }
    }
  }
};
}  // namespace Sequencer