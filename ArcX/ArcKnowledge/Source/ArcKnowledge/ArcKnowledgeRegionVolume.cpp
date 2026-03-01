// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcKnowledgeRegionVolume.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntryDefinition.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"

AArcKnowledgeRegionVolume::AArcKnowledgeRegionVolume()
{
	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("RegionBounds"));
	SphereComponent->SetSphereRadius(50000.0f);
	SphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SphereComponent->SetGenerateOverlapEvents(false);
	SphereComponent->ShapeColor = FColor::Yellow;
	RootComponent = SphereComponent;
}

void AArcKnowledgeRegionVolume::BeginPlay()
{
	Super::BeginPlay();

	// Resolve effective values — per-instance properties override definition
	FGameplayTagContainer EffectiveTags = KnowledgeTags;
	FInstancedStruct EffectivePayload = Payload;
	float EffectiveBroadcastRadius = SpatialBroadcastRadius;

	if (Definition)
	{
		if (EffectiveTags.IsEmpty())
		{
			EffectiveTags = Definition->Tags;
		}
		if (!EffectivePayload.IsValid())
		{
			EffectivePayload = Definition->Payload;
		}
		if (EffectiveBroadcastRadius == 0.0f)
		{
			EffectiveBroadcastRadius = Definition->SpatialBroadcastRadius;
		}
		// Apply bounding radius from definition
		if (SphereComponent)
		{
			SphereComponent->SetSphereRadius(Definition->BoundingRadius);
		}
	}

	const bool bHasDefinitionEntries = Definition && !Definition->InitialKnowledge.IsEmpty();
	if (EffectiveTags.IsEmpty() && InitialKnowledge.IsEmpty() && !bHasDefinitionEntries)
	{
		UE_LOG(LogTemp, Warning, TEXT("AArcKnowledgeRegionVolume '%s' has no tags, definition, or initial knowledge set."), *GetName());
		return;
	}

	UArcKnowledgeSubsystem* Subsystem = GetWorld()->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	// Register primary knowledge entry for this region
	if (!EffectiveTags.IsEmpty())
	{
		FArcKnowledgeEntry Entry;
		Entry.Tags = EffectiveTags;
		Entry.Location = GetActorLocation();
		Entry.Payload = EffectivePayload;
		Entry.SpatialBroadcastRadius = EffectiveBroadcastRadius;
		KnowledgeHandle = Subsystem->RegisterKnowledge(Entry);
	}

	// Register initial knowledge entries from definition first
	if (bHasDefinitionEntries)
	{
		for (FArcKnowledgeEntry InitEntry : Definition->InitialKnowledge)
		{
			if (InitEntry.Location.IsZero())
			{
				InitEntry.Location = GetActorLocation();
			}
			FArcKnowledgeHandle Handle = Subsystem->RegisterKnowledge(InitEntry);
			InitialKnowledgeHandles.Add(Handle);
		}
	}

	// Register per-instance initial knowledge entries
	for (FArcKnowledgeEntry& InitEntry : InitialKnowledge)
	{
		if (InitEntry.Location.IsZero())
		{
			InitEntry.Location = GetActorLocation();
		}
		FArcKnowledgeHandle Handle = Subsystem->RegisterKnowledge(InitEntry);
		InitialKnowledgeHandles.Add(Handle);
	}
}

void AArcKnowledgeRegionVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UArcKnowledgeSubsystem* Subsystem = GetWorld()->GetSubsystem<UArcKnowledgeSubsystem>())
	{
		if (KnowledgeHandle.IsValid())
		{
			Subsystem->RemoveKnowledge(KnowledgeHandle);
			KnowledgeHandle = FArcKnowledgeHandle();
		}

		for (const FArcKnowledgeHandle& Handle : InitialKnowledgeHandles)
		{
			if (Handle.IsValid())
			{
				Subsystem->RemoveKnowledge(Handle);
			}
		}
		InitialKnowledgeHandles.Empty();
	}

	Super::EndPlay(EndPlayReason);
}
