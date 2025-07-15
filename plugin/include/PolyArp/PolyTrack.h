#pragma once
#include "PolyArp/Part.h"
#include "PolyArp/KeyboardState.h"
#include "PolyArp/VoiceLimiter.h"

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

  void sortByNote() {
    std::sort(std::begin(notes), std::end(notes),
              [](const Note& a, const Note& b) { return a.number > b.number; });
  }

  void sortByOffset() {
    std::sort(std::begin(notes), std::end(notes),
              [](const Note& a, const Note& b) { return a.offset < b.offset; });
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

  void addNote(Note newNote, int maxNumNotes = POLYPHONY) {
    if (!enabled) {  // step enabled by realtime recording
      reset();
      notes[0] = newNote;
      alignWith(newNote);
      enabled = true;
      return;
    };

    // note: be consistent with voice stealing policy in VoiceLimiter
    // same note replacement -> free slot -> closest/latest note replacement
    for (auto& note : notes) {
      if (note.number == newNote.number) {
        note = newNote;
        sortByNote();
        return;
      }
    }

    for (int i = 0; i < maxNumNotes; ++i) {
      if (notes[i].number <= DISABLED_NOTE) {
        notes[i] = newNote;
        sortByNote();
        return;
      }
    }

    // replace latest note
    // int latest_index = 0;
    // float max_offset = notes[0].offset;
    // for (int i = 1; i < maxNumNotes; ++i) {
    //   if (notes[i].offset > max_offset) {
    //     max_offset = notes[i].offset;
    //     latest_index = i;
    //   }
    // }
    // notes[latest_index] = newNote;
    // sortByNote();
    // return;

    // replace closest note
    int closest_index = 0;
    int closest_distance = 128;

    for (int i = 0; i < maxNumNotes; ++i) {
      int distance = std::abs(notes[i].number - newNote.number);
      if (distance < closest_distance) {
        closest_distance = distance;
        closest_index = i;
      }
    }
    notes[closest_index] = newNote;
    sortByNote();
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
            const VoiceLimiter& noteLimiter,
            int length = STEP_SEQ_DEFAULT_LENGTH,
            Resolution resolution = _16th)
      : Part(channel, length, resolution),
        voiceLimiterRef(noteLimiter),
        interval_(0),
        overdub_(false),
        rest_(false) {}

  StepType getStepAtIndex(int index) const { return steps_[index]; }

  void setStepAtIndex(int index, StepType step) { steps_[index] = step; }

  void resetStepAtIndex(int index) { steps_[index].reset(); }

  // returns default note if there is not data in the track
  int getRootNoteNumber() const {
    // idea: some kind of key detection algorithm
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

  void setRest(bool enabled) { rest_ = enabled; }

private:
  StepType steps_[STEP_SEQ_MAX_LENGTH];

  const VoiceLimiter& voiceLimiterRef;

  int interval_;
  bool overdub_;
  bool rest_;

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
      // overdub (modify step data based on actual voice usage)
      if (overdub_) {
        if (rest_) {
          step.reset();
          return;
        }

        // warning: the order of evaluating tryNoteOn matters
        step.sortByOffset();

        for (auto& note : step.notes) {
          if (note.number > DISABLED_NOTE) {
            if (!voiceLimiterRef.tryNoteOn(
                    note.number, Priority::Sequencer,
                    VoiceLimiter::StealingPolicy::Closest)) {
              note.number = DISABLED_NOTE;
            }
          }
        }

        if (step.isEmpty()) {
          step.reset();
          return;
        } else {
          step.sortByNote();
        }
      }

      // probability check
      // if (juce::Random::getSystemRandom().nextFloat() >= step.probability) {
      //   return;
      // }

      // mute if rest is pressed
      if (rest_) {
        return;
      }

      // render all notes in the step
      // order by offset for arp to play correctly
      step.sortByOffset();

      for (const auto& note : steps_[index].notes) {
        // render note
        renderNote(index, note.transposed(interval_));
      }

      step.sortByNote();
    }
  }
};
}  // namespace Sequencer