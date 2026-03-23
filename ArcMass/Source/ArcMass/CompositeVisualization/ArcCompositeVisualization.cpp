// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCompositeVisualization.h"
#include "ArcMass/ArcMassEntityVisualization.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"

// ---------------------------------------------------------------------------
// FArcCompositeVisConfigFragment
// ---------------------------------------------------------------------------

FArcCompositeVisConfigFragment::FArcCompositeVisConfigFragment()
{
	ISMManagerClass = AActor::StaticClass();
}

// ---------------------------------------------------------------------------
// Mesh Capture
// ---------------------------------------------------------------------------

namespace UE::ArcMass::CompositeVis
{
	void CaptureActorMeshData(AActor* Actor, TArray<FArcCompositeVisInstanceEntry>& OutEntries)
	{
		if (!Actor)
		{
			return;
		}

		OutEntries.Reset();

		TArray<UStaticMeshComponent*> MeshComponents;
		Actor->GetComponents<UStaticMeshComponent>(MeshComponents);

		const FTransform ActorTransform = Actor->GetActorTransform();
		int32 EntryIdx = 0;

		for (UStaticMeshComponent* Comp : MeshComponents)
		{
			if (!Comp)
			{
				continue;
			}

			// Skip instanced static mesh components
			if (Comp->IsA<UInstancedStaticMeshComponent>())
			{
				continue;
			}

			UStaticMesh* Mesh = Comp->GetStaticMesh();
			if (!Mesh)
			{
				continue;
			}

			FArcCompositeVisInstanceEntry& Entry = OutEntries.AddDefaulted_GetRef();
			Entry.Mesh = Mesh;
			Entry.RelativeTransform = Comp->GetComponentTransform().GetRelativeTransform(ActorTransform);
			Entry.EntryIndex = EntryIdx++;

			const int32 NumMaterials = Comp->GetNumMaterials();
			Entry.Materials.SetNum(NumMaterials);
			for (int32 i = 0; i < NumMaterials; i++)
			{
				Entry.Materials[i] = Comp->GetMaterial(i);
			}
		}
	}
} // namespace UE::ArcMass::CompositeVis

// ---------------------------------------------------------------------------
// UArcCompositeVisSubsystem
// ---------------------------------------------------------------------------

void UArcCompositeVisSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UArcCompositeVisSubsystem::Deinitialize()
{
	for (TPair<FCellManagerKey, FCellManager>& Pair : CellManagers)
	{
		if (Pair.Value.ManagerActor)
		{
			Pair.Value.ManagerActor->Destroy();
		}
	}
	CellManagers.Empty();

	Super::Deinitialize();
}

UArcCompositeVisSubsystem::FCellManager& UArcCompositeVisSubsystem::GetOrCreateCellManager(
	const FIntVector& Cell,
	TSubclassOf<AActor> ManagerClass)
{
	const FCellManagerKey Key{Cell, ManagerClass.Get()};
	if (FCellManager* Existing = CellManagers.Find(Key))
	{
		return *Existing;
	}

	FCellManager& NewManager = CellManagers.Add(Key);

	UWorld* World = GetWorld();
	if (World)
	{
		float CellSize = 1000.f;
		UArcEntityVisualizationSubsystem* VisSubsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
		if (VisSubsystem)
		{
			CellSize = VisSubsystem->GetGrid().CellSize;
		}

		const FVector CellCenter(
			Cell.X * CellSize + CellSize * 0.5f,
			Cell.Y * CellSize + CellSize * 0.5f,
			Cell.Z * CellSize + CellSize * 0.5f
		);

		UClass* MgrClass = ManagerClass.Get() ? ManagerClass.Get() : AActor::StaticClass();
		FActorSpawnParameters SpawnParams;
		NewManager.ManagerActor = World->SpawnActor<AActor>(MgrClass, FTransform(CellCenter), SpawnParams);
		if (NewManager.ManagerActor)
		{
			NewManager.ManagerActor->SetActorHiddenInGame(false);
		}
	}

	return NewManager;
}

UInstancedStaticMeshComponent* UArcCompositeVisSubsystem::GetOrCreateISMComponent(
	FCellManager& Manager,
	UStaticMesh* Mesh,
	const TArray<TObjectPtr<UMaterialInterface>>& Materials,
	bool bCastShadows)
{
	if (!Mesh || !Manager.ManagerActor)
	{
		return nullptr;
	}

	const FObjectKey Key(Mesh);
	if (TObjectPtr<UInstancedStaticMeshComponent>* Existing = Manager.ISMComponents.Find(Key))
	{
		return *Existing;
	}

	UInstancedStaticMeshComponent* NewISMC = NewObject<UInstancedStaticMeshComponent>(Manager.ManagerActor);
	NewISMC->SetStaticMesh(Mesh);
	NewISMC->SetCastShadow(bCastShadows);

	for (int32 i = 0; i < Materials.Num(); i++)
	{
		if (Materials[i])
		{
			NewISMC->SetMaterial(i, Materials[i]);
		}
	}

	NewISMC->SetWorldTransform(FTransform::Identity);
	NewISMC->RegisterComponent();
	Manager.ManagerActor->AddInstanceComponent(NewISMC);

	Manager.ISMComponents.Add(Key, NewISMC);
	return NewISMC;
}

void UArcCompositeVisSubsystem::AddISMInstance(
	const FIntVector& Cell,
	TSubclassOf<AActor> ManagerClass,
	bool bCastShadows,
	const FTransform& WorldTransform,
	FMassEntityHandle OwnerEntity,
	int32 EntryIndex,
	FArcCompositeVisInstanceEntry& InOutEntry)
{
	if (!InOutEntry.Mesh)
	{
		return;
	}

	FCellManager& Manager = GetOrCreateCellManager(Cell, ManagerClass);
	UInstancedStaticMeshComponent* ISMC = GetOrCreateISMComponent(Manager, InOutEntry.Mesh, InOutEntry.Materials, bCastShadows);
	if (!ISMC)
	{
		return;
	}

	const FTransform InstanceTransform = InOutEntry.RelativeTransform * WorldTransform;
	const int32 NewIndex = ISMC->AddInstance(InstanceTransform, true);

	InOutEntry.ISMInstanceId = NewIndex;

	const FObjectKey MeshKey(InOutEntry.Mesh);
	TArray<FInstanceOwnerEntry>& Owners = Manager.InstanceOwners.FindOrAdd(MeshKey);
	if (Owners.Num() <= NewIndex)
	{
		Owners.SetNum(NewIndex + 1);
	}
	Owners[NewIndex] = FInstanceOwnerEntry{OwnerEntity, EntryIndex};
}

void UArcCompositeVisSubsystem::RemoveISMInstance(
	const FIntVector& Cell,
	TSubclassOf<AActor> ManagerClass,
	UStaticMesh* Mesh,
	int32 InstanceId,
	FMassEntityManager& EntityManager)
{
	if (!Mesh || InstanceId == INDEX_NONE)
	{
		return;
	}

	const FCellManagerKey Key{Cell, ManagerClass.Get()};
	FCellManager* Manager = CellManagers.Find(Key);
	if (!Manager)
	{
		return;
	}

	const FObjectKey MeshKey(Mesh);
	TObjectPtr<UInstancedStaticMeshComponent>* Found = Manager->ISMComponents.Find(MeshKey);
	if (!Found || !*Found)
	{
		return;
	}

	UInstancedStaticMeshComponent* ISMC = *Found;
	const int32 InstanceCount = ISMC->GetInstanceCount();
	if (InstanceId >= InstanceCount)
	{
		return;
	}

	TArray<FInstanceOwnerEntry>* Owners = Manager->InstanceOwners.Find(MeshKey);

	// RemoveInstance swaps the last instance into the removed slot.
	// Update the swapped entity's stored ISMInstanceId so it remains correct.
	const int32 LastIndex = InstanceCount - 1;
	if (Owners && InstanceId != LastIndex && LastIndex >= 0)
	{
		const FInstanceOwnerEntry SwappedOwner = (*Owners)[LastIndex];
		const FMassEntityHandle SwappedEntity = SwappedOwner.Entity;

		if (EntityManager.IsEntityValid(SwappedEntity))
		{
			FArcCompositeVisRepFragment* SwappedRep = EntityManager.GetFragmentDataPtr<FArcCompositeVisRepFragment>(SwappedEntity);
			if (SwappedRep && !SwappedRep->bIsActorRepresentation)
			{
				// Find the matching entry by EntryIndex and update its ISMInstanceId
				for (FArcCompositeVisInstanceEntry& Entry : SwappedRep->MeshEntries)
				{
					if (Entry.EntryIndex == SwappedOwner.EntryIndex)
					{
						Entry.ISMInstanceId = InstanceId;
						break;
					}
				}
			}
		}

		// Update owner tracking: the swapped instance now lives at InstanceId
		(*Owners)[InstanceId] = SwappedOwner;
	}

	// Shrink owner array
	if (Owners && Owners->Num() > 0)
	{
		Owners->RemoveAt(LastIndex);
	}

	ISMC->RemoveInstance(InstanceId);
}
