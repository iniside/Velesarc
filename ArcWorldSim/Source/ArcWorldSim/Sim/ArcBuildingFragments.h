// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcNamedPrimaryAssetId.h"
#include "GameplayTagContainer.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "Mass/EntityElementTypes.h"
#include "MassLogging.h"

#include "ArcBuildingFragments.generated.h"

USTRUCT()
struct ARCWORLDSIM_API FArcStaffedFragment : public FMassFragment
{
	GENERATED_BODY()

	int32 StaffCount = 0;
};

USTRUCT()
struct ARCWORLDSIM_API FArcEmploymentCountFragment : public FMassFragment
{
	GENERATED_BODY()

	int32 EmploymentCount = 0;
};

USTRUCT()
struct ARCWORLDSIM_API FArcBuildingTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct ARCWORLDSIM_API FArcWorldSimLabelFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "WorldSim")
	FString DisplayName;
};

template<>
struct TMassFragmentTraits<FArcWorldSimLabelFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT(BlueprintType, meta = (ExcludeBaseStruct))
struct ARCWORLDSIM_API FArcBuildingFunctionConfig : public FMassConstSharedFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCWORLDSIM_API FArcProductionConfigFragment : public FArcBuildingFunctionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Production", meta = (AllowedTypes = "ArcRecipe"))
	TArray<FArcNamedPrimaryAssetId> Recipes;

	UPROPERTY(EditAnywhere, Category = "Production", meta = (ClampMin = "1"))
	int32 InputCapacity = 20;

	UPROPERTY(EditAnywhere, Category = "Production", meta = (ClampMin = "1"))
	int32 OutputCapacity = 20;
};

template<>
struct TMassFragmentTraits<FArcProductionConfigFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

UENUM()
enum class EArcProductionState : uint8 { Unstaffed, Starved, Producing, Blocked };

USTRUCT()
struct ARCWORLDSIM_API FArcProductionSlot
{
	GENERATED_BODY()

	// GC does not walk per-entity fragments (EntityManager.cpp:143-151); index Config.Recipes instead of TObjectPtr.
	int32 RecipeIndex = INDEX_NONE;
	int32 RequiredWorkerCount = 1;
	float Progress = 0.0f;
	float HaltDuration = 0.0f;
	EArcProductionState State = EArcProductionState::Unstaffed;
};

USTRUCT()
struct ARCWORLDSIM_API FArcProductionSlotsFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcProductionSlot> Slots;

	bool bProductionEnabled = true;
};

template<>
struct TMassFragmentTraits<FArcProductionSlotsFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT(BlueprintType)
struct ARCWORLDSIM_API FArcGatheringConfigFragment : public FArcBuildingFunctionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Gathering")
	FGameplayTag AreaTag;

	UPROPERTY(EditAnywhere, Category = "Gathering", meta = (ClampMin = "1"))
	int32 OutputCapacity = 20;
};

template<>
struct TMassFragmentTraits<FArcGatheringConfigFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

// Gathering has no inputs or recipes, so there is no Starved state.
UENUM()
enum class EArcGatheringState : uint8 { Unstaffed, Gathering, Blocked };

USTRUCT()
struct ARCWORLDSIM_API FArcGatheringStateFragment : public FMassFragment
{
	GENERATED_BODY()

	EArcGatheringState State = EArcGatheringState::Unstaffed;
};

USTRUCT(BlueprintType)
struct ARCWORLDSIM_API FArcItemSourceConfigFragment : public FArcBuildingFunctionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Item Source")
	FArcNamedPrimaryAssetId ItemDefinition;

	UPROPERTY(EditAnywhere, Category = "Item Source", meta = (ClampMin = "1"))
	int32 AmountPerInteraction = 1;
};

template<>
struct TMassFragmentTraits<FArcItemSourceConfigFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT(BlueprintType)
struct ARCWORLDSIM_API FArcStorageConfigFragment : public FArcBuildingFunctionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Storage", meta = (ClampMin = "1"))
	int32 Capacity = 9999999;
};

USTRUCT(BlueprintType)
struct ARCWORLDSIM_API FArcItemConsumerConfigFragment : public FArcBuildingFunctionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Item Consumer")
	TArray<FArcNamedPrimaryAssetId> WantedItems;

	UPROPERTY(EditAnywhere, Category = "Item Consumer", meta = (ClampMin = "1"))
	int32 InputCapacity = 20;
};

template<>
struct TMassFragmentTraits<FArcItemConsumerConfigFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT(BlueprintType)
struct ARCWORLDSIM_API FArcServiceConfigFragment : public FArcBuildingFunctionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Service")
	FGameplayTagContainer SlotTags;
};

template<>
struct TMassFragmentTraits<FArcServiceConfigFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT(BlueprintType)
struct ARCWORLDSIM_API FArcStaffingConfigFragment : public FArcBuildingFunctionConfig
{
	GENERATED_BODY()
};

USTRUCT()
struct ARCWORLDSIM_API FArcGatherAreaConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Gathering")
	FGameplayTag AreaTag;

	UPROPERTY(EditAnywhere, Category = "Gathering")
	FArcNamedPrimaryAssetId ItemDefinition;

	UPROPERTY(EditAnywhere, Category = "Gathering", meta = (ClampMin = "1"))
	int32 AmountPerVisit = 1;
};

template<>
struct TMassFragmentTraits<FArcGatherAreaConfigFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};
