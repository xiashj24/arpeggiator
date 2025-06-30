#include "PolyArp/Arpeggiator.h"

namespace Sequencer {

// reference: https://paulbatchelor.github.io/sndkit/euclid/
inline bool euclid_simple(int p, int n, int r, int i) {
  return (((p * (i + r)) % n) + p) >= n;
}

// int euclid_simple_2(int p, int n, int r, int i) {
//   return (p * (i + r)) % n < p;
// }

inline int positive_modulo(int i, int n) {
  return (i % n + n) % n;
}

template <class T>
void RemoveDuplicatesInVector(std::vector<T>& vec) {
  std::sort(vec.begin(), vec.end());
  vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
}

template <typename T>
void FastShuffle(std::vector<T>& array, juce::Random& rng) {
  for (int i = static_cast<int>(array.size()) - 1; i > 0; --i) {
    int j = rng.nextInt(i + 1);  // 0 ≤ j ≤ i
    std::swap(array[static_cast<size_t>(i)], array[static_cast<size_t>(j)]);
  }
}

void Arpeggiator::shuffleNotesWithOctave() {
  shuffledNoteList_ = keyboard_.getNoteStack();

  auto note_list_copy = shuffledNoteList_;

  for (int i = 1; i < octave_; ++i) {
    for (auto& n : note_list_copy) {
      n += 12;
    }
    shuffledNoteList_.insert(shuffledNoteList_.end(), note_list_copy.begin(),
                              note_list_copy.end());
  }

  RemoveDuplicatesInVector(shuffledNoteList_);
  FastShuffle(shuffledNoteList_, juce::Random::getSystemRandom());
}

Note Arpeggiator::getAdjacentArpNote(bool repeat_boundary) {
  auto arp_note = (rising_) ? keyboard_.getHigherNote(lastNoteNumber_)
                            : keyboard_.getLowerNote(lastNoteNumber_);

  if (arp_note.isDummy()) {
    // reverse direction if hit boundary
    if (rising_ && currentOctave_ == octave_ - 1) {
      rising_ = false;
      if (repeat_boundary) {
        return keyboard_.getHighestNote();
      }
    } else if (!rising_ && currentOctave_ == 0) {
      rising_ = true;
      if (repeat_boundary) {
        return keyboard_.getLowestNote();
      }
    }

    // try again
    if (rising_) {
      arp_note = keyboard_.getHigherNote(lastNoteNumber_);

      if (arp_note.isDummy()) {
        arp_note = keyboard_.getLowestNote();
        currentOctave_ = (currentOctave_ + 1) % octave_;
      }
    } else {
      arp_note = keyboard_.getLowerNote(lastNoteNumber_);
      if (arp_note.isDummy()) {
        arp_note = keyboard_.getHighestNote();
        currentOctave_ = positive_modulo(currentOctave_ - 1, octave_);
      }
    }
  }
  return arp_note;
}

void Arpeggiator::renderStep(int index) {
  int num_notes_pressed = keyboard_.getNumNotesPressed();
  jassert(num_notes_pressed >= 1);  // otherwise there is nothing to play

  // check euclid
  int euclid_index = index % euclidLength_;
  if (!euclid_simple(euclidFill_, euclidLength_, euclidRotate_,
                     euclid_index))
    return;

  float note_length = gate_;
  // euclid legato
  if (euclidLegato_) {
    euclid_index = (euclid_index + 1) % euclidLength_;
    while (!euclid_simple(euclidFill_, euclidLength_, euclidRotate_,
                          euclid_index)) {
      note_length += 1.f;
      euclid_index = (euclid_index + 1) % euclidLength_;
    }
  }

  Note arp_note;
  bool repeat_boundary = false;

  switch (type_) {
    // MARK: manual
    case ArpType::Manual:
      if (index == 0) {
        arp_note = keyboard_.getEarliestNote();
        currentOctave_ = 0;
      } else {
        arp_note = keyboard_.getNextNote(lastNoteNumber_);

        if (arp_note.isDummy()) {
          arp_note = keyboard_.getEarliestNote();
          currentOctave_ = (currentOctave_ + 1) % octave_;
        }
      }
      break;

    // MARK: classic
    case ArpType::Rise:
      if (index == 0) {
        arp_note = keyboard_.getLowestNote();
        currentOctave_ = 0;
      } else {
        arp_note = keyboard_.getHigherNote(lastNoteNumber_);
        if (arp_note.isDummy()) {
          arp_note = keyboard_.getLowestNote();
          currentOctave_ = (currentOctave_ + 1) % octave_;
        }
      }
      break;

    case ArpType::Fall:
      if (index == 0) {
        arp_note = keyboard_.getHighestNote();
        currentOctave_ = octave_ - 1;
      } else {
        arp_note = keyboard_.getLowerNote(lastNoteNumber_);

        if (arp_note.isDummy()) {
          arp_note = keyboard_.getHighestNote();
          currentOctave_ = positive_modulo(currentOctave_ - 1, octave_);
        }
      }
      break;

    case ArpType::RiseNFall:
      repeat_boundary = true;
      [[fallthrough]];

    case ArpType::RiseFall:
      if (index == 0) {
        arp_note = keyboard_.getLowestNote();
        rising_ = true;
        currentOctave_ = 0;
      } else {
        arp_note = getAdjacentArpNote(repeat_boundary);
      }
      break;

    case ArpType::FallNRise:
      repeat_boundary = true;
      [[fallthrough]];

    case ArpType::FallRise:
      if (index == 0) {
        arp_note = keyboard_.getHighestNote();
        rising_ = false;
        currentOctave_ = octave_ - 1;
      } else {
        arp_note = getAdjacentArpNote(repeat_boundary);
      }
      break;

    // MARK: random
    case ArpType::Random:
      arp_note = keyboard_.getRandomNote();
      currentOctave_ = juce::Random::getSystemRandom().nextInt(octave_);
      DBG("index: " << index << " random 1 mode");
      break;

    case ArpType::Shuffle:
      // re-shuffle on every loop start
      if (index % (num_notes_pressed * octave_) == 0) {
        shuffleNotesWithOctave();
        // TODO: reshuffle untile shuffled_note_list[0] != last_note_number
      }
      arp_note.number = shuffledNoteList_[static_cast<size_t>(
          index % (num_notes_pressed * octave_))];
      arp_note.velocity = keyboard_.getAverageVelocity();
      break;

    case ArpType::Walk:
      if (index == 0) {
        arp_note = keyboard_.getRandomNote();
        currentOctave_ = juce::Random::getSystemRandom().nextInt(octave_);
      } else if (juce::Random::getSystemRandom().nextBool()) {
        arp_note = keyboard_.getHigherNote(lastNoteNumber_);
        if (arp_note.isDummy()) {
          arp_note = keyboard_.getLowestNote();
          currentOctave_ = (currentOctave_ + 1) % octave_;
        }
      } else {
        arp_note = keyboard_.getLowerNote(lastNoteNumber_);
        if (arp_note.isDummy()) {
          arp_note = keyboard_.getHighestNote();
          currentOctave_ = positive_modulo(currentOctave_ - 1, octave_);
        }
      }
      break;

    case ArpType::RandomThree:
      if (num_notes_pressed > 2 || octave_ > 2 ||
          (num_notes_pressed == 2 && octave_ == 2)) {
        // re-shuffle on every step
        shuffleNotesWithOctave();

        for (size_t i = 0; i < 3; ++i) {
          arp_note.number = shuffledNoteList_[i];
          arp_note.length = note_length;
          arp_note.velocity = keyboard_.getAverageVelocity();

          renderNote(index, arp_note);
        }
        return;
      } else
        [[fallthrough]];

    case ArpType::RandomTwo:
      jassert(octave_ >= 1);
      if (num_notes_pressed == 1 && octave_ == 1) {
        arp_note = keyboard_.getLowestNote();
      } else {
        // re-shuffle on every step
        shuffleNotesWithOctave();

        for (size_t i = 0; i < 2; ++i) {
          arp_note.number = shuffledNoteList_[i];
          arp_note.length = note_length;
          arp_note.velocity = keyboard_.getAverageVelocity();

          renderNote(index, arp_note);
        }
        DBG("index: " << index << " random 2/3 mode");
        return;
      }
      break;

    case ArpType::Chord:
      for (int n : shuffledNoteList_) {
        arp_note.number = n;
        arp_note.length = note_length;
        arp_note.velocity = keyboard_.getAverageVelocity();

        renderNote(index, arp_note);
      }
      DBG("index: " << index << " chord");
      return;
      break;
  }

  lastNoteNumber_ = arp_note.number;
  arp_note.number += 12 * currentOctave_;
  arp_note.length = note_length;
  // wrap to the same note below 128
  arp_note.number = WrapNoteIntoValidRange(arp_note.number);

  renderNote(index, arp_note);
  DBG("index: " << index << " note: " << arp_note.number);
}

// MARK: euclid
void Arpeggiator::setEuclidPattern(EuclidPattern pattern) {
  switch (pattern) {
    case EuclidPattern::Off:
      euclidFill_ = 1;
      euclidLength_ = 1;
      break;
    case EuclidPattern::_15_16:
      euclidFill_ = 15;
      euclidLength_ = 16;
      break;
    case EuclidPattern::_13_14:
      euclidFill_ = 13;
      euclidLength_ = 14;
      break;
    case EuclidPattern::_12_13:
      euclidFill_ = 12;
      euclidLength_ = 13;
      break;
    case EuclidPattern::_11_12:
      euclidFill_ = 11;
      euclidLength_ = 12;
      break;
    case EuclidPattern::_10_11:
      euclidFill_ = 10;
      euclidLength_ = 11;
      break;
    case EuclidPattern::_9_10:
      euclidFill_ = 9;
      euclidLength_ = 10;
      break;
    case EuclidPattern::_8_9:
      euclidFill_ = 8;
      euclidLength_ = 9;
      break;
    case EuclidPattern::_7_8:
      euclidFill_ = 7;
      euclidLength_ = 8;
      break;
    case EuclidPattern::_13_15:
      euclidFill_ = 13;
      euclidLength_ = 15;
      break;
    case EuclidPattern::_6_7:
      euclidFill_ = 6;
      euclidLength_ = 7;
      break;
    case EuclidPattern::_11_13:
      euclidFill_ = 11;
      euclidLength_ = 13;
      break;
    case EuclidPattern::_5_6:
      euclidFill_ = 5;
      euclidLength_ = 6;
      break;
    case EuclidPattern::_9_11:
      euclidFill_ = 9;
      euclidLength_ = 11;
      break;
    case EuclidPattern::_13_16:
      euclidFill_ = 13;
      euclidLength_ = 16;
      break;
    case EuclidPattern::_4_5:
      euclidFill_ = 4;
      euclidLength_ = 5;
      break;
    case EuclidPattern::_11_14:
      euclidFill_ = 11;
      euclidLength_ = 14;
      break;
    case EuclidPattern::_7_9:
      euclidFill_ = 7;
      euclidLength_ = 9;
      break;
    case EuclidPattern::_10_13:
      euclidFill_ = 10;
      euclidLength_ = 13;
      break;
    case EuclidPattern::_3_4:
      euclidFill_ = 3;
      euclidLength_ = 4;
      break;
    case EuclidPattern::_11_15:
      euclidFill_ = 11;
      euclidLength_ = 15;
      break;
    case EuclidPattern::_8_11:
      euclidFill_ = 8;
      euclidLength_ = 11;
      break;
    case EuclidPattern::_5_7:
      euclidFill_ = 5;
      euclidLength_ = 7;
      break;
    case EuclidPattern::_7_10:
      euclidFill_ = 7;
      euclidLength_ = 10;
      break;
    case EuclidPattern::_9_13:
      euclidFill_ = 9;
      euclidLength_ = 13;
      break;
    case EuclidPattern::_11_16:
      euclidFill_ = 11;
      euclidLength_ = 16;
      break;
    case EuclidPattern::_9_14:
      euclidFill_ = 9;
      euclidLength_ = 14;
      break;
    case EuclidPattern::_7_11:
      euclidFill_ = 7;
      euclidLength_ = 11;
      break;
    case EuclidPattern::_5_8:
      euclidFill_ = 5;
      euclidLength_ = 8;
      break;
    case EuclidPattern::_8_13:
      euclidFill_ = 8;
      euclidLength_ = 13;
      break;
    case EuclidPattern::_3_5:
      euclidFill_ = 3;
      euclidLength_ = 5;
      break;
    case EuclidPattern::_7_12:
      euclidFill_ = 7;
      euclidLength_ = 12;
      break;
    case EuclidPattern::_4_7:
      euclidFill_ = 4;
      euclidLength_ = 7;
      break;
    case EuclidPattern::_9_16:
      euclidFill_ = 9;
      euclidLength_ = 16;
      break;
    case EuclidPattern::_5_9:
      euclidFill_ = 5;
      euclidLength_ = 9;
      break;
    case EuclidPattern::_6_11:
      euclidFill_ = 6;
      euclidLength_ = 11;
      break;
    case EuclidPattern::_7_13:
      euclidFill_ = 7;
      euclidLength_ = 13;
      break;
    case EuclidPattern::_8_15:
      euclidFill_ = 8;
      euclidLength_ = 15;
      break;
    case EuclidPattern::_7_15:
      euclidFill_ = 7;
      euclidLength_ = 15;
      break;
    case EuclidPattern::_6_13:
      euclidFill_ = 6;
      euclidLength_ = 13;
      break;
    case EuclidPattern::_5_11:
      euclidFill_ = 5;
      euclidLength_ = 11;
      break;
    case EuclidPattern::_4_9:
      euclidFill_ = 4;
      euclidLength_ = 9;
      break;
    case EuclidPattern::_7_16:
      euclidFill_ = 7;
      euclidLength_ = 16;
      break;
    case EuclidPattern::_3_7:
      euclidFill_ = 3;
      euclidLength_ = 7;
      break;
    case EuclidPattern::_5_12:
      euclidFill_ = 5;
      euclidLength_ = 12;
      break;
    case EuclidPattern::_2_5:
      euclidFill_ = 2;
      euclidLength_ = 5;
      break;
    case EuclidPattern::_5_13:
      euclidFill_ = 5;
      euclidLength_ = 13;
      break;
    case EuclidPattern::_3_8:
      euclidFill_ = 3;
      euclidLength_ = 8;
      break;
    case EuclidPattern::_4_11:
      euclidFill_ = 4;
      euclidLength_ = 11;
      break;
    case EuclidPattern::_5_14:
      euclidFill_ = 5;
      euclidLength_ = 14;
      break;
    case EuclidPattern::_5_16:
      euclidFill_ = 5;
      euclidLength_ = 16;
      break;
    case EuclidPattern::_4_13:
      euclidFill_ = 4;
      euclidLength_ = 13;
      break;
    case EuclidPattern::_3_10:
      euclidFill_ = 3;
      euclidLength_ = 10;
      break;
    case EuclidPattern::_2_7:
      euclidFill_ = 2;
      euclidLength_ = 7;
      break;
    case EuclidPattern::_3_11:
      euclidFill_ = 3;
      euclidLength_ = 11;
      break;
    case EuclidPattern::_4_15:
      euclidFill_ = 4;
      euclidLength_ = 15;
      break;
    case EuclidPattern::_3_13:
      euclidFill_ = 3;
      euclidLength_ = 13;
      break;
    case EuclidPattern::_2_9:
      euclidFill_ = 2;
      euclidLength_ = 9;
      break;
    case EuclidPattern::_3_14:
      euclidFill_ = 3;
      euclidLength_ = 14;
      break;
    case EuclidPattern::_3_16:
      euclidFill_ = 3;
      euclidLength_ = 16;
      break;
    case EuclidPattern::_2_11:
      euclidFill_ = 2;
      euclidLength_ = 11;
      break;
    case EuclidPattern::_2_13:
      euclidFill_ = 2;
      euclidLength_ = 13;
      break;
    case EuclidPattern::_2_15:
      euclidFill_ = 2;
      euclidLength_ = 15;
      break;
  }
  // this line guarantees that first pulse is always on
  euclidRotate_ = euclidLength_ / euclidFill_;
}

}  // namespace Sequencer