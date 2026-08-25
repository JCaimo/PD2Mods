#pragma once

#include "../Module.h"

#include <Windows.h>
#include <string>

enum class QuickResumeStep
{
    Idle,
    Ready,
    Triggered
};

class QuickResume : public Module
{
private:
    std::string characterName;

    QuickResumeStep step;

    bool available;
    bool xWasDown;

    unsigned int xPressCount;

    DWORD controllerResults[4];
    WORD controllerButtons[4];

    void CaptureCharacter();
    bool PollXPressed();
    void SampleControllers();

public:
    QuickResume();

    void OnGameJoin() override;
    void OnGameExit() override;
    void OnLoop() override;
    void OnOOGDraw() override;
};