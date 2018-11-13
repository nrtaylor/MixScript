/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include <atomic>

namespace MixScript {
    struct Mixer;
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

    StringArray getMenuBarNames() override;
    PopupMenu getMenuForIndex(int topLevelMenuIndex, const String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    //==============================================================================
    void paint (Graphics& g) override;
    void resized() override;
    
private:
    //==============================================================================
    // Your private member variables go here...
    std::unique_ptr<MixScript::Mixer> mixer;

    std::atomic_int32_t queued_cue;
    std::atomic_bool playback_paused;
    std::atomic_bool modifier_mono;

    // UI
    TextButton button_loadfile;
    Label label_loadfile;
    TextButton button_outfile;
    Label label_outfile;

    MenuBarComponent menuBar;

    void ExportRender();
    void SaveProject();
    void LoadProject();

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
