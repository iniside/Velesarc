// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Perception/AISense.h"
#include "Perception/AISenseEvent.h"
#include "ArcAISense_GameplayAbility.generated.h"

class IAIPerceptionListenerInterface;
class UAISenseEvent;

USTRUCT(BlueprintType)
struct ARCAI_API FArcAIBaseEvent
{
	GENERATED_BODY()
};
USTRUCT(BlueprintType)
struct ARCAI_API FArcAIGameplayAbilityEvent : public FArcAIBaseEvent
{	
	GENERATED_BODY()

	typedef class UArcAISense_GameplayAbility FSenseClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sense")
	FGameplayTagContainer AbilityTags;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sense")
	FVector InstigatorLocation;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sense")
	FVector Direction;
	
	/** Damaged actor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sense")
	TObjectPtr<AActor> AffectedActor;

	/** Actor that instigated damage. Can be None */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sense")
	TObjectPtr<AActor> Instigator;

	FArcAIGameplayAbilityEvent();
	FArcAIGameplayAbilityEvent(const FGameplayTagContainer& InAbilityTags
		, AActor* InAffectedActor
		, AActor* InInstigator);
	
	void Compile();

	bool IsValid() const
	{
		return AffectedActor != nullptr;
	}

	IAIPerceptionListenerInterface* GetDamagedActorAsPerceptionListener() const;
};

UCLASS()
class ARCAI_API UArcAISenseEvent_GameplayAbilityEvent : public UAISenseEvent
{
	GENERATED_BODY()

public:	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sense")
	FArcAIGameplayAbilityEvent Event;
	
	virtual FAISenseID GetSenseID() const override;

	inline FArcAIGameplayAbilityEvent GetDamageEvent()
	{
		Event.Compile();
		return Event;
	}
};


/**
 * 
 */
UCLASS()
class ARCAI_API UArcAISense_GameplayAbility : public UAISense
{
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<FArcAIGameplayAbilityEvent> RegisteredEvents;

public:		
	void RegisterEvent(const FArcAIGameplayAbilityEvent& Event);	
	virtual void RegisterWrappedEvent(UAISenseEvent& PerceptionEvent) override;

	/** EventLocation will be reported as Instigator's location at the moment of event happening*/
	UFUNCTION(BlueprintCallable, Category = "Arc|AI|Perception", meta = (WorldContext="WorldContextObject"))
	static void ReportArcAIGameplayAbilityEvent(UObject* WorldContextObject, const FGameplayTagContainer& AbilityTags, AActor* AffectedActor, AActor* Instigator);

protected:
	virtual float Update() override;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FArcAIGameplayAbilityEventDelegate, const FArcAIGameplayAbilityEvent&);

UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class UArcAIGameplayAbilitySenseComponent : public UActorComponent
{
	GENERATED_BODY()
	
public:
	FArcAIGameplayAbilityEventDelegate OnArcAIGameplayAbilityEvent;
	
	UFUNCTION(BlueprintCallable, Category = "Arc|AI|Perception")
	static void SendAGameplayAbilityEventDelegate(const FGameplayTagContainer& AbilityTags
	, AActor* AffectedActor, AActor* Instigator)
	{
		UArcAIGameplayAbilitySenseComponent* Component = Cast<UArcAIGameplayAbilitySenseComponent>(AffectedActor->GetComponentByClass(UArcAIGameplayAbilitySenseComponent::StaticClass()));
		FArcAIGameplayAbilityEvent Event(AbilityTags, AffectedActor, Instigator);
		Component->OnArcAIGameplayAbilityEvent.Broadcast(Event);
	};
};