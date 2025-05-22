#include "PolyArp/Track.h"

namespace Sequencer {

int Track::getCurrentStepIndex() const {
  return (tick_ + getTicksHalfStep()) / getTicksPerStep();
}

void Track::renderNote(int index, Note note) {
  if (note.number <= DISABLED_NOTE)
    return;

  int note_on_tick =
      static_cast<int>((index + note.offset) * getTicksPerStep());
  int note_off_tick =
      static_cast<int>((index + note.offset + note.length) * getTicksPerStep());

  // force note off before the next note on of the same note
  // search all midi messages after note_on_tick
  // if there is a note off with the same note number
  // delete that and insert a new note off at note_on_tick
  // Can I simply this code with updateMatchPairs?

  bool note_off_deleted = false;
  for (int i = midiQueue_.getNextIndexAtTime(tick_);
       i < midiQueue_.getNumEvents(); ++i) {
    auto message = midiQueue_.getEventPointer(i)->message;
    if (message.isNoteOff() && message.getNoteNumber() == note.number) {
      midiQueue_.deleteEvent(i, false);
      note_off_deleted = true;
    }
  }

  if (note_off_deleted) {
    juce::MidiMessage early_note_off_message = juce::MidiMessage::noteOff(
        getChannel(), note.number, static_cast<juce::uint8>(note.velocity));
    early_note_off_message.setTimeStamp(note_on_tick);  // -1?
    renderMidiMessage(early_note_off_message);
  }

  // note on
  juce::MidiMessage note_on_message = juce::MidiMessage::noteOn(
      getChannel(), note.number, static_cast<juce::uint8>(note.velocity));
  note_on_message.setTimeStamp(note_on_tick);
  renderMidiMessage(note_on_message);

  // note off
  juce::MidiMessage note_off_message = juce::MidiMessage::noteOff(
      getChannel(), note.number, static_cast<juce::uint8>(note.velocity));
  note_off_message.setTimeStamp(note_off_tick);
  renderMidiMessage(note_off_message);
}

// insert a future MIDI message into the MIDI buffer based on its timestamp
void Track::renderMidiMessage(juce::MidiMessage message) {
  // int tick = static_cast<int>(message.getTimeStamp());

  midiQueue_.addEvent(message);
  // if (tick < trackLength_ * getTicksPerStep() - getTicksHalfStep()) {
  //   firstRun_.addEvent(message);

  // } else {
  //   message.setTimeStamp(tick - trackLength_ * getTicksPerStep());
  //   secondRun_.addEvent(message);
  // }
}

void Track::reset(float index) {
  midiQueue_.clear();
  tick_ = static_cast<int>(getTicksPerStep() * index);
}

void Track::sendNoteOffNow() {
  for (int i = midiQueue_.getNextIndexAtTime(tick_);
       i < midiQueue_.getNumEvents(); ++i) {
    auto message = midiQueue_.getEventPointer(i)->message;
    if (message.isNoteOff()) {
      message.setTimeStamp(tick_);  // +1?
      sendMidiMessage(message);
    }
  }
}

void Track::tick() {
  if (this->enabled_) {
    int index = getCurrentStepIndex();

    // render the step just right before it's too late
    if (tick_ == getStepRenderTick(index)) {
      renderStep(index);
      // DBG("step render tick: " << tick_);
    }

    // send current tick's MIDI events
    for (int i = midiQueue_.getNextIndexAtTime(tick_);
         i < midiQueue_.getNextIndexAtTime(tick_ + 1); ++i) {
      auto message = midiQueue_.getEventPointer(i)->message;

      // TODO: this is necessary for sequencer but should not be used by arp
      // which indicate that MIDI merging should be processed by a separate
      // module (probably Voice Assigner)
      // do not note off if held by the keyboard
      // if (message.isNoteOff() &&
      //     keyboardRef.isKeyDown(message.getNoteNumber())) {
      //   continue;
      // }

      sendMidiMessage(message);
    }
  }

  // advance ticks and overwrap from (length-0.5) to (-0.5) step
  // because the first step could start from negative steps

  // remeber trackLength_ could be suddenly changed larger or smaller
  // (potentially by a different thread) at any time
  // one possible solution: only change length at the start of each step

  // when the length suddently changes out of no where, it fucks up the note off
  // timing of notes already rendered... to compensate for that, need to modify
  // existing notes in the second run but it's also a good chance to reflect if
  // this data structure makes sense

  // also, noteoffs in the firstrun need to be adjusted too?

  // + getTicksPerStep() * (difference in track length?)
  // so modifying track length should be defereed when on step grid

  // occasional missing note off, why?
  tick_ += 1;

  // update track length at the end of each loop
  if (tick_ % getTicksPerStep() == getTicksHalfStep()) {
    trackLength_ = trackLengthDeferred_;
    // DBG("Track Legnth: " << trackLength_);
  }

  if (tick_ >= trackLength_ * getTicksPerStep() - getTicksHalfStep()) {
    // tick_ = -getTicksHalfStep();
    tick_ -= trackLength_ * getTicksPerStep();

    midiQueue_.addTimeToMessages(-trackLength_ * getTicksPerStep());

    for (const auto& midiEvent : midiQueue_) {
      if (midiEvent->message.getTimeStamp() >= -getTicksHalfStep()) {
        midiQueueNext_.addEvent(midiEvent->message);
      }
    }
    midiQueue_.swapWith(midiQueueNext_);
    midiQueueNext_.clear();
  }
}

}  // namespace Sequencer