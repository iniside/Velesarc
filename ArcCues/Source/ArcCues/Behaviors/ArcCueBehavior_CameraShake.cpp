// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCueBehavior_CameraShake.h"

#include "Camera/CameraShakeBase.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcCueBehavior_CameraShake)

void FArcCueBehavior_CameraShake::OnExecute(const FArcCueContext& Context) const
{
	UWorld* World = Context.World;
	if (World == nullptr)
	{
		return;
	}

	if (World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (ShakeClass == nullptr)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (PC == nullptr)
	{
		return;
	}

	PC->ClientStartCameraShake(ShakeClass, Scale);
}
