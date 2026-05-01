// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityHandle.h"
#include "Attributes/ArcAttribute.h"
#include "GameplayTagContainer.h"
#include "ArcEffectEventSubsystem.generated.h"

class UArcEffectDefinition;
class UArcAbilityDefinition;

DECLARE_MULTICAST_DELEGATE_FourParams(FArcOnAnyAttributeChanged, FMassEntityHandle /*Entity*/, FArcAttributeRef /*Attribute*/, float /*OldValue*/, float /*NewValue*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FArcOnAttributeChanged, FMassEntityHandle /*Entity*/, float /*OldValue*/, float /*NewValue*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FArcOnAbilityActivated, FMassEntityHandle /*Entity*/, UArcAbilityDefinition* /*Ability*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FArcOnEffectApplied, FMassEntityHandle /*Entity*/, UArcEffectDefinition* /*Effect*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FArcOnEffectExecuted, FMassEntityHandle /*Entity*/, UArcEffectDefinition* /*Effect*/);
DECLARE_MULTICAST_DELEGATE_FourParams(FArcOnTagCountChanged, FMassEntityHandle /*Entity*/, FGameplayTag /*Tag*/, int32 /*OldCount*/, int32 /*NewCount*/);

struct FArcAttributeSubscriptionKey
{
	FMassEntityHandle Entity;
	FArcAttributeRef Attribute;

	bool operator==(const FArcAttributeSubscriptionKey& Other) const
	{
		return Entity == Other.Entity && Attribute == Other.Attribute;
	}

	friend uint32 GetTypeHash(const FArcAttributeSubscriptionKey& Key)
	{
		return HashCombine(GetTypeHash(Key.Entity), GetTypeHash(Key.Attribute));
	}
};

UCLASS()
class ARCMASSABILITIES_API UArcEffectEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	FDelegateHandle SubscribeAnyAttributeChanged(FMassEntityHandle Entity, FArcOnAnyAttributeChanged::FDelegate Delegate);
	void UnsubscribeAnyAttributeChanged(FMassEntityHandle Entity, FDelegateHandle Handle);

	FDelegateHandle SubscribeAttributeChanged(FMassEntityHandle Entity, FArcAttributeRef Attribute, FArcOnAttributeChanged::FDelegate Delegate);
	void UnsubscribeAttributeChanged(FMassEntityHandle Entity, FArcAttributeRef Attribute, FDelegateHandle Handle);

	FDelegateHandle SubscribeAbilityActivated(FMassEntityHandle Entity, FArcOnAbilityActivated::FDelegate Delegate);
	void UnsubscribeAbilityActivated(FMassEntityHandle Entity, FDelegateHandle Handle);

	FDelegateHandle SubscribeEffectApplied(FMassEntityHandle Entity, FArcOnEffectApplied::FDelegate Delegate);
	void UnsubscribeEffectApplied(FMassEntityHandle Entity, FDelegateHandle Handle);

	FDelegateHandle SubscribeEffectExecuted(FMassEntityHandle Entity, FArcOnEffectExecuted::FDelegate Delegate);
	void UnsubscribeEffectExecuted(FMassEntityHandle Entity, FDelegateHandle Handle);

	FDelegateHandle SubscribeTagCountChanged(FMassEntityHandle Entity, FArcOnTagCountChanged::FDelegate Delegate);
	void UnsubscribeTagCountChanged(FMassEntityHandle Entity, FDelegateHandle Handle);

	void BroadcastAttributeChanged(FMassEntityHandle Entity, FArcAttributeRef Attribute, float OldValue, float NewValue);
	void BroadcastAbilityActivated(FMassEntityHandle Entity, UArcAbilityDefinition* Ability);
	void BroadcastEffectApplied(FMassEntityHandle Entity, UArcEffectDefinition* Effect);
	void BroadcastEffectExecuted(FMassEntityHandle Entity, UArcEffectDefinition* Effect);
	void BroadcastTagCountChanged(FMassEntityHandle Entity, FGameplayTag Tag, int32 OldCount, int32 NewCount);

	void RemoveEntity(FMassEntityHandle Entity);

private:
	TMap<FMassEntityHandle, FArcOnAnyAttributeChanged> AnyAttributeChangedDelegates;
	TMap<FArcAttributeSubscriptionKey, FArcOnAttributeChanged> SpecificAttributeChangedDelegates;
	TMap<FMassEntityHandle, FArcOnAbilityActivated> AbilityActivatedDelegates;
	TMap<FMassEntityHandle, FArcOnEffectApplied> EffectAppliedDelegates;
	TMap<FMassEntityHandle, FArcOnEffectExecuted> EffectExecutedDelegates;
	TMap<FMassEntityHandle, FArcOnTagCountChanged> TagCountChangedDelegates;
};
