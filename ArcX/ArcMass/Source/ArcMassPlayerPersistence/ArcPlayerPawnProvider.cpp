// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcPlayerPawnProvider.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

UE_ARC_IMPLEMENT_PLAYER_PROVIDER(
	APawn,
	Pawn,
	nullptr,
	[](UObject* Parent) -> UObject* {
		APlayerController* PC = Cast<APlayerController>(Parent);
		return PC ? PC->GetPawn() : nullptr;
	},
	false
);
