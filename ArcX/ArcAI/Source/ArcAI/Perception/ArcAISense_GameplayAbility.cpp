// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAISense_GameplayAbility.h"

#include "GameFramework/Pawn.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionListenerInterface.h"
#include "Perception/AIPerceptionSystem.h"

FArcAIGameplayAbilityEvent::FArcAIGameplayAbilityEvent()
	: AbilityTags()
	, InstigatorLocation()
	, Direction()
	, AffectedActor(nullptr)
	, Instigator(nullptr)
{}

FArcAIGameplayAbilityEvent::FArcAIGameplayAbilityEvent(const FGameplayTagContainer& InAbilityTags
		, AActor* InAffectedActor
		, AActor* InInstigator)
{
	AbilityTags = InAbilityTags;
	AffectedActor = InAffectedActor;
	Instigator = InInstigator;
	
	Compile();
}

void FArcAIGameplayAbilityEvent::Compile()
{
	if (AffectedActor == nullptr)
	{
		// nothing to do here, this event is invalid
		return;
	}

	if (Instigator != nullptr)
	{
		InstigatorLocation = Instigator->GetActorLocation();
	}

	Direction = (AffectedActor->GetActorLocation() - InstigatorLocation).GetSafeNormal();
}

IAIPerceptionListenerInterface* FArcAIGameplayAbilityEvent::GetDamagedActorAsPerceptionListener() const
{
	IAIPerceptionListenerInterface* Listener = nullptr;
	if (AffectedActor)
	{
		Listener = Cast<IAIPerceptionListenerInterface>(AffectedActor);
		if (Listener == nullptr)
		{
			APawn* ListenerAsPawn = Cast<APawn>(AffectedActor);
			if (ListenerAsPawn)
			{
				Listener = Cast<IAIPerceptionListenerInterface>(ListenerAsPawn->GetController());
			}
		}
	}
	return Listener;
}

FAISenseID UArcAISenseEvent_GameplayAbilityEvent::GetSenseID() const
{
	return UAISense::GetSenseID<UArcAISenseEvent_GameplayAbilityEvent>();
}

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//

float UArcAISense_GameplayAbility::Update()
{
	AIPerception::FListenerMap& ListenersMap = *GetListeners();

	for (const FArcAIGameplayAbilityEvent& Event : RegisteredEvents)
	{
		IAIPerceptionListenerInterface* PerceptionListener = Event.GetDamagedActorAsPerceptionListener();
		if (PerceptionListener != nullptr)
		{
			UAIPerceptionComponent* PerceptionComponent = PerceptionListener->GetPerceptionComponent();
			if (PerceptionComponent != nullptr && PerceptionComponent->GetListenerId().IsValid())
			{
				// this has to succeed, will assert a failure
				FPerceptionListener& Listener = ListenersMap[PerceptionComponent->GetListenerId()];

				if (Listener.HasSense(GetSenseID()))
				{
					Listener.RegisterStimulus(Event.Instigator, FAIStimulus(*this, 1, Event.InstigatorLocation, Event.AffectedActor->GetActorLocation(), FAIStimulus::SensingSucceeded, NAME_None));
				}
			}
		}
	}

	RegisteredEvents.Reset();

	// return decides when next tick is going to happen
	return SuspendNextUpdate;
}

void UArcAISense_GameplayAbility::RegisterEvent(const FArcAIGameplayAbilityEvent& Event)
{
	if (Event.IsValid())
	{
		RegisteredEvents.Add(Event);

		RequestImmediateUpdate();
	}
}

void UArcAISense_GameplayAbility::RegisterWrappedEvent(UAISenseEvent& PerceptionEvent)
{
	UArcAISenseEvent_GameplayAbilityEvent* DamageEvent = Cast<UArcAISenseEvent_GameplayAbilityEvent>(&PerceptionEvent);
	ensure(DamageEvent);
	if (DamageEvent)
	{
		RegisterEvent(DamageEvent->GetDamageEvent());
	}
}

void UArcAISense_GameplayAbility::ReportArcAIGameplayAbilityEvent(UObject* WorldContextObject, const FGameplayTagContainer& AbilityTags
	, AActor* AffectedActor, AActor* Instigator)
{
	UAIPerceptionSystem* PerceptionSystem = UAIPerceptionSystem::GetCurrent(WorldContextObject);
	if (PerceptionSystem)
	{
		FArcAIGameplayAbilityEvent Event(AbilityTags, AffectedActor, Instigator);
		PerceptionSystem->OnEvent(Event);
	}
}

