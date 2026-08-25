#pragma once

#include "../Module.h"

#include <Windows.h>
#include <string>


enum class QuickResumeStep
{
    Idle,
    Ready,

    OpenSinglePlayer,
    WaitCharacterSelect,

    SelectCharacter,
    WaitCharacterSelected,

    PressCharacterOk,
    WaitDifficultySelect,

    SelectDifficulty,
    WaitGameJoin,

    Failed
};


enum class QuickResumeDifficultyMode
{
    HighestUnlocked = 0,
    LastPlayed = 1,
    Normal = 2,
    Nightmare = 3,
    Hell = 4
};


class QuickResume : public Module
{
private:
    std::string characterName;

    QuickResumeStep step;

    bool available;
    bool crossWasDown;

    BYTE lastPlayedDifficulty;

    DWORD stepStartedAt;


    void CaptureGameState();

    bool PollCrossPressed();
    void SyncCrossState();

    void SetStep(
        QuickResumeStep newStep
    );

    void Fail(
        const std::string& message
    );

    void AdvanceQuickResume();


public:
    QuickResume();

    ~QuickResume() override =
        default;

    void OnGameJoin() override;
    void OnGameExit() override;
    void OnLoop() override;
    void OnOOGDraw() override;
};