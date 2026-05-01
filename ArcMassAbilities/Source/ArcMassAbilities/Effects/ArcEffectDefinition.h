// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "ScalableFloat.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectTypes.h"
#include "Effects/ArcEffectModifier.h"
#include "ArcEffectDefinition.generated.h"

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcEffectPolicy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	EArcEffectDuration DurationType = EArcEffectDuration::Instant;

	UPROPERTY(EditAnywhere, meta=(EditCondition="DurationType==EArcEffectDuration::Duration", EditConditionHides))
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, meta=(EditCondition="DurationType!=EArcEffectDuration::Instant", EditConditionHides))
	EArcEffectPeriodicity Periodicity = EArcEffectPeriodicity::NonPeriodic;

	UPROPERTY(EditAnywhere, meta=(EditCondition="Periodicity==EArcEffectPeriodicity::Periodic", EditConditionHides))
	float Period = 0.f;

	UPROPERTY(EditAnywhere, meta=(EditCondition="Periodicity==EArcEffectPeriodicity::Periodic", EditConditionHides))
	EArcPeriodicExecutionPolicy PeriodicExecPolicy = EArcPeriodicExecutionPolicy::PeriodAndApplication;

	UPROPERTY(EditAnywhere, meta=(EditCondition="DurationType!=EArcEffectDuration::Instant", EditConditionHides))
	EArcStackType StackType = EArcStackType::Counter;

	UPROPERTY(EditAnywhere, meta=(EditCondition="DurationType!=EArcEffectDuration::Instant", EditConditionHides))
	EArcStackGrouping Grouping = EArcStackGrouping::ByTarget;

	UPROPERTY(EditAnywhere, meta=(EditCondition="DurationType!=EArcEffectDuration::Instant&&StackType==EArcStackType::Counter", EditConditionHides))
	EArcStackPolicy StackPolicy = EArcStackPolicy::Replace;

	UPROPERTY(EditAnywhere, meta=(EditCondition="DurationType!=EArcEffectDuration::Instant", EditConditionHides))
	int32 MaxStacks = 1;

	UPROPERTY(EditAnywhere, meta=(EditCondition="DurationType!=EArcEffectDuration::Instant&&StackType==EArcStackType::Counter&&StackPolicy==EArcStackPolicy::Aggregate", EditConditionHides))
	EArcStackDurationRefresh RefreshPolicy = EArcStackDurationRefresh::Refresh;

	UPROPERTY(EditAnywhere, meta=(EditCondition="Periodicity==EArcEffectPeriodicity::Periodic&&StackType==EArcStackType::Counter", EditConditionHides))
	EArcStackPeriodPolicy PeriodPolicy = EArcStackPeriodPolicy::Unchanged;

	UPROPERTY(EditAnywhere, meta=(EditCondition="DurationType!=EArcEffectDuration::Instant&&StackType==EArcStackType::Instance", EditConditionHides))
	EArcStackOverflowPolicy OverflowPolicy = EArcStackOverflowPolicy::Deny;
};

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcEffectExecutionEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TArray<FArcEffectModifier> Modifiers;

	UPROPERTY(EditAnywhere, meta=(BaseStruct="/Script/ArcMassAbilities.ArcEffectExecution", ExcludeBaseStruct))
	FInstancedStruct CustomExecution;
};

UCLASS(BlueprintType, meta=(LoadBehavior="LazyOnDemand"))
class ARCMASSABILITIES_API UArcEffectDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Identity")
	FGuid EffectId;

	UPROPERTY(EditAnywhere, Category = "Identity")
	FPrimaryAssetType EffectType;

	UPROPERTY(EditAnywhere, Category = "Modifiers")
	TArray<FArcEffectModifier> Modifiers;

	UPROPERTY(EditAnywhere, Category = "Executions")
	TArray<FArcEffectExecutionEntry> Executions;

	UPROPERTY(EditAnywhere, Category = "Policy")
	FArcEffectPolicy StackingPolicy;

	UPROPERTY(EditAnywhere, Category = "Components", meta=(BaseStruct="/Script/ArcMassAbilities.ArcEffectComponent", ExcludeBaseStruct))
	TArray<FInstancedStruct> Components;

	UPROPERTY(EditAnywhere, Category = "Tasks", meta=(BaseStruct="/Script/ArcMassAbilities.ArcEffectTask", ExcludeBaseStruct))
	TArray<FInstancedStruct> Tasks;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Identity")
	void RegenerateEffectId();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void PostInitProperties() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PostLoad() override;
};
