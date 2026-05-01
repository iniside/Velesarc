// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Attributes/ArcAttribute.h"
#include "ArcEffectStateTreeEvents.generated.h"

class UArcEffectDefinition;
class FNativeGameplayTag;

// ---------------------------------------------------------------------------
// Gameplay tags sent as StateTree events
// ---------------------------------------------------------------------------

namespace ArcEffectStateTreeTags
{
	extern ARCMASSABILITIES_API FNativeGameplayTag AttributeChanged;
	extern ARCMASSABILITIES_API FNativeGameplayTag EffectApplied;
}

// ---------------------------------------------------------------------------
// Event payload structs
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcEffectEvent_AttributeChanged
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	FArcAttributeRef Attribute;

	UPROPERTY(VisibleAnywhere)
	float OldValue = 0.f;

	UPROPERTY(VisibleAnywhere)
	float NewValue = 0.f;
};

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcEffectEvent_EffectApplied
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UArcEffectDefinition> Effect = nullptr;

	UPROPERTY(VisibleAnywhere)
	FMassEntityHandle SourceEntity;
};
