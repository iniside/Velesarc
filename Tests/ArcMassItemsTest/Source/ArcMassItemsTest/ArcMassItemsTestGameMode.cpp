// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassItemsTestGameMode.h"

#include "GameFramework/SpectatorPawn.h"
#include "Player/ArcMassPlayerController.h"
#include "Player/ArcMassPlayerState.h"

AArcMassItemsTestGameMode::AArcMassItemsTestGameMode()
{
	PlayerControllerClass = AArcMassPlayerController::StaticClass();
	PlayerStateClass = AArcMassPlayerState::StaticClass();
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}
