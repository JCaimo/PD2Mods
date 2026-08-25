#include "QuickResume.h"

#include "../../BH.h"
#include "../../ControllerInput.h"
#include "../../D2Ptrs.h"

#include <string>


namespace
{
    constexpr BYTE DIFFICULTY_NORMAL =
        0;

    constexpr BYTE DIFFICULTY_NIGHTMARE =
        1;

    constexpr BYTE DIFFICULTY_HELL =
        2;


    constexpr DWORD STEP_TIMEOUT_MS =
        3000;

    constexpr DWORD GAME_JOIN_TIMEOUT_MS =
        10000;


    std::string WideToUtf8(
        const wchar_t* text)
    {
        if (!text ||
            text[0] == L'\0')
        {
            return "";
        }


        const int requiredSize =
            WideCharToMultiByte(
                CP_UTF8,
                0,
                text,
                -1,
                nullptr,
                0,
                nullptr,
                nullptr
            );


        if (requiredSize <= 1)
            return "";


        std::string result(
            static_cast<size_t>(
                requiredSize
            ),
            '\0'
        );


        WideCharToMultiByte(
            CP_UTF8,
            0,
            text,
            -1,
            &result[0],
            requiredSize,
            nullptr,
            nullptr
        );


        if (!result.empty() &&
            result.back() == '\0')
        {
            result.pop_back();
        }


        return result;
    }


    bool IsControlEnabled(
        const Control* control)
    {
        if (!control)
            return false;

        //
        // Observed D2Win states:
        //
        // 5  = normal enabled
        // 4  = normal disabled
        // 13 = modal enabled
        // 12 = modal disabled
        //
        // Bit 0 represents enabled.
        //
        return
            (control->dwState & 1) != 0;
    }


    bool IsUsableButton(
        Button* button)
    {
        return
            button != nullptr &&
            IsControlEnabled(button) &&
            button->OnPress != nullptr;
    }


    bool TextBoxContainsExactText(
        TextBox* textBox,
        const std::string& wantedText)
    {
        if (!textBox ||
            wantedText.empty())
        {
            return false;
        }


        ControlText* text =
            textBox->pFirstText;

        unsigned int guard =
            0;


        while (text &&
               guard < 50)
        {
            for (int field = 0;
                 field < 5;
                 ++field)
            {
                const std::string value =
                    WideToUtf8(
                        text->wText[field]
                    );


                if (value ==
                    wantedText)
                {
                    return true;
                }
            }


            text =
                text->pNext;

            ++guard;
        }


        return false;
    }


    Button* FindButtonByText(
        const char* wantedText)
    {
        if (!wantedText)
            return nullptr;


        Control* control =
            p_D2WIN_FirstControl
                ? *p_D2WIN_FirstControl
                : nullptr;

        unsigned int guard =
            0;


        while (control &&
               guard < 200)
        {
            if (control->dwType == 6)
            {
                Button* button =
                    reinterpret_cast<
                        Button*
                    >(control);


                const std::string text =
                    WideToUtf8(
                        button->wText
                    );


                if (text ==
                    wantedText)
                {
                    return button;
                }
            }


            control =
                control->pNext;

            ++guard;
        }


        return nullptr;
    }


    TextBox* FindCharacterEntryByName(
        const std::string& characterName)
    {
        Control* control =
            p_D2WIN_FirstControl
                ? *p_D2WIN_FirstControl
                : nullptr;

        unsigned int guard =
            0;


        while (control &&
               guard < 200)
        {
            if (control->dwType == 4 &&
                control->OnPress != nullptr)
            {
                TextBox* textBox =
                    reinterpret_cast<
                        TextBox*
                    >(control);


                if (TextBoxContainsExactText(
                        textBox,
                        characterName))
                {
                    return textBox;
                }
            }


            control =
                control->pNext;

            ++guard;
        }


        return nullptr;
    }


    bool IsCharacterSelected(
        const std::string& characterName)
    {
        Control* control =
            p_D2WIN_FirstControl
                ? *p_D2WIN_FirstControl
                : nullptr;

        unsigned int guard =
            0;


        while (control &&
               guard < 200)
        {
            //
            // The selected-character header is
            // a non-clickable TextBox.
            //
            if (control->dwType == 4 &&
                control->OnPress == nullptr)
            {
                TextBox* textBox =
                    reinterpret_cast<
                        TextBox*
                    >(control);


                if (TextBoxContainsExactText(
                        textBox,
                        characterName))
                {
                    return true;
                }
            }


            control =
                control->pNext;

            ++guard;
        }


        return false;
    }


    bool HasTextBoxWithText(
        const std::string& wantedText)
    {
        Control* control =
            p_D2WIN_FirstControl
                ? *p_D2WIN_FirstControl
                : nullptr;

        unsigned int guard =
            0;


        while (control &&
               guard < 200)
        {
            if (control->dwType == 4)
            {
                TextBox* textBox =
                    reinterpret_cast<
                        TextBox*
                    >(control);


                if (TextBoxContainsExactText(
                        textBox,
                        wantedText))
                {
                    return true;
                }
            }


            control =
                control->pNext;

            ++guard;
        }


        return false;
    }


    bool IsCharacterSelectionScreen()
    {
        return
            FindButtonByText("OK") !=
                nullptr &&
            FindButtonByText(
                "CREATE NEW"
            ) != nullptr;
    }


    bool IsDifficultySelectionScreen()
    {
        if (!HasTextBoxWithText(
                "SELECT DIFFICULTY"))
        {
            return false;
        }


        return
            FindButtonByText("NORMAL") !=
                nullptr &&
            FindButtonByText(
                "NIGHTMARE"
            ) != nullptr &&
            FindButtonByText("HELL") !=
                nullptr;
    }


    Button* FindHighestUnlockedDifficulty()
    {
        Button* hell =
            FindButtonByText(
                "HELL"
            );


        if (IsUsableButton(hell))
            return hell;


        Button* nightmare =
            FindButtonByText(
                "NIGHTMARE"
            );


        if (IsUsableButton(
                nightmare))
        {
            return nightmare;
        }


        Button* normal =
            FindButtonByText(
                "NORMAL"
            );


        if (IsUsableButton(normal))
            return normal;


        return nullptr;
    }


    Button* FindPreferredDifficulty(
        QuickResumeDifficultyMode mode,
        BYTE lastPlayedDifficulty)
    {
        Button* normal =
            FindButtonByText(
                "NORMAL"
            );

        Button* nightmare =
            FindButtonByText(
                "NIGHTMARE"
            );

        Button* hell =
            FindButtonByText(
                "HELL"
            );


        switch (mode)
        {
        case QuickResumeDifficultyMode::
            HighestUnlocked:
        {
            return
                FindHighestUnlockedDifficulty();
        }


        case QuickResumeDifficultyMode::
            LastPlayed:
        {
            if (lastPlayedDifficulty ==
                    DIFFICULTY_HELL &&
                IsUsableButton(hell))
            {
                return hell;
            }


            if (lastPlayedDifficulty ==
                    DIFFICULTY_NIGHTMARE &&
                IsUsableButton(
                    nightmare))
            {
                return nightmare;
            }


            if (lastPlayedDifficulty ==
                    DIFFICULTY_NORMAL &&
                IsUsableButton(normal))
            {
                return normal;
            }


            //
            // Recover safely if the previous
            // difficulty is no longer usable.
            //
            return
                FindHighestUnlockedDifficulty();
        }


        case QuickResumeDifficultyMode::
            Normal:
        {
            return
                IsUsableButton(normal)
                    ? normal
                    : nullptr;
        }


        case QuickResumeDifficultyMode::
            Nightmare:
        {
            if (IsUsableButton(
                    nightmare))
            {
                return nightmare;
            }


            return
                IsUsableButton(normal)
                    ? normal
                    : nullptr;
        }


        case QuickResumeDifficultyMode::
            Hell:
        {
            if (IsUsableButton(hell))
                return hell;


            if (IsUsableButton(
                    nightmare))
            {
                return nightmare;
            }


            return
                IsUsableButton(normal)
                    ? normal
                    : nullptr;
        }
        }


        return
            FindHighestUnlockedDifficulty();
    }
}


QuickResume::QuickResume()
    : Module("Quick Resume"),
      step(
          QuickResumeStep::Idle),
      available(false),
      crossWasDown(false),
      lastPlayedDifficulty(
          DIFFICULTY_NORMAL),
      stepStartedAt(0)
{
}


void QuickResume::CaptureGameState()
{
    UnitAny* player =
        D2CLIENT_GetPlayerUnit();


    if (!player)
        return;


    if (player->pPlayerData &&
        player->pPlayerData
            ->szName[0] != '\0')
    {
        characterName.assign(
            player->pPlayerData
                ->szName
        );
    }


    const BYTE difficulty =
        D2CLIENT_GetDifficulty();


    if (difficulty <=
        DIFFICULTY_HELL)
    {
        lastPlayedDifficulty =
            difficulty;
    }
}


void QuickResume::SyncCrossState()
{
    ControllerInput& input =
        ControllerInput::Get();


    if (!input.Poll())
    {
        crossWasDown =
            false;

        return;
    }


    crossWasDown =
        input.IsButtonDown(1);
}


bool QuickResume::PollCrossPressed()
{
    ControllerInput& input =
        ControllerInput::Get();


    if (!input.Poll())
    {
        crossWasDown =
            false;

        return false;
    }


    //
    // DualSense Cross:
    // DirectInput button 1.
    //
    const bool crossDown =
        input.IsButtonDown(1);


    const bool crossPressed =
        crossDown &&
        !crossWasDown;


    crossWasDown =
        crossDown;


    return crossPressed;
}


void QuickResume::SetStep(
    QuickResumeStep newStep)
{
    step =
        newStep;

    stepStartedAt =
        GetTickCount();
}


void QuickResume::Fail(
    const std::string& message)
{
    const std::string debugMessage =
        "[QuickResume] " +
        message +
        "\n";


    OutputDebugStringA(
        debugMessage.c_str()
    );


    SetStep(
        QuickResumeStep::Failed
    );
}


void QuickResume::AdvanceQuickResume()
{
    const DWORD elapsed =
        GetTickCount() -
        stepStartedAt;


    switch (step)
    {
    case QuickResumeStep::Idle:
    case QuickResumeStep::Ready:
    case QuickResumeStep::Failed:
        return;


    case QuickResumeStep::
        OpenSinglePlayer:
    {
        Button* singlePlayer =
            FindButtonByText(
                "SINGLE PLAYER"
            );


        if (!singlePlayer)
        {
            if (elapsed >
                STEP_TIMEOUT_MS)
            {
                Fail(
                    "SINGLE PLAYER "
                    "button not found"
                );
            }

            return;
        }


        if (!IsUsableButton(
                singlePlayer))
        {
            Fail(
                "SINGLE PLAYER "
                "button unavailable"
            );

            return;
        }


        //
        // Do not dereference the control again
        // after OnPress: D2Win may rebuild
        // the control list immediately.
        //
        singlePlayer->OnPress(
            singlePlayer
        );


        SetStep(
            QuickResumeStep::
                WaitCharacterSelect
        );

        return;
    }


    case QuickResumeStep::
        WaitCharacterSelect:
    {
        if (IsCharacterSelectionScreen())
        {
            SetStep(
                QuickResumeStep::
                    SelectCharacter
            );

            return;
        }


        if (elapsed >
            STEP_TIMEOUT_MS)
        {
            Fail(
                "Character Selection "
                "did not appear"
            );
        }

        return;
    }


    case QuickResumeStep::
        SelectCharacter:
    {
        TextBox* characterEntry =
            FindCharacterEntryByName(
                characterName
            );


        if (!characterEntry)
        {
            if (elapsed >
                STEP_TIMEOUT_MS)
            {
                Fail(
                    "Character not found: " +
                    characterName
                );
            }

            return;
        }


        if (!IsControlEnabled(
                characterEntry) ||
            !characterEntry->OnPress)
        {
            Fail(
                "Character entry "
                "unavailable"
            );

            return;
        }


        characterEntry->OnPress(
            characterEntry
        );


        SetStep(
            QuickResumeStep::
                WaitCharacterSelected
        );

        return;
    }


    case QuickResumeStep::
        WaitCharacterSelected:
    {
        if (IsCharacterSelected(
                characterName))
        {
            SetStep(
                QuickResumeStep::
                    PressCharacterOk
            );

            return;
        }


        if (elapsed >
            STEP_TIMEOUT_MS)
        {
            Fail(
                "Character selection "
                "was not confirmed"
            );
        }

        return;
    }


    case QuickResumeStep::
        PressCharacterOk:
    {
        Button* ok =
            FindButtonByText(
                "OK"
            );


        if (!ok)
        {
            if (elapsed >
                STEP_TIMEOUT_MS)
            {
                Fail(
                    "Character OK "
                    "button not found"
                );
            }

            return;
        }


        if (!IsUsableButton(ok))
        {
            Fail(
                "Character OK "
                "button unavailable"
            );

            return;
        }


        ok->OnPress(ok);


        SetStep(
            QuickResumeStep::
                WaitDifficultySelect
        );

        return;
    }


    case QuickResumeStep::
        WaitDifficultySelect:
    {
        if (IsDifficultySelectionScreen())
        {
            SetStep(
                QuickResumeStep::
                    SelectDifficulty
            );

            return;
        }


        if (elapsed >
            STEP_TIMEOUT_MS)
        {
            Fail(
                "Difficulty Selection "
                "did not appear"
            );
        }

        return;
    }


    case QuickResumeStep::
        SelectDifficulty:
    {
        int configuredMode =
            App.mods
                .quickResumeDifficulty
                .value;


        if (configuredMode < 0 ||
            configuredMode > 4)
        {
            configuredMode =
                0;
        }


        const auto mode =
            static_cast<
                QuickResumeDifficultyMode
            >(configuredMode);


        Button* selectedDifficulty =
            FindPreferredDifficulty(
                mode,
                lastPlayedDifficulty
            );


        if (!selectedDifficulty)
        {
            Fail(
                "No playable difficulty found"
            );

            return;
        }


        selectedDifficulty->OnPress(
            selectedDifficulty
        );


        SetStep(
            QuickResumeStep::
                WaitGameJoin
        );

        return;
    }


    case QuickResumeStep::
        WaitGameJoin:
    {
        if (elapsed >
            GAME_JOIN_TIMEOUT_MS)
        {
            Fail(
                "Game did not start"
            );
        }

        return;
    }
    }
}


void QuickResume::OnGameJoin()
{
    available =
        false;


    SetStep(
        QuickResumeStep::Idle
    );


    CaptureGameState();
    SyncCrossState();
}


void QuickResume::OnLoop()
{
    if (D2CLIENT_GetPlayerUnit())
    {
        CaptureGameState();
    }
}


void QuickResume::OnGameExit()
{
    if (!App.mods
            .quickResumeEnabled
            .value)
    {
        available =
            false;

        return;
    }


    if (characterName.empty())
    {
        available =
            false;

        return;
    }


    available =
        true;


    SetStep(
        QuickResumeStep::Ready
    );


    SyncCrossState();
}


void QuickResume::OnOOGDraw()
{
    if (!App.mods
            .quickResumeEnabled
            .value)
    {
        available =
            false;

        return;
    }


    if (!available)
        return;


    if (D2CLIENT_GetPlayerUnit())
        return;


    if (PollCrossPressed() &&
        (step ==
            QuickResumeStep::Ready ||
         step ==
            QuickResumeStep::Failed))
    {
        SetStep(
            QuickResumeStep::
                OpenSinglePlayer
        );
    }


    AdvanceQuickResume();
}