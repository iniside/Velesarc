#pragma once

#include "Items/Fragments/ArcItemFragment.h"
#include "Items/ArcItemInstance.h"
#include "Items/ArcItemId.h"
#include "GameplayTagContainer.h"
#include "ArcItemFragment_MassAbilityEffects.generated.h"

class UArcEffectDefinition;

USTRUCT(BlueprintType, meta = (ToolTip = "A single entry mapping Mass effect definitions to source/target tag requirements."))
struct ARCMASSITEMSRUNTIME_API FArcMassEffectEntry
{
	GENERATED_BODY()

	/** All of these tags must be present on source (owner of this item). */
	UPROPERTY(EditAnywhere, Category = "Config")
	FGameplayTagContainer SourceRequiredTags;

	/** None of these tags must be present on source. */
	UPROPERTY(EditAnywhere, Category = "Config")
	FGameplayTagContainer SourceIgnoreTags;

	/** All of these tags must be present on target. */
	UPROPERTY(EditAnywhere, Category = "Config")
	FGameplayTagContainer TargetRequiredTags;

	/** None of these tags must be present on target. */
	UPROPERTY(EditAnywhere, Category = "Config")
	FGameplayTagContainer TargetIgnoreTags;

	UPROPERTY(EditAnywhere, Category = "Config")
	TArray<TObjectPtr<UArcEffectDefinition>> Effects;
};

USTRUCT(BlueprintType, meta = (ToolTip = "Cached runtime entry holding a resolved Mass effect spec with tag filtering data."))
struct ARCMASSITEMSRUNTIME_API FArcMassEffectSpecItem
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag EffectTag;

	FArcItemId SourceItemId;

	FGameplayTagContainer SourceRequiredTags;
	FGameplayTagContainer SourceIgnoreTags;
	FGameplayTagContainer TargetRequiredTags;
	FGameplayTagContainer TargetIgnoreTags;

	UPROPERTY()
	TArray<TObjectPtr<UArcEffectDefinition>> Effects;

	bool operator==(const FArcItemId& InId) const { return SourceItemId == InId; }
};

USTRUCT(meta = (ToolTip = "Mutable instance data holding pre-cached Mass effect specs. Populated at item initialization and queried by abilities at runtime."))
struct ARCMASSITEMSRUNTIME_API FArcItemInstance_MassAbilityEffects : public FArcItemInstance_ItemData
{
	GENERATED_BODY()

	TArray<FArcMassEffectSpecItem> CachedSpecs;

	TArray<const FArcMassEffectSpecItem*> GetSpecsByTag(const FGameplayTag& InTag) const;

	const TArray<FArcMassEffectSpecItem>& GetAllSpecs() const { return CachedSpecs; }

	virtual bool Equals(const FArcItemInstance& Other) const override { return true; }
};

USTRUCT(BlueprintType, meta = (DisplayName = "Mass Ability Effects", Category = "Mass Abilities",
	ToolTip = "Pre-caches Mass effect definitions keyed by tag for ability-triggered application."))
struct ARCMASSITEMSRUNTIME_API FArcItemFragment_MassAbilityEffects : public FArcItemFragment_ItemInstanceBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Config", meta = (Categories = "GameplayEffect", ForceInlineRow))
	TMap<FGameplayTag, FArcMassEffectEntry> Effects;

	virtual UScriptStruct* GetItemInstanceType() const override
	{
		return FArcItemInstance_MassAbilityEffects::StaticStruct();
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemFragment_MassAbilityEffects::StaticStruct();
	}

	virtual ~FArcItemFragment_MassAbilityEffects() override = default;

	virtual void OnItemInitialize(const FArcItemData* InItem) const override;
	virtual void OnItemChanged(const FArcItemData* InItem) const override;

#if WITH_EDITOR
	virtual FArcFragmentDescription GetDescription(const UScriptStruct* InStruct) const override;
#endif
};
