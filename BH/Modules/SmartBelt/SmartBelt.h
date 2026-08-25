#pragma once

#include "../Module.h"

struct UnitAny;

enum class SmartBeltPotionType
{
    Healing,
    Mana,
    Rejuvenation
};

class SmartBelt : public Module
{
private:
    DWORD lastControllerPov;

    bool beltStateInitialized;
    bool beltColumnOccupied[4];

    int healthInventoryCount;
    int manaInventoryCount;
    int rejuvInventoryCount;


    UnitAny* FindInventoryPotion(
        UnitAny* player,
        SmartBeltPotionType type
    );

    bool UseInventoryItem(
        UnitAny* player,
        UnitAny* item
    );

    bool TryUseInventoryPotion(
        SmartBeltPotionType type
    );

    void PlayPotionUseSound(
        UnitAny* player,
        int soundId
    );

    void RefreshInventoryState();


public:
    SmartBelt();

    void OnLoop() override;
    void OnDraw() override;
};