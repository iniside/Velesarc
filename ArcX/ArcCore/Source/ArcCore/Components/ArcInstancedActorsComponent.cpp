// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInstancedActorsComponent.h"

#include "MassEntityConfigAsset.h"
#include "MassEntityManager.h"
#include "MassEntityTraitBase.h"
#include "MassEntityUtils.h"
#include "ArcMass/Persistence/ArcMassFragmentSerializer.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "Serialization/ArcJsonSaveArchive.h"
#include "Serialization/ArcJsonLoadArchive.h"

// Sets default values for this component's properties
UArcInstancedActorsComponent::UArcInstancedActorsComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

// Called when the game starts
void UArcInstancedActorsComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


void UArcInstancedActorsComponent::ModifyMassEntityConfig(FMassEntityManager& InMassEntityManager, UInstancedActorsData* InstancedActorData
	, FMassEntityConfig& InOutMassEntityConfig) const
{
	for (UMassEntityTraitBase* Trait : Traits)
	{
		//if (Trait && !InOutMassEntityConfig.FindTrait(Trait->GetClass(), true))
		{
			InOutMassEntityConfig.AddTrait(*Trait);
		}
	}
}


void UArcInstancedActorsComponent::OnServerPreSpawnInitForInstance(FInstancedActorsInstanceHandle InInstanceHandle)
{
	Super::OnServerPreSpawnInitForInstance(InInstanceHandle);
}

void UArcInstancedActorsComponent::InitializeComponentForInstance(FInstancedActorsInstanceHandle InInstanceHandle)
{
	Super::InitializeComponentForInstance(InInstanceHandle);
}

// ── Persistence (Flow 2) ──────────────────────────────────────────────────

uint32 UArcInstancedActorsComponent::GetInstancePersistenceDataID() const
{
	// Stable non-zero ID for this component class.
	// Uses a CRC of the class name so subclasses get unique IDs automatically.
	static const uint32 PersistenceID = FCrc::StrCrc32(
		*GetClass()->GetPathName());
	return PersistenceID != 0 ? PersistenceID : 1;
}

bool UArcInstancedActorsComponent::ShouldSerializeInstancePersistenceData(
	const FArchive& Archive, UInstancedActorsData* InstanceData,
	int64 TimeDelta) const
{
	return PersistenceAllowedFragments.Num() > 0
		|| PersistenceDisallowedFragments.Num() > 0;
}

void UArcInstancedActorsComponent::SerializeInstancePersistenceData(
	FStructuredArchive::FRecord Record, UInstancedActorsData* InstanceData,
	int64 TimeDelta) const
{
	Super::SerializeInstancePersistenceData(Record, InstanceData, TimeDelta);

	FMassEntityHandle EntityHandle = GetMassEntityHandle();
	if (!EntityHandle.IsValid())
	{
		return;
	}

	FArchive& UnderlyingArchive = Record.GetUnderlyingArchive();

	if (UnderlyingArchive.IsSaving())
	{
		// Build config from configured fragment lists
		FArcMassPersistenceConfigFragment Config;
		Config.AllowedFragments = PersistenceAllowedFragments;
		Config.DisallowedFragments = PersistenceDisallowedFragments;

		// Get entity manager from the owning world
		UWorld* World = GetWorld();
		FMassEntityManager* EM = UE::Mass::Utils::GetEntityManager(World);
		if (!EM || !EM->IsEntityValid(EntityHandle))
		{
			int32 DataSize = 0;
			Record << SA_VALUE(TEXT("FragmentDataSize"), DataSize);
			return;
		}

		// Serialize fragments to JSON bytes
		FArcJsonSaveArchive SaveAr;
		FArcMassFragmentSerializer::SaveEntityFragments(
			*EM, EntityHandle, Config, SaveAr);
		TArray<uint8> Data = SaveAr.Finalize();

		int32 DataSize = Data.Num();
		Record << SA_VALUE(TEXT("FragmentDataSize"), DataSize);
		if (DataSize > 0)
		{
			UnderlyingArchive.Serialize(Data.GetData(), DataSize);
		}
	}
	else
	{
		int32 DataSize = 0;
		Record << SA_VALUE(TEXT("FragmentDataSize"), DataSize);
		if (DataSize > 0)
		{
			TArray<uint8> Data;
			Data.SetNumUninitialized(DataSize);
			UnderlyingArchive.Serialize(Data.GetData(), DataSize);

			UWorld* World = GetWorld();
			FMassEntityManager* EM = UE::Mass::Utils::GetEntityManager(World);
			if (EM && EM->IsEntityValid(EntityHandle))
			{
				FArcJsonLoadArchive LoadAr;
				if (LoadAr.InitializeFromData(Data))
				{
					FArcMassFragmentSerializer::LoadEntityFragments(
						*EM, EntityHandle, LoadAr);
				}
			}
		}
	}
}
