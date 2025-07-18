#pragma once
#include <deque>
#include <algorithm>
#include <vector>

// priority based LRU note stealing module with dynamic polyphony

// TODO: the logic of this code is very similar to a VoiceAllocator (MI stmlib)
// refactor VoiceLimiter as a special case of VoiceAllocator?
// the only thing you need to do is probably use voice index as the physical
// voice number (%Polyphony?)

namespace Sequencer {

// larger value, higher priority
enum class Priority { Keyboard = 1, Sequencer = 0 };

class VoiceLimiter {
public:
  enum class StealingPolicy { LRU, Closest };

  VoiceLimiter(size_t numVoices)
      : numVoices_{numVoices}, numVoicesCached_{numVoices} {}

  // Caveat: when setNumVoices reduces polyphony (e.g. 10 -> 5)
  // note offs will not be properly be issued, the caller is responsible to
  // note off those voices
  // TODO: specify rules for which voices are discarded (e.g. higher indexed
  // ones)
  void setNumVoices(size_t numVoices) { numVoices_ = numVoices; }
  size_t getNumVoices() const { return numVoices_; }

  void setBypass(bool enabled) {
    if (enabled) {
      numVoicesCached_ = numVoices_;
      numVoices_ = 127;
    } else {
      numVoices_ = numVoicesCached_;
    }
  }

  bool tryNoteOn(int noteNumber,
                 Priority priority,
                 StealingPolicy policy) const {
    // unsafe but avoids code duplication...
    return const_cast<VoiceLimiter*>(this)->noteOn(noteNumber, priority, policy,
                                                   nullptr, false);
  }

  // return true if sucessfully allocated
  // stolenNote is not written if no note stealing happens
  bool noteOn(int noteNumber,
              Priority priority,
              StealingPolicy policy,
              int* stolenNote = nullptr,
              bool modify = true) {
    // retrigger same note from same or higher priority
    auto result = std::find_if(
        lru_.begin(), lru_.end(),
        [noteNumber](const auto& voice) { return voice.note == noteNumber; });
    if (result != lru_.end()) {  // same note found
      if (result->priority <= priority) {
        if (modify) {
          // replace
          if (stolenNote) {
            *stolenNote = noteNumber;
          }
          lru_.erase(result);
          lru_.push_back({noteNumber, priority});
        }
        return true;
      } else {
        return false;
      }
    }

    // voice available
    if (lru_.size() < numVoices_) {
      if (modify) {
        lru_.push_back({noteNumber, priority});
      }
      return true;
    }

    // steal LRU note with priority <= incoming
    if (policy == StealingPolicy::LRU) {
      result = std::find_if(
          lru_.begin(), lru_.end(),
          [priority](const auto& voice) { return voice.priority <= priority; });

      if (result != lru_.end()) {
        // replace
        if (modify) {
          if (stolenNote) {
            *stolenNote = result->note;
          }
          lru_.erase(result);
          lru_.push_back({noteNumber, priority});
        }

        return true;
      }
    } else if (policy == StealingPolicy::Closest) {
      // find the closest note with priority <= incoming
      result = lru_.end();
      int min_distance = 128;

      for (auto it = lru_.begin(); it != lru_.end(); ++it) {
        if (it->priority <= priority) {
          int distance = std::abs(it->note - noteNumber);
          if (distance < min_distance) {
            min_distance = distance;
            result = it;
          }
        }
      }

      if (result != lru_.end()) {
        if (modify) {
          if (stolenNote) {
            *stolenNote = result->note;
          }
          lru_.erase(result);
          lru_.push_back({noteNumber, priority});
        }
        return true;
      }
    }

    return false;
  }

  // return true if note successfully released
  bool noteOff(int noteNumber, Priority priority) {
    auto result = std::find_if(
        lru_.begin(), lru_.end(),
        [noteNumber](const auto& voice) { return voice.note == noteNumber; });

    if (result != lru_.end()) {
      // do not note off if voice is used by higher priority source
      if (result->priority > priority) {
        return false;
      }

      lru_.erase(result);
      return true;
    }

    return false;  // not found, technically this should not happen though
  }

  size_t getNumActiveVoices() const { return lru_.size(); }

  std::vector<int> getActiveNotes() const {
    std::vector<int> result;
    for (const auto& voice : lru_) {
      result.push_back(voice.note);
    }
    return result;
  }

private:
  struct Voice {
    int note;
    Priority priority;
  };

  // TODO: refactor using RingBuffer (use std::array to minimize allocation)
  std::deque<Voice> lru_;  // front is least-recently used
  size_t numVoices_;
  size_t numVoicesCached_;
};
}  // namespace Sequencer