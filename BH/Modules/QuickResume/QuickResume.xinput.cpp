#include "QuickResume.h"

#include "../../BH.h"
#include "../../D2Ptrs.h"
#include "../../Drawing.h"

#include <Xinput.h>

#pragma comment(lib, "Xinput9_1_0.lib")

using namespace Drawing;

QuickResume::QuickResume()
    : Module("Quick Resume"),
      step(QuickResumeStep::Idle),
      available(false),
      xWasDown(false),
      xPressCount(0)
{
    for (int i = 0; i < 4; ++i)
    {
        controllerResults[i] = ERROR_DEVICE_NOT_CONNECTED;
        controllerButtons[i] = 0;
    }
}

void QuickResume::CaptureCharacter()
{
    UnitAny* player = D2CLIENT_GetPlayerUnit();

    if (!player)
        return;

    if (!player->pPlayerData)
        return;

    if (player->pPlayerData->szName[0] == '\0')
        return;

    characterName.assign(player->pPlayerData->szName);
}

void QuickResume::SampleControllers()
{
    for (DWORD i = 0; i < 4; ++i)
    {
        XINPUT_STATE state = {};

        controllerResults[i] =
            XInputGetState(i, &state);

        if (controllerResults[i] == ERROR_SUCCESS)
        {
            controllerButtons[i] =
                state.Gamepad.wButtons;
        }
        else
        {
            controllerButtons[i] = 0;
        }
    }
}

bool QuickResume::PollXPressed()
{
    SampleControllers();

    bool xDown = false;

    for (int i = 0; i < 4; ++i)
    {
        if (controllerResults[i] != ERROR_SUCCESS)
            continue;

        //
        // Xbox controller:
        // XINPUT_GAMEPAD_X = left face button.
        //
        // We deliberately inspect all four XInput slots.
        //
        if ((controllerButtons[i] & XINPUT_GAMEPAD_X) != 0)
        {
            xDown = true;
            break;
        }
    }

    const bool xPressed =
        xDown && !xWasDown;

    xWasDown = xDown;

    return xPressed;
}

void QuickResume::OnGameJoin()
{
    available = false;
    step = QuickResumeStep::Idle;
    xPressCount = 0;

    CaptureCharacter();

    SampleControllers();

    xWasDown = false;

    for (int i = 0; i < 4; ++i)
    {
        if (controllerResults[i] == ERROR_SUCCESS &&
            (controllerButtons[i] & XINPUT_GAMEPAD_X) != 0)
        {
            xWasDown = true;
        }
    }
}

void QuickResume::OnLoop()
{
    if (D2CLIENT_GetPlayerUnit())
        CaptureCharacter();
}

void QuickResume::OnGameExit()
{
    if (characterName.empty())
        return;

    available = true;
    step = QuickResumeStep::Ready;

    SampleControllers();

    xWasDown = false;

    for (int i = 0; i < 4; ++i)
    {
        if (controllerResults[i] == ERROR_SUCCESS &&
            (controllerButtons[i] & XINPUT_GAMEPAD_X) != 0)
        {
            xWasDown = true;
        }
    }
}

void QuickResume::OnOOGDraw()
{
    if (!available)
        return;

    if (D2CLIENT_GetPlayerUnit())
        return;

    if (PollXPressed())
    {
        ++xPressCount;
        step = QuickResumeStep::Triggered;
    }

    const char* stateText = "UNKNOWN";

    switch (step)
    {
    case QuickResumeStep::Idle:
        stateText = "IDLE";
        break;

    case QuickResumeStep::Ready:
        stateText = "READY";
        break;

    case QuickResumeStep::Triggered:
        stateText = "X DETECTED";
        break;
    }

    Texthook::Draw(
        20,
        30,
        None,
        0,
        Gold,
        "Quick Resume: %s",
        characterName.c_str()
    );

    Texthook::Draw(
        20,
        46,
        None,
        0,
        White,
        "State: %s | X presses: %u",
        stateText,
        xPressCount
    );

    Texthook::Draw(
        20,
        68,
        None,
        0,
        White,
        "XI0 result=%lu buttons=%04X",
        controllerResults[0],
        controllerButtons[0]
    );

    Texthook::Draw(
        20,
        84,
        None,
        0,
        White,
        "XI1 result=%lu buttons=%04X",
        controllerResults[1],
        controllerButtons[1]
    );

    Texthook::Draw(
        20,
        100,
        None,
        0,
        White,
        "XI2 result=%lu buttons=%04X",
        controllerResults[2],
        controllerButtons[2]
    );

    Texthook::Draw(
        20,
        116,
        None,
        0,
        White,
        "XI3 result=%lu buttons=%04X",
        controllerResults[3],
        controllerButtons[3]
    );
}