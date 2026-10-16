// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSPreviewEntities.h"

#include "ArcTQSPreviewTypes.h"
#include "ArcMass/Spatial/ArcMassSpatialHashSubsystem.h"
#include "Engine/World.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Mass/EntityBuilder.h"
#include "Mass/EntityFragments.h"
#include "MassEntitySubsystem.h"

FMassEntityHandle FArcTQSPreviewEntities::Create(UWorld& InWorld, const FArcTQSPreviewMock& Mock)
{
	if (World.IsValid() && World.Get() != &InWorld)
	{
		Release();
	}

	UMassEntitySubsystem* EntitySubsystem = InWorld.GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return FMassEntityHandle();
	}
	World = &InWorld;

	UE::Mass::FEntityBuilder Builder(EntitySubsystem->GetMutableEntityManager());
	Builder.Add<FTransformFragment>(Mock.Transform);
	if (!Mock.OwnedTags.IsEmpty())
	{
		FArcOwnedTagsFragment OwnedTags;
		for (const FGameplayTag& Tag : Mock.OwnedTags)
		{
			OwnedTags.Tags.AddTag(Tag);
		}
		Builder.Add<FArcOwnedTagsFragment>(OwnedTags);
	}
	const FMassEntityHandle Entity = Builder.Commit();

	if (UArcMassSpatialHashSubsystem* SpatialHash = InWorld.GetSubsystem<UArcMassSpatialHashSubsystem>())
	{
		SpatialHash->RegisterExternalEntity(Entity, Mock.Transform.GetLocation(), Mock.SpatialIndex);
	}

	Entries.Add({Mock.Name, Entity});
	return Entity;
}

void FArcTQSPreviewEntities::Move(const FMassEntityHandle Entity, const FTransform& Transform)
{
	FMassEntityManager* EntityManager = GetEntityManager();
	if (!EntityManager || !EntityManager->IsEntityValid(Entity))
	{
		return;
	}

	if (FTransformFragment* TransformFragment = EntityManager->GetFragmentDataPtr<FTransformFragment>(Entity))
	{
		TransformFragment->SetTransform(Transform);
	}

	if (UArcMassSpatialHashSubsystem* SpatialHash = World->GetSubsystem<UArcMassSpatialHashSubsystem>())
	{
		SpatialHash->MoveExternalEntity(Entity, Transform.GetLocation());
	}
}

void FArcTQSPreviewEntities::Destroy(const FMassEntityHandle Entity)
{
	const int32 Index = Entries.IndexOfByPredicate([Entity](const FEntry& Entry)
	{
		return Entry.Entity == Entity;
	});
	if (Index == INDEX_NONE)
	{
		return;
	}

	DestroyEntity(Entity);
	Entries.RemoveAtSwap(Index);
}

void FArcTQSPreviewEntities::Release()
{
	for (const FEntry& Entry : Entries)
	{
		DestroyEntity(Entry.Entity);
	}
	Entries.Reset();
	World.Reset();
}

FMassEntityHandle FArcTQSPreviewEntities::FindByName(const FName Name) const
{
	const FEntry* Entry = Entries.FindByPredicate([Name](const FEntry& Candidate)
	{
		return Candidate.Name == Name;
	});
	return Entry ? Entry->Entity : FMassEntityHandle();
}

FMassEntityManager* FArcTQSPreviewEntities::GetEntityManager() const
{
	UWorld* PreviewWorld = World.Get();
	UMassEntitySubsystem* EntitySubsystem = PreviewWorld ? PreviewWorld->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	return EntitySubsystem ? &EntitySubsystem->GetMutableEntityManager() : nullptr;
}

void FArcTQSPreviewEntities::DestroyEntity(const FMassEntityHandle Entity) const
{
	FMassEntityManager* EntityManager = GetEntityManager();
	if (!EntityManager)
	{
		return;
	}

	if (UArcMassSpatialHashSubsystem* SpatialHash = World->GetSubsystem<UArcMassSpatialHashSubsystem>())
	{
		SpatialHash->UnregisterExternalEntity(Entity);
	}

	if (!EntityManager->IsEntityValid(Entity))
	{
		return;
	}
	if (EntityManager->IsProcessing())
	{
		EntityManager->Defer().DestroyEntity(Entity);
		return;
	}
	EntityManager->DestroyEntity(Entity);
}

FArcTQSQueryReference ArcTQS::MakeReferenceWithEntityParameters(
	const FArcTQSQueryReference& Reference,
	const TMap<FGuid, FMassEntityHandle>& EntityParameters)
{
	FArcTQSQueryReference Result = Reference;
	FInstancedPropertyBag& Parameters = Result.GetMutableParameters();
	for (const TPair<FGuid, FMassEntityHandle>& EntityParameter : EntityParameters)
	{
		if (const FPropertyBagPropertyDesc* Desc = Parameters.FindPropertyDescByID(EntityParameter.Key))
		{
			Parameters.SetValueStruct(*Desc, EntityParameter.Value);
		}
	}
	return Result;
}
