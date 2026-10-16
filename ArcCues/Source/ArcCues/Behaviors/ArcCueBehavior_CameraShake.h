// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "ArcCueBehavior.h"

#include "ArcCueBehavior_CameraShake.generated.h"

class UCameraShakeBase;

// ---------------------------------------------------------------------------
// FArcCueBehavior_CameraShake
//
// One-shot. Plays a camera shake on the local player controller. No-op on
// dedicated server. v1 targets the first local PC; broaden to all locals when
// split-screen is in scope.
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType, meta=(DisplayName="Camera Shake"))
struct ARCCUES_API FArcCueBehavior_CameraShake : public FArcCueBehavior
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="CameraShake")
	TSubclassOf<UCameraShakeBase> ShakeClass;

	UPROPERTY(EditAnywhere, Category="CameraShake")
	float Scale = 1.0f;

	virtual void OnExecute(const FArcCueContext& Context) const override;
};
