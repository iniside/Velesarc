// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/PrimaryAssetId.h"

#include "ArcCueDefinition.generated.h"

struct FAssetData;

/**
 * Data asset describing one cue. Fired by tag through UArcCueSubsystem.
 *
 * - CueTag is the primary lookup key. Multiple definitions may share a tag;
 *   FilterTags is the secondary discriminator (matched against
 *   FArcCueParameters::Tags at fire time, e.g. surface type for impact cues).
 * - Behaviors compose lifecycle hooks (Execute / Apply / Tick / Remove).
 */
UCLASS(BlueprintType, meta = (LoadBehavior = "LazyOnDemand"))
class ARCCUES_API UArcCueDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Primary lookup key. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	FGameplayTag CueTag;

	/** Optional secondary filter — matched against FArcCueParameters::Tags. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	FGameplayTagContainer FilterTags;

	/** Composable behaviors. Subclasses of FArcCueBehavior. */
	UPROPERTY(EditAnywhere, Category = "Cue", meta = (BaseStruct = "/Script/ArcCues.ArcCueBehavior", ExcludeBaseStruct))
	TArray<FInstancedStruct> Behaviors;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;

	/** Asset registry key names for fast lookups. */
	static const FName CueTagName;
	static const FName FilterTagsName;

	/** Helper: read CueTag from FAssetData without loading the asset. */
	static FGameplayTag GetCueTagFromAssetData(const FAssetData& AssetData);

	/** Helper: read FilterTags from FAssetData without loading the asset. */
	static FGameplayTagContainer GetFilterTagsFromAssetData(const FAssetData& AssetData);
};
