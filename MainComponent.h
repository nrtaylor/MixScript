/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "TrackControlsComponent.h"
#include <atomic>

namespace MixScript {
    class Mixer;
    struct TrackVisualCache;
}

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent   : public AudioAppComponent, private Timer, private MenuBarModel                        
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent();

    //==============================================================================    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    bool keyPressed(const KeyPress & key) override;
    bool keyStateChanged(bool isKeyDown) override;
    void mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel) override;
    void mouseDown(const MouseEvent &event) override;

    StringArray getMenuBarNames() override;
    PopupMenu getMenuForIndex(int topLevelMenuIndex, const String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    //==============================================================================
    void paint (Graphics& g) override;
    void resized() override;
    
    void LoadControls();
private:
    //==============================================================================
    // Your private member variables go here...
    std::unique_ptr<MixScript::Mixer> mixer;

    std::unique_ptr<MixScript::TrackVisualCache> track_playing_visuals;
    std::unique_ptr<MixScript::TrackVisualCache> track_incoming_visuals;
    MixScript::TrackVisualCache* SelectedVisuals();

    std::atomic_int32_t queued_cue;
    std::atomic_bool playback_paused;
    std::atomic_bool modifier_mono;
    std::atomic_bool record_automation_mode;

    // UI
    TextButton button_loadfile;
    Label label_loadfile;
    TextButton button_outfile;
    Label label_outfile;

    ToggleButton visual_accentuate;
    ToggleButton record_automation;

    TrackControlsComponent playing_controls;

    MenuBarComponent menuBar;
    PopupMenu menuMarkerType;

    void ExportRender();
    void SaveProject();
    void LoadProject();
    
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
