#pragma once

#include <Windows.h>
#include <dinput.h>

#include <cstddef>

class ControllerInput
{
private:
    LPDIRECTINPUT8 directInput;
    LPDIRECTINPUTDEVICE8 controller;

    DIJOYSTATE2 state;

    ControllerInput();
    ~ControllerInput();

    bool Initialize();
    void Shutdown();

public:
    ControllerInput(
        const ControllerInput&) = delete;

    ControllerInput& operator=(
        const ControllerInput&) = delete;

    static ControllerInput& Get();

    bool Poll();

    bool IsButtonDown(
        std::size_t index) const;

    DWORD GetPov(
        std::size_t index = 0) const;
};