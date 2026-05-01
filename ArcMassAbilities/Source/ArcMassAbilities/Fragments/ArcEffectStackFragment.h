// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Mass/EntityHandle.h"
#include "Effects/ArcEffectTypes.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Attributes/ArcAggregator.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"
#include "ArcEffectStackFragment.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcActiveEffect
{
	GENERATED_BODY()

	UPROPERTY()
	FSharedStruct Spec;

	float RemainingDuration = 0.f;
	float PeriodTimer = 0.f;

	TArray<FArcModifierHandle> ModifierHandles;

	TArray<FInstancedStruct> Tasks;

	bool bInhibited = false;

	FArcActiveEffectHandle Handle;

	FMassEntityHandle OwnerEntity;

	UPROPERTY()
	TObjectPtr<UWorld> OwnerWorld = nullptr;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectStackFragment : public FMassFragment
{
	GENERATED_BODY()

	TArray<FArcActiveEffect> ActiveEffects;
};

template<>
struct TMassFragmentTraits<FArcEffectStackFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
