// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcConditionTypes.h"
#include "MassSignalProcessorBase.h"
#include "Containers/StaticArray.h"
#include "GameplayTagContainer.h"

#include "ArcConditionReactionProcessor.generated.h"

class UGameplayEffect;
class UArcConditionEffectsSubsystem;
class UArcCoreAbilitySystemComponent;
struct FArcConditionEffectEntry;

UCLASS()
class ARCCONDITIONEFFECTS_API UArcConditionReactionProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcConditionReactionProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;

private:
	void ProcessConditionTransition(
		EArcConditionType ConditionType,
		const FArcConditionState* CurrentState,
		FArcConditionPrevState& PrevState,
		FMassEntityHandle Entity,
		UArcCoreAbilitySystemComponent* ASC,
		UArcConditionEffectsSubsystem* Subsystem);

	void ApplyEffectToASC(UArcCoreAbilitySystemComponent* ASC, TSubclassOf<UGameplayEffect> EffectClass, float Saturation);
	void RemoveEffectsWithTags(UArcCoreAbilitySystemComponent* ASC, const FGameplayTagContainer& TagContainer);

	TMap<FMassEntityHandle, TStaticArray<FArcConditionPrevState, ArcConditionTypeCount>> PrevStates;

	TStaticArray<FGameplayTagContainer, ArcConditionTypeCount> ActiveTagContainers;
	TStaticArray<FGameplayTagContainer, ArcConditionTypeCount> OverloadTagContainers;
	bool bTagContainersBuilt = false;
	void EnsureTagContainersBuilt();
};
