// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcConditionVolume.h"

#include "ArcConditionEffectsSubsystem.h"
#include "Components/BoxComponent.h"
#include "MassAgentComponent.h"

AArcConditionVolume::AArcConditionVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	CollisionBox->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionBox->SetGenerateOverlapEvents(true);
}

void AArcConditionVolume::BeginPlay()
{
	Super::BeginPlay();

	CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AArcConditionVolume::OnOverlapBegin);
	CollisionBox->OnComponentEndOverlap.AddDynamic(this, &AArcConditionVolume::OnOverlapEnd);

	GetWorldTimerManager().SetTimer(TimerHandle, this, &AArcConditionVolume::ApplyConditionToTrackedEntities, Interval, true);
}

void AArcConditionVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(TimerHandle);
	TrackedEntities.Empty();

	Super::EndPlay(EndPlayReason);
}

void AArcConditionVolume::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor)
	{
		return;
	}

	if (UMassAgentComponent* AgentComp = OtherActor->FindComponentByClass<UMassAgentComponent>())
	{
		const FMassEntityHandle Entity = AgentComp->GetEntityHandle();
		if (Entity.IsValid())
		{
			TrackedEntities.Add(Entity);
		}
	}
}

void AArcConditionVolume::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor)
	{
		return;
	}

	if (UMassAgentComponent* AgentComp = OtherActor->FindComponentByClass<UMassAgentComponent>())
	{
		TrackedEntities.Remove(AgentComp->GetEntityHandle());
	}
}

void AArcConditionVolume::ApplyConditionToTrackedEntities()
{
	if (TrackedEntities.IsEmpty())
	{
		return;
	}

	UArcConditionEffectsSubsystem* Subsystem = GetWorld()->GetSubsystem<UArcConditionEffectsSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	for (const FMassEntityHandle& Entity : TrackedEntities)
	{
		if (Entity.IsValid())
		{
			Subsystem->ApplyCondition(Entity, ConditionType, Amount);
		}
	}
}
