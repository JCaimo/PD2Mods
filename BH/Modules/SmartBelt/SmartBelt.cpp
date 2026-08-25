#include "SmartBelt.h"

#include "../../BH.h"
#include "../../Constants.h"
#include "../../ControllerInput.h"
#include "../../D2DataTables.h"
#include "../../D2Ptrs.h"
#include "../../D2Structs.h"
#include "../../Drawing/Basic/Texthook/Texthook.h"

#include <climits>
#include <cstdio>
#include <cstring>


namespace
{
    constexpr DWORD POV_CENTERED =
        0xFFFFFFFF;

    constexpr DWORD POV_BELT_1 =
        27000; // DualSense D-pad Left

    constexpr DWORD POV_BELT_2 =
        0; // DualSense D-pad Up

    constexpr DWORD POV_BELT_3 =
        18000; // DualSense D-pad Down

    constexpr DWORD POV_BELT_4 =
        9000; // DualSense D-pad Right


    //
    // Controller belt HUD positioning.
    //
    constexpr int BELT_FIRST_X_OFFSET =
        40;

    constexpr int BELT_SLOT_STEP =
        30;

    constexpr int BELT_COUNT_Y_OFFSET =
        22;


    //
    // Returns a priority for the potion.
    //
    // Smaller number = consume first.
    //
    // This follows the historical BH behavior
    // of consuming weaker potions before
    // stronger ones.
    //
    int GetPotionPriority(
        SmartBeltPotionType type,
        const char* code)
    {
        if (!code)
            return -1;


        switch (type)
        {
        case SmartBeltPotionType::Healing:
        {
            //
            // hp1 .. hp5
            //
            if (code[0] == 'h' &&
                code[1] == 'p' &&
                code[2] >= '1' &&
                code[2] <= '5')
            {
                return
                    code[2] - '0';
            }

            break;
        }


        case SmartBeltPotionType::Mana:
        {
            //
            // mp1 .. mp5
            //
            if (code[0] == 'm' &&
                code[1] == 'p' &&
                code[2] >= '1' &&
                code[2] <= '5')
            {
                return
                    code[2] - '0';
            }

            break;
        }


        case SmartBeltPotionType::Rejuvenation:
        {
            //
            // rvs = Rejuvenation Potion
            // rvl = Full Rejuvenation Potion
            //
            // Regular Rejuvenation is consumed
            // before Full Rejuvenation.
            //
            if (code[0] == 'r' &&
                code[1] == 'v')
            {
                if (code[2] == 's')
                    return 1;

                if (code[2] == 'l')
                    return 2;
            }

            break;
        }
        }


        return -1;
    }


    int BeltColumnFromPov(
        DWORD pov)
    {
        switch (pov)
        {
        case POV_BELT_1:
            return 0;

        case POV_BELT_2:
            return 1;

        case POV_BELT_3:
            return 2;

        case POV_BELT_4:
            return 3;

        default:
            return -1;
        }
    }
}


SmartBelt::SmartBelt()
    : Module("Smart Belt"),
      lastControllerPov(
          POV_CENTERED),
      beltStateInitialized(
          false),
      healthInventoryCount(
          0),
      manaInventoryCount(
          0),
      rejuvInventoryCount(
          0)
{
    std::memset(
        beltColumnOccupied,
        0,
        sizeof(beltColumnOccupied)
    );
}


UnitAny* SmartBelt::FindInventoryPotion(
    UnitAny* player,
    SmartBeltPotionType type)
{
    if (!player ||
        !player->pInventory)
    {
        return nullptr;
    }


    UnitAny* selectedPotion =
        nullptr;

    int selectedPriority =
        INT_MAX;


    UnitAny* item =
        D2COMMON_GetItemFromInventory(
            player->pInventory
        );


    while (item)
    {
        if (item->pItemData &&
            item->pItemData->ItemLocation ==
                STORAGE_INVENTORY)
        {
            ItemsTxt* itemText =
                D2COMMON_GetItemText(
                    item->dwTxtFileNo
                );

            if (itemText)
            {
                const int priority =
                    GetPotionPriority(
                        type,
                        itemText->szCode
                    );

                if (priority >= 0 &&
                    priority <
                        selectedPriority)
                {
                    selectedPriority =
                        priority;

                    selectedPotion =
                        item;
                }
            }
        }


        item =
            D2COMMON_GetNextItemFromInventory(
                item
            );
    }


    return selectedPotion;
}


bool SmartBelt::UseInventoryItem(
    UnitAny* player,
    UnitAny* item)
{
    if (!player ||
        !item ||
        !player->pPath)
    {
        return false;
    }


    int useSoundId =
        0;


    ItemsTxt* itemText =
        D2COMMON_GetItemText(
            item->dwTxtFileNo
        );


    if (itemText)
    {
        useSoundId =
            static_cast<int>(
                itemText->wusesound
            );
    }


    BYTE packet[13] = {
        0x20,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
    };


    const DWORD itemId =
        item->dwUnitId;

    const WORD playerX =
        static_cast<WORD>(
            player->pPath->xPos
        );

    const WORD playerY =
        static_cast<WORD>(
            player->pPath->yPos
        );


    std::memcpy(
        packet + 1,
        &itemId,
        sizeof(itemId)
    );

    std::memcpy(
        packet + 5,
        &playerX,
        sizeof(playerX)
    );

    std::memcpy(
        packet + 9,
        &playerY,
        sizeof(playerY)
    );


    D2NET_SendPacket(
        sizeof(packet),
        0,
        packet
    );


    //
    // Packet 0x20 performs the actual use,
    // but bypasses the normal local potion SFX.
    //
    if (useSoundId > 0)
    {
        PlayPotionUseSound(
            player,
            useSoundId
        );
    }


    return true;
}


bool SmartBelt::TryUseInventoryPotion(
    SmartBeltPotionType type)
{
    UnitAny* player =
        D2CLIENT_GetPlayerUnit();


    if (!player ||
        !player->pInventory ||
        !player->pPath)
    {
        return false;
    }


    UnitAny* potion =
        FindInventoryPotion(
            player,
            type
        );


    if (!potion)
        return false;


    return
        UseInventoryItem(
            player,
            potion
        );
}


void SmartBelt::PlayPotionUseSound(
    UnitAny* player,
    int soundId)
{
    if (!player ||
        soundId <= 0 ||
        !App.pd2.pd2PlaySoundImpl)
    {
        return;
    }


    if (!p_D2CLIENT_SoundsTxt ||
        !p_D2CLIENT_SoundRecords)
    {
        return;
    }


    SoundsTxt* sounds =
        *p_D2CLIENT_SoundsTxt;

    const DWORD soundCount =
        *p_D2CLIENT_SoundRecords;


    if (!sounds ||
        static_cast<DWORD>(
            soundId
        ) >= soundCount)
    {
        return;
    }


    SoundsTxt* sound =
        &sounds[soundId];


    //
    // Potion use sounds should be one-shot.
    //
    if (sound->loop != 0)
        return;


    App.pd2.pd2PlaySoundImpl(
        player,
        soundId,
        sound->volume,
        255,
        FALSE
    );
}


void SmartBelt::RefreshInventoryState()
{
    //
    // One inventory traversal updates everything
    // SmartBelt needs for both gameplay and HUD.
    //
    std::memset(
        beltColumnOccupied,
        0,
        sizeof(beltColumnOccupied)
    );

    healthInventoryCount =
        0;

    manaInventoryCount =
        0;

    rejuvInventoryCount =
        0;


    UnitAny* player =
        D2CLIENT_GetPlayerUnit();


    if (!player ||
        !player->pInventory)
    {
        return;
    }


    UnitAny* item =
        D2COMMON_GetItemFromInventory(
            player->pInventory
        );


    while (item)
    {
        if (item->pItemData)
        {
            //
            // Physical belt item.
            //
            if (item->pItemPath &&
                item->dwMode ==
                    ITEM_MODE_IN_BELT &&
                item->pItemData->NodePage ==
                    NODEPAGE_BELTSLOTS)
            {
                const DWORD beltSlot =
                    item->pItemPath->dwPosX;

                const int column =
                    static_cast<int>(
                        beltSlot % 4
                    );


                if (column >= 0 &&
                    column < 4)
                {
                    beltColumnOccupied[
                        column
                    ] = true;
                }
            }


            //
            // Backpack item.
            //
            if (item->pItemData->ItemLocation ==
                STORAGE_INVENTORY)
            {
                ItemsTxt* itemText =
                    D2COMMON_GetItemText(
                        item->dwTxtFileNo
                    );


                if (itemText)
                {
                    const char* code =
                        itemText->szCode;


                    if (GetPotionPriority(
                            SmartBeltPotionType::
                                Healing,
                            code) >= 0)
                    {
                        ++healthInventoryCount;
                    }
                    else if (
                        GetPotionPriority(
                            SmartBeltPotionType::
                                Mana,
                            code) >= 0)
                    {
                        ++manaInventoryCount;
                    }
                    else if (
                        GetPotionPriority(
                            SmartBeltPotionType::
                                Rejuvenation,
                            code) >= 0)
                    {
                        ++rejuvInventoryCount;
                    }
                }
            }
        }


        item =
            D2COMMON_GetNextItemFromInventory(
                item
            );
    }
}


void SmartBelt::OnLoop()
{
    UnitAny* player =
        D2CLIENT_GetPlayerUnit();


    if (!player ||
        !player->pInventory)
    {
        beltStateInitialized =
            false;

        lastControllerPov =
            POV_CENTERED;

        std::memset(
            beltColumnOccupied,
            0,
            sizeof(beltColumnOccupied)
        );

        healthInventoryCount =
            0;

        manaInventoryCount =
            0;

        rejuvInventoryCount =
            0;

        return;
    }


    ControllerInput& input =
        ControllerInput::Get();


    if (!input.Poll())
    {
        //
        // Keep HUD/inventory data current even
        // if the controller is temporarily lost.
        //
        RefreshInventoryState();

        beltStateInitialized =
            false;

        lastControllerPov =
            POV_CENTERED;

        return;
    }


    const DWORD currentPov =
        input.GetPov();


    //
    // First sample establishes the belt snapshot
    // and controller baseline.
    //
    if (!beltStateInitialized)
    {
        RefreshInventoryState();

        lastControllerPov =
            currentPov;

        beltStateInitialized =
            true;

        return;
    }


    //
    // Detect a new D-pad direction.
    //
    const bool povPressed =
        currentPov !=
            POV_CENTERED &&
        currentPov !=
            lastControllerPov;


    if (povPressed)
    {
        const int column =
            BeltColumnFromPov(
                currentPov
            );


        //
        // IMPORTANT:
        //
        // beltColumnOccupied is the snapshot from
        // the previous loop. This prevents consuming
        // an inventory potion in the same press that
        // consumed the last physical belt potion.
        //
        if (column >= 0 &&
            column <= 2 &&
            !beltColumnOccupied[column])
        {
            switch (column)
            {
            case 0:
                TryUseInventoryPotion(
                    SmartBeltPotionType::
                        Healing
                );
                break;

            case 1:
                TryUseInventoryPotion(
                    SmartBeltPotionType::
                        Mana
                );
                break;

            case 2:
                TryUseInventoryPotion(
                    SmartBeltPotionType::
                        Rejuvenation
                );
                break;
            }
        }
    }


    lastControllerPov =
        currentPov;


    //
    // This becomes the snapshot used by
    // the next controller press and by OnDraw().
    //
    RefreshInventoryState();
}


void SmartBelt::OnDraw()
{
    UnitAny* player =
        D2CLIENT_GetPlayerUnit();


    if (!player ||
        !player->pInventory)
    {
        return;
    }


    if (!p_D2CLIENT_ScreenSizeX ||
        !p_D2CLIENT_ScreenSizeY)
    {
        return;
    }


    const int centerX =
        static_cast<int>(
            *p_D2CLIENT_ScreenSizeX /
            2
        );

    const int screenY =
        static_cast<int>(
            *p_D2CLIENT_ScreenSizeY
        );


    const int xHealth =
        centerX +
        BELT_FIRST_X_OFFSET;

    const int xMana =
        xHealth +
        BELT_SLOT_STEP;

    const int xRejuv =
        xMana +
        BELT_SLOT_STEP;

    const int y =
        screenY -
        BELT_COUNT_Y_OFFSET;


    char text[16] = {};


    //
    // Pocket 1:
    // Health inventory fallback.
    //
    if (!beltColumnOccupied[0] &&
        healthInventoryCount > 0)
    {
        sprintf_s(
            text,
            "%d",
            healthInventoryCount
        );

        Drawing::Texthook::Draw(
            xHealth,
            y,
            true,
            0,
            Red,
            text
        );
    }


    //
    // Pocket 2:
    // Mana inventory fallback.
    //
    if (!beltColumnOccupied[1] &&
        manaInventoryCount > 0)
    {
        sprintf_s(
            text,
            "%d",
            manaInventoryCount
        );

        Drawing::Texthook::Draw(
            xMana,
            y,
            true,
            0,
            Blue,
            text
        );
    }


    //
    // Pocket 3:
    // Rejuvenation inventory fallback.
    //
    if (!beltColumnOccupied[2] &&
        rejuvInventoryCount > 0)
    {
        sprintf_s(
            text,
            "%d",
            rejuvInventoryCount
        );

        Drawing::Texthook::Draw(
            xRejuv,
            y,
            true,
            0,
            Purple,
            text
        );
    }
}