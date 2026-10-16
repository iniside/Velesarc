// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Tags/ArcGameplayTagRequirements.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcEffectApplyRequest.h"
#include "Effects/ArcEffectTypes.h"
#include "ArcEffectComponent.generated.h"

class UArcEffectDefinition;
struct FArcEffectSpec;

namespace ArcEffectHelpers
{
	ARCMASSABILITIES_API const FGameplayTagContainer& GetEffectTags(const UArcEffectDefinition* Effect);
}

USTRUCT(meta=(ShowTooltip))
struct ARCMASSABILITIES_API FArcEffectComponent
{
	GENERATED_BODY()

	virtual ~FArcEffectComponent() = default;

	virtual void OnPreApplication(const FArcEffectApplyRequest& Request, const FArcEffectSpec& Spec,
	                              FArcEffectPreApplicationResult& Result) const {}
	virtual void OnApplied(const FArcEffectContext& Context) const {}
	virtual void OnExecuted(const FArcEffectContext& Context) const {}
	virtual void OnRemoved(const FArcEffectContext& Context, EArcEffectRemovalReason Reason) const {}
	virtual void OnStackChanged(const FArcEffectContext& Context, int32 OldCount, int32 NewCount) const {}
	virtual void GatherIdentityTags(FGameplayTagContainer& OutTags) const {}
};

/** Gates effect application by checking RequirementFilter tags against the spec's captured target tags; rejects the effect if requirements are not met. */
USTRUCT(BlueprintType, meta=(DisplayName="Tag Filter", Tooltip="Gates effect application by checking RequirementFilter tags against the spec's captured target tags; rejects the effect if requirements are not met."))
struct ARCMASSABILITIES_API FArcEffectComp_TagFilter : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcGameplayTagRequirements RequirementFilter;

	virtual void OnPreApplication(const FArcEffectApplyRequest& Request, const FArcEffectSpec& Spec,
	                              FArcEffectPreApplicationResult& Result) const override;
};

/** Adds GrantedTags to the target entity's owned tags on application and removes them on removal. */
USTRUCT(BlueprintType, meta=(DisplayName="Grant Tags", Tooltip="Adds GrantedTags to the target entity's owned tags on application and removes them on removal."))
struct ARCMASSABILITIES_API FArcEffectComp_GrantTags : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FGameplayTagContainer GrantedTags;

	virtual void OnApplied(const FArcEffectContext& Context) const override;
	virtual void OnRemoved(const FArcEffectContext& Context, EArcEffectRemovalReason Reason) const override;
};

/** Applies EffectToApply to the target entity when this effect is applied. */
USTRUCT(BlueprintType, meta=(DisplayName="Apply Other Effect", Tooltip="Applies EffectToApply to the target entity when this effect is applied."))
struct ARCMASSABILITIES_API FArcEffectComp_ApplyOtherEffect : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcEffectDefinition> EffectToApply = nullptr;

	virtual void OnApplied(const FArcEffectContext& Context) const override;
};

/** Removes the target's active effects whose identity tags match TagQuery when this effect is applied; never removes this effect. */
USTRUCT(BlueprintType, meta=(DisplayName="Remove Effects", Tooltip="Removes the target's active effects whose identity tags match TagQuery when this effect is applied. An empty query removes nothing. Never removes this effect."))
struct ARCMASSABILITIES_API FArcEffectComp_RemoveEffects : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FGameplayTagQuery TagQuery;

	UPROPERTY(EditAnywhere, meta=(ClampMin="0", ToolTip="0 removes every match."))
	int32 MaxCount = 0;

	UPROPERTY(EditAnywhere)
	EArcEffectRemovalOrder Order = EArcEffectRemovalOrder::NewestFirst;

	virtual void OnApplied(const FArcEffectContext& Context) const override;
};

/** Declares identity tags carried by this effect; used by other components and tasks to identify and filter it. */
USTRUCT(BlueprintType, meta=(DisplayName="Effect Tags", Tooltip="Declares identity tags carried by this effect; used by other components and tasks to identify and filter it."))
struct ARCMASSABILITIES_API FArcEffectComp_EffectTags : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FGameplayTagContainer OwnedTags;

	virtual void GatherIdentityTags(FGameplayTagContainer& OutTags) const override;
};
