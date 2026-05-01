// Copyright Lukasz Baran. All Rights Reserved.

#include "Events/ArcMassEventBufferSubsystem.h"

#include "MassSignalSubsystem.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "Signals/ArcAttributeChangedSignal.h"
#include "Engine/World.h"

void UArcMassEventBufferSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(GetWorld());
	if (SignalSubsystem)
	{
		UE::MassSignal::FSignalDelegate& SignalDelegate = SignalSubsystem->GetSignalDelegateByName(UE::ArcMassAbilities::Signals::AttributeChanged);
		SignalDelegateHandle = SignalDelegate.AddUObject(this, &UArcMassEventBufferSubsystem::HandleAttributeChangedSignal);
	}

	WorldTickDelegateHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &UArcMassEventBufferSubsystem::HandleWorldPostActorTick);
}

void UArcMassEventBufferSubsystem::Deinitialize()
{
	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(GetWorld());
	if (SignalSubsystem)
	{
		SignalSubsystem->GetSignalDelegateByName(UE::ArcMassAbilities::Signals::AttributeChanged).Remove(SignalDelegateHandle);
	}

	FWorldDelegates::OnWorldPostActorTick.Remove(WorldTickDelegateHandle);

	Super::Deinitialize();
}

void UArcMassEventBufferSubsystem::HandleAttributeChangedSignal(FName SignalName, TConstArrayView<FMassEntityHandle> Entities)
{
	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(GetWorld());
	if (!EntitySubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	for (const FMassEntityHandle& Entity : Entities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}

		FMassEntityView EntityView(EntityManager, Entity);
		const FArcAttributeChangedFragment* ChangedFragment = EntityView.GetSparseFragmentDataPtr<FArcAttributeChangedFragment>();
		if (!ChangedFragment || ChangedFragment->Changes.IsEmpty())
		{
			continue;
		}

		FArcBufferedAttributeChange& Buffered = AttributeChangeBuffer.AddDefaulted_GetRef();
		Buffered.Entity = Entity;
		Buffered.Changes = ChangedFragment->Changes;
	}

	bBufferDirty = !AttributeChangeBuffer.IsEmpty();
}

void UArcMassEventBufferSubsystem::HandleWorldPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	if (World != GetWorld())
	{
		return;
	}

	if (!bBufferDirty)
	{
		return;
	}

	OnAttributeChangesReady.Broadcast(AttributeChangeBuffer);

	AttributeChangeBuffer.Reset();
	bBufferDirty = false;
}
