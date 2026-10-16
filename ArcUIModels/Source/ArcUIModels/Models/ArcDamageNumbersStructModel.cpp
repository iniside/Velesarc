// Copyright Lukasz Baran. All Rights Reserved.

#include "Models/ArcDamageNumbersStructModel.h"

#include "ArcAbilityKitTags.h"
#include "Attributes/ArcResourceAttributeHandlers.h"
#include "Events/ArcGameEventSubsystem.h"
#include "MassAgentComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"

void FArcDamageNumbersStructModel::CreateViewModels(UArcFrontendMVVMModelComponent& Component)
{
	FArcViewModelSpec Spec;
	Spec.ContextName = ContextName;
	Spec.ViewModelClass = UArcDamageNumbersViewModel::StaticClass();
	Spec.RegistrationTag = RegistrationTag;
	Component.CreateAndRegisterViewModels({ Spec });
}

void FArcDamageNumbersStructModel::Initialize(UArcFrontendMVVMModelComponent& Component)
{
	APlayerState* PS = Cast<APlayerState>(Component.GetOwner());
	if (!PS)
	{
		return;
	}

	APawn* Pawn = PS->GetPawn();
	if (!Pawn)
	{
		return;
	}

	UMassAgentComponent* MassAgent = Pawn->FindComponentByClass<UMassAgentComponent>();
	if (!MassAgent)
	{
		return;
	}

	FMassEntityHandle PlayerEntity = MassAgent->GetEntityHandle();
	if (!PlayerEntity.IsSet())
	{
		return;
	}

	UWorld* World = Component.GetWorld();
	if (!World)
	{
		return;
	}

	UArcGameEventSubsystem* Events = World->GetSubsystem<UArcGameEventSubsystem>();
	if (!Events)
	{
		return;
	}

	DamageNumbersVM = Cast<UArcDamageNumbersViewModel>(Component.FindViewModelByContext(ContextName, UArcDamageNumbersViewModel::StaticClass()));
	BoundEventSubsystem = Events;

	// Capturing `this` is safe only because Deinitialize unbinds before this struct dies.
	TWeakObjectPtr<UWorld> WeakWorld = World;
	HealthChangedHandle = Events->Subscribe(
		ArcAbilityKitTags::Event_HealthChanged,
		PlayerEntity,
		EArcEventRole::Instigator,
		FArcGameEventDelegate::CreateLambda([this, WeakWorld](const FArcGameEvent& Event)
		{
			if (UWorld* EventWorld = WeakWorld.Get())
			{
				HandleHealthChanged(Event, *EventWorld);
			}
		}));
}

void FArcDamageNumbersStructModel::HandleHealthChanged(const FArcGameEvent& Event, UWorld& World)
{
	const FArcHealthChangedPayload* Payload = Event.Payload.GetPtr<FArcHealthChangedPayload>();
	if (!Payload || Event.Target == Event.Instigator || Payload->TargetLocation.IsZero())
	{
		return;
	}

	FArcDamageNumberEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Id = ++NextId;
	Entry.Kind = Event.SourceTags.HasTagExact(CritTag) ? TEXT("crit") : TEXT("hit");
	Entry.Value = FMath::RoundToInt(FMath::Abs(Event.Magnitude));
	Entry.WorldX = Payload->TargetLocation.X;
	Entry.WorldY = Payload->TargetLocation.Y;
	Entry.WorldZ = Payload->TargetLocation.Z;
	Entry.OffsetX = FMath::FRandRange(OffsetMin.X, OffsetMax.X);
	Entry.OffsetY = FMath::FRandRange(OffsetMin.Y, OffsetMax.Y);
	Entry.Drift = FMath::FRandRange(DriftMin, DriftMax);
	ExpiryTimes.Add(World.GetTimeSeconds() + Lifetime);

	const int32 Overflow = Entries.Num() - MaxEntries;
	if (Overflow > 0)
	{
		Entries.RemoveAt(0, Overflow);
		ExpiryTimes.RemoveAt(0, Overflow);
	}

	PushToViewModel();
}

void FArcDamageNumbersStructModel::Tick(float DeltaTime, UArcFrontendMVVMModelComponent& Component)
{
	UWorld* World = Component.GetWorld();
	if (!World || ExpiryTimes.IsEmpty())
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	int32 ExpiredCount = 0;
	while (ExpiredCount < ExpiryTimes.Num() && ExpiryTimes[ExpiredCount] <= Now)
	{
		++ExpiredCount;
	}

	if (ExpiredCount == 0)
	{
		return;
	}

	Entries.RemoveAt(0, ExpiredCount);
	ExpiryTimes.RemoveAt(0, ExpiredCount);
	PushToViewModel();
}

void FArcDamageNumbersStructModel::Deinitialize(UArcFrontendMVVMModelComponent& Component)
{
	if (UArcGameEventSubsystem* Events = BoundEventSubsystem.Get())
	{
		Events->Unsubscribe(HealthChangedHandle);
	}
	HealthChangedHandle.Reset();
	BoundEventSubsystem.Reset();

	Entries.Reset();
	ExpiryTimes.Reset();
	PushToViewModel();
	DamageNumbersVM.Reset();
}

void FArcDamageNumbersStructModel::PushToViewModel() const
{
	if (UArcDamageNumbersViewModel* VM = DamageNumbersVM.Get())
	{
		VM->SetEntries(Entries);
	}
}
