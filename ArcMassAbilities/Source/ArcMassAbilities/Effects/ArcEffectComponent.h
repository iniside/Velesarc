// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "Effects/ArcEffectContext.h"
#include "ArcEffectComponent.generated.h"

class UArcEffectDefinition;

namespace ArcEffectHelpers
{
	ARCMASSABILITIES_API FGameplayTagContainer GetEffectTags(const UArcEffectDefinition* Effect);
}

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectComponent
{
	GENERATED_BODY()

	virtual ~FArcEffectComponent() = default;

	virtual bool OnPreApplication(const FArcEffectContext& Context) const { return true; }
	virtual void OnApplied(const FArcEffectContext& Context) const {}
	virtual void OnExecuted(const FArcEffectContext& Context) const {}
	virtual void OnRemoved(const FArcEffectContext& Context) const {}
	virtual void OnStackChanged(const FArcEffectContext& Context, int32 OldCount, int32 NewCount) const {}
};

USTRUCT(BlueprintType, meta=(DisplayName="Tag Filter"))
struct ARCMASSABILITIES_API FArcEffectComp_TagFilter : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FGameplayTagRequirements RequirementFilter;

	virtual bool OnPreApplication(const FArcEffectContext& Context) const override;
};

USTRUCT(BlueprintType, meta=(DisplayName="Grant Tags"))
struct ARCMASSABILITIES_API FArcEffectComp_GrantTags : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FGameplayTagContainer GrantedTags;

	virtual void OnApplied(const FArcEffectContext& Context) const override;
	virtual void OnRemoved(const FArcEffectContext& Context) const override;
};

USTRUCT(BlueprintType, meta=(DisplayName="Apply Other Effect"))
struct ARCMASSABILITIES_API FArcEffectComp_ApplyOtherEffect : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcEffectDefinition> EffectToApply = nullptr;

	virtual void OnApplied(const FArcEffectContext& Context) const override;
};

USTRUCT(BlueprintType, meta=(DisplayName="Remove Effect By Tag"))
struct ARCMASSABILITIES_API FArcEffectComp_RemoveEffectByTag : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FGameplayTag EffectTag;

	virtual void OnApplied(const FArcEffectContext& Context) const override;
};

USTRUCT(BlueprintType, meta=(DisplayName="Effect Tags"))
struct ARCMASSABILITIES_API FArcEffectComp_EffectTags : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FGameplayTagContainer OwnedTags;
};
