#pragma once
#include "PolyArp/Part.h"
#include "PolyArp/KeyboardState.h"
#include "PolyArp/NoteLimiter.h"

// this class serve as a data management layer between the core sequencer logic
// (Part.cpp) and global seq/arp business logic

// TODO: track utilities (randomize, humanize, rotate, etc.)

namespace Sequencer {

static inline bool ApproximatelyEqual(float x, float y) {
  return std::abs(x - y) < 0.0001f;
}

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
    std::sort(&notes[0], &notes[POLYPHONY], [](const Note& a, const Note& b) {
      if (ApproximatelyEqual(a.offset, b.offset))
        return a.number < b.number;
      return a.offset < b.offset;
    });
  }

  void alignWith(Note other) {
    for (auto& note : notes) {
      note.velocity = other.velocity;
      note.offset = other.offset;
      note.length = other.length;
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

  // TODO: take into account polyphony here? add a parameter called
  // maxNumberVoices here
  void addNote(Note newNote) {
    if (!enabled) {  // step enabled through realtime recording
      reset();
      notes[0] = newNote;
      alignWith(newNote);
      enabled = true;
      return;
    };

    // note: be consistent with note stealing policy in NoteLimiter
    // same note replacement -> search free slot -> LRU replacement
    for (auto& note : notes) {
      if (note.number == newNote.number) {
        note = newNote;
        sort();
        return;
      }
    }

    // take voice limit into account here?
    for (auto& note : notes) {
      if (note.number <= DISABLED_NOTE) {
        note = newNote;
        sort();
        return;
      }
    }

    // LRU replacement
    // int closest_index = 0;
    // int closest_distance = 127;
    // for (int i = 0; i < POLYPHONY; ++i) {
    //   int distance = std::abs(notes[i].number - newNote.number);
    //   if (distance <= closest_distance) {
    //     closest_distance = distance;
    //     closest_index = i;
    //   }
    // }

    int lru_index = 0;
    float min_offset = notes[0].offset;
    for (int i = 1; i < POLYPHONY; ++i) {
      if (notes[i].offset < min_offset) {
        min_offset = notes[i].offset;
        lru_index = i;
      }
    }

    notes[lru_index] = newNote;
    sort();
    return;
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
            const NoteLimiter& noteLimiter,
            int length = STEP_SEQ_DEFAULT_LENGTH,
            Resolution resolution = _16th)
      : Part(channel, length, resolution),
        noteLimiterRef(noteLimiter),
        interval_(0),
        overdub_(false) {}

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

  void setOverdub(bool enabled) { overdub_ = enabled; }

private:
  StepType steps_[STEP_SEQ_MAX_LENGTH];

  // const KeyboardState& keyboardRef;

  const NoteLimiter& noteLimiterRef;

  int interval_;

  bool overdub_;

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
      // overdub (mute active notes from note limiter)
      if (overdub_) {
        for (int i = 0; i < POLYPHONY; ++i) {
          // order matters: sort by offset and then by note number
          if (!noteLimiterRef.tryNoteOn(step.notes[i].number,
                                        Priority::Sequencer)) {
            step.notes[i].number = DISABLED_NOTE;
          }
        }

        if (step.isEmpty()) {
          step.reset();
          return;
        }
      }

      // probability check
      // if (juce::Random::getSystemRandom().nextFloat() >= step.probability) {
      //   return;
      // }

      // render all notes in the step
      for (int j = 0; j < POLYPHONY; ++j) {
        // render note
        renderNote(index, steps_[index].notes[j].transposed(interval_));
      }
    }
  }
};
}  // namespace Sequencer