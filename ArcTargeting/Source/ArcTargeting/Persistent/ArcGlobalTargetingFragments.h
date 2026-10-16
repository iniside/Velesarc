#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "ArcGlobalTargetingFragments.generated.h"

class UArcTargetingPreset;

USTRUCT(BlueprintType)
struct ARCTARGETING_API FArcGlobalTargetingConfigEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targeting")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targeting")
	TObjectPtr<UArcTargetingPreset> Preset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0"))
	float MinInterval = 0.f;
};

USTRUCT()
struct ARCTARGETING_API FArcGlobalTargetingConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcGlobalTargetingConfigEntry> Entries;
};

USTRUCT()
struct ARCTARGETING_API FArcGlobalTargetingRegistrationTag : public FMassTag
{
	GENERATED_BODY()
};

template<>
struct TMassFragmentTraits<FArcGlobalTargetingConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
