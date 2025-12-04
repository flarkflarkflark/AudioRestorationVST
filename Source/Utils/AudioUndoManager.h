#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <deque>

/**
 * Audio Undo Manager
 *
 * Manages undo/redo history for audio editing operations.
 * Stores snapshots of audio buffer states with descriptions.
 *
 * Memory management:
 * - Limits number of undo states to prevent excessive memory usage
 * - Each state stores a complete copy of the audio buffer
 */
class AudioUndoManager
{
public:
    AudioUndoManager (int maxUndoStates = 20)
        : maxStates (maxUndoStates)
    {
    }

    //==============================================================================
    /** Save current state before making changes */
    void saveState (const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::String& description)
    {
        // Clear any redo states when new action is performed
        while (!redoStack.empty())
            redoStack.pop_back();

        // Create state snapshot
        UndoState state;
        state.buffer.makeCopyOf (buffer);
        state.sampleRate = sampleRate;
        state.description = description;
        state.timestamp = juce::Time::getCurrentTime();

        undoStack.push_back (std::move (state));

        // Limit stack size
        while (undoStack.size() > static_cast<size_t> (maxStates))
            undoStack.pop_front();

        DBG ("Undo state saved: " + description + " (stack size: " + juce::String (undoStack.size()) + ")");
    }

    //==============================================================================
    /** Check if undo is available */
    bool canUndo() const
    {
        return !undoStack.empty();
    }

    /** Check if redo is available */
    bool canRedo() const
    {
        return !redoStack.empty();
    }

    /** Get description of next undo action */
    juce::String getUndoDescription() const
    {
        if (undoStack.empty())
            return {};
        return undoStack.back().description;
    }

    /** Get description of next redo action */
    juce::String getRedoDescription() const
    {
        if (redoStack.empty())
            return {};
        return redoStack.back().description;
    }

    //==============================================================================
    /** Perform undo - returns true if successful */
    bool undo (juce::AudioBuffer<float>& currentBuffer, double& currentSampleRate,
               const juce::String& currentDescription = "Current state")
    {
        if (undoStack.empty())
            return false;

        // Save current state to redo stack
        UndoState currentState;
        currentState.buffer.makeCopyOf (currentBuffer);
        currentState.sampleRate = currentSampleRate;
        currentState.description = currentDescription;
        currentState.timestamp = juce::Time::getCurrentTime();
        redoStack.push_back (std::move (currentState));

        // Restore previous state
        UndoState& previousState = undoStack.back();
        currentBuffer.makeCopyOf (previousState.buffer);
        currentSampleRate = previousState.sampleRate;

        juce::String desc = previousState.description;
        undoStack.pop_back();

        DBG ("Undo performed: " + desc);
        return true;
    }

    /** Perform redo - returns true if successful */
    bool redo (juce::AudioBuffer<float>& currentBuffer, double& currentSampleRate,
               const juce::String& currentDescription = "Current state")
    {
        if (redoStack.empty())
            return false;

        // Save current state to undo stack
        UndoState currentState;
        currentState.buffer.makeCopyOf (currentBuffer);
        currentState.sampleRate = currentSampleRate;
        currentState.description = currentDescription;
        currentState.timestamp = juce::Time::getCurrentTime();
        undoStack.push_back (std::move (currentState));

        // Restore next state
        UndoState& nextState = redoStack.back();
        currentBuffer.makeCopyOf (nextState.buffer);
        currentSampleRate = nextState.sampleRate;

        juce::String desc = nextState.description;
        redoStack.pop_back();

        DBG ("Redo performed: " + desc);
        return true;
    }

    //==============================================================================
    /** Clear all undo/redo history */
    void clear()
    {
        undoStack.clear();
        redoStack.clear();
        DBG ("Undo history cleared");
    }

    /** Get number of undo states */
    int getNumUndoStates() const { return static_cast<int> (undoStack.size()); }

    /** Get number of redo states */
    int getNumRedoStates() const { return static_cast<int> (redoStack.size()); }

    /** Get list of undo descriptions (newest first) */
    juce::StringArray getUndoHistory() const
    {
        juce::StringArray history;
        for (auto it = undoStack.rbegin(); it != undoStack.rend(); ++it)
            history.add (it->description);
        return history;
    }

    /** Get list of redo descriptions (nearest first) */
    juce::StringArray getRedoHistory() const
    {
        juce::StringArray history;
        for (auto it = redoStack.rbegin(); it != redoStack.rend(); ++it)
            history.add (it->description);
        return history;
    }

private:
    struct UndoState
    {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 44100.0;
        juce::String description;
        juce::Time timestamp;
    };

    std::deque<UndoState> undoStack;
    std::deque<UndoState> redoStack;
    int maxStates;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioUndoManager)
};
