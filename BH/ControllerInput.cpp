#include "ControllerInput.h"

#include "D2Ptrs.h"

#include <cstring>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace
{
    struct EnumContext
    {
        LPDIRECTINPUT8 directInput;
        LPDIRECTINPUTDEVICE8* outputDevice;
    };


    BOOL CALLBACK FindFirstControllerCallback(
        const DIDEVICEINSTANCE* instance,
        VOID* context)
    {
        EnumContext* enumContext =
            reinterpret_cast<EnumContext*>(
                context
            );

        if (!enumContext ||
            !enumContext->directInput ||
            !enumContext->outputDevice)
        {
            return DIENUM_STOP;
        }

        LPDIRECTINPUTDEVICE8 device =
            nullptr;

        HRESULT hr =
            enumContext->directInput
                ->CreateDevice(
                    instance->guidInstance,
                    &device,
                    nullptr
                );

        if (FAILED(hr) ||
            !device)
        {
            return DIENUM_CONTINUE;
        }

        *enumContext->outputDevice =
            device;

        return DIENUM_STOP;
    }
}


ControllerInput::ControllerInput()
    : directInput(nullptr),
      controller(nullptr)
{
    std::memset(
        &state,
        0,
        sizeof(state)
    );

    state.rgdwPOV[0] =
        0xFFFFFFFF;
}


ControllerInput::~ControllerInput()
{
    Shutdown();
}


ControllerInput& ControllerInput::Get()
{
    static ControllerInput instance;

    return instance;
}


bool ControllerInput::Initialize()
{
    if (controller)
        return true;

    if (!directInput)
    {
        HRESULT hr =
            DirectInput8Create(
                GetModuleHandle(nullptr),
                DIRECTINPUT_VERSION,
                IID_IDirectInput8,
                reinterpret_cast<void**>(
                    &directInput
                ),
                nullptr
            );

        if (FAILED(hr) ||
            !directInput)
        {
            directInput = nullptr;

            return false;
        }
    }


    EnumContext context = {};

    context.directInput =
        directInput;

    context.outputDevice =
        &controller;

    HRESULT hr =
        directInput->EnumDevices(
            DI8DEVCLASS_GAMECTRL,
            FindFirstControllerCallback,
            &context,
            DIEDFL_ATTACHEDONLY
        );

    if (FAILED(hr) ||
        !controller)
    {
        return false;
    }


    hr =
        controller->SetDataFormat(
            &c_dfDIJoystick2
        );

    if (FAILED(hr))
    {
        controller->Release();
        controller = nullptr;

        return false;
    }


    HWND gameWindow =
        D2GFX_GetHwnd();

    if (!gameWindow)
    {
        controller->Release();
        controller = nullptr;

        return false;
    }


    hr =
        controller->SetCooperativeLevel(
            gameWindow,
            DISCL_BACKGROUND |
            DISCL_NONEXCLUSIVE
        );

    if (FAILED(hr))
    {
        controller->Release();
        controller = nullptr;

        return false;
    }


    controller->Acquire();

    return true;
}


void ControllerInput::Shutdown()
{
    if (controller)
    {
        controller->Unacquire();
        controller->Release();

        controller = nullptr;
    }

    if (directInput)
    {
        directInput->Release();

        directInput = nullptr;
    }
}


bool ControllerInput::Poll()
{
    if (!controller &&
        !Initialize())
    {
        return false;
    }


    HRESULT hr =
        controller->Poll();

    if (FAILED(hr))
    {
        hr =
            controller->Acquire();

        while (hr ==
               DIERR_INPUTLOST)
        {
            hr =
                controller->Acquire();
        }

        if (FAILED(hr))
            return false;

        hr =
            controller->Poll();

        if (FAILED(hr))
            return false;
    }


    DIJOYSTATE2 newState = {};

    hr =
        controller->GetDeviceState(
            sizeof(newState),
            &newState
        );

    if (FAILED(hr))
        return false;


    state =
        newState;

    return true;
}


bool ControllerInput::IsButtonDown(
    std::size_t index) const
{
    if (index >=
        sizeof(state.rgbButtons))
    {
        return false;
    }

    return
        (state.rgbButtons[index] &
         0x80) != 0;
}


DWORD ControllerInput::GetPov(
    std::size_t index) const
{
    if (index >= 4)
        return 0xFFFFFFFF;

    return
        state.rgdwPOV[index];
}