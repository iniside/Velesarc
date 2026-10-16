// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTaskHelpers.h"
#include "Mass/EntityManager.h"
#include "MassActorSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

namespace ArcAbilityKit
{
    ULocalPlayer* GetLocalPlayerForEntity(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
    {
        FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
        if (!ActorFrag)
        {
            return nullptr;
        }

        AActor* Actor = ActorFrag->GetMutable();
        if (!Actor)
        {
            return nullptr;
        }

        APawn* Pawn = Cast<APawn>(Actor);
        if (!Pawn)
        {
            return nullptr;
        }

        APlayerController* PC = Pawn->GetController<APlayerController>();
        if (!PC)
        {
            return nullptr;
        }

        return PC->GetLocalPlayer();
    }
}
