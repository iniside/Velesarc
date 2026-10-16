// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "NativeGameplayTags.h"
#include "ArcTabTargetingMessages.generated.h"

UENUM(BlueprintType)
enum class EArcTabTargetChangeReason : uint8
{
	Selected,
	Deselected,
	TargetDestroyed,
	TargetDead,
	OutOfRange,
	InvalidTarget
};

USTRUCT(BlueprintType)
struct ARCABILITYKIT_API FArcTabTargetChangedMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	FMassEntityHandle SourceEntity;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	FMassEntityHandle PreviousTarget;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	FMassEntityHandle NewTarget;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	EArcTabTargetChangeReason Reason = EArcTabTargetChangeReason::Selected;
};

namespace Arcx::TargetingMessages
{
	ARCABILITYKIT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TargetChanged);
}
