// Copyright Lukasz Baran. All Rights Reserved.

#include "Replication/ArcMassEntityReplicationProxy.h"
#include "Subsystem/ArcMassEntityReplicationProxySubsystem.h"
#include "Traits/ArcMassEntityReplicationTrait.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "MassEntityConfigAsset.h"
#include "Engine/World.h"

AArcMassEntityReplicationProxy::AArcMassEntityReplicationProxy()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	bNetLoadOnClient = false;
	ReplicatedEntities.OwnerProxy = this;
}

void AArcMassEntityReplicationProxy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams PushParams;
	PushParams.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AArcMassEntityReplicationProxy, ReplicatedEntities, PushParams);

	DOREPLIFETIME(AArcMassEntityReplicationProxy, EntityConfigAsset);
}

void AArcMassEntityReplicationProxy::RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	Super::RegisterReplicationFragments(Context, RegistrationFlags);
	EnsureDescriptorSet();
}

void AArcMassEntityReplicationProxy::BeginPlay()
{
	Super::BeginPlay();
	EnsureDescriptorSet();
}

void AArcMassEntityReplicationProxy::Init(UMassEntityConfigAsset* InEntityConfigAsset)
{
	EntityConfigAsset = InEntityConfigAsset;
	EnsureDescriptorSet();
}

UMassEntityConfigAsset* AArcMassEntityReplicationProxy::GetEffectiveConfigAsset() const
{
	return EntityConfigAsset ? EntityConfigAsset.Get() : LocalEntityConfigAsset.Get();
}

void AArcMassEntityReplicationProxy::EnsureDescriptorSet()
{
	if (bDescriptorSetBuilt)
	{
		return;
	}

	UWorld* World = GetWorld();
	const TCHAR* NetModeStr = TEXT("Unknown");
	if (World)
	{
		ENetMode NetMode = World->GetNetMode();
		NetModeStr = NetMode == NM_DedicatedServer ? TEXT("Server") :
			NetMode == NM_Client ? TEXT("Client") :
			NetMode == NM_ListenServer ? TEXT("ListenServer") : TEXT("Standalone");
	}

	UMassEntityConfigAsset* Config = GetEffectiveConfigAsset();
	if (!Config)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] ArcProxy::EnsureDescriptorSet - No config asset (Replicated=%s, Local=%s)"),
			NetModeStr,
			EntityConfigAsset ? TEXT("yes") : TEXT("no"),
			LocalEntityConfigAsset ? TEXT("yes") : TEXT("no"));
		return;
	}

	const UArcMassEntityReplicationTrait* RepTrait = Cast<UArcMassEntityReplicationTrait>(
		Config->GetConfig().FindTrait(UArcMassEntityReplicationTrait::StaticClass()));

	if (!RepTrait || RepTrait->ReplicatedFragments.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] ArcProxy::EnsureDescriptorSet - No replication trait or empty fragments"),
			NetModeStr);
		return;
	}

	DescriptorSet = ArcMassReplication::FArcMassReplicationDescriptorSet::Build(RepTrait->ReplicatedFragments);
	bDescriptorSetBuilt = true;

	UE_LOG(LogTemp, Log, TEXT("[%s] ArcProxy::EnsureDescriptorSet - Built descriptor set Hash=%u, FragmentCount=%d"),
		NetModeStr, DescriptorSet.Hash, DescriptorSet.FragmentTypes.Num());
	for (int32 Idx = 0; Idx < DescriptorSet.FragmentTypes.Num(); ++Idx)
	{
		const UScriptStruct* FragType = DescriptorSet.FragmentTypes[Idx];
		bool bHasDescriptor = DescriptorSet.Descriptors.IsValidIndex(Idx) && DescriptorSet.Descriptors[Idx].IsValid();
		UE_LOG(LogTemp, Log, TEXT("[%s]   Slot %d: %s (IrisDescriptor=%s)"),
			NetModeStr, Idx,
			FragType ? *FragType->GetName() : TEXT("null"),
			bHasDescriptor ? TEXT("yes") : TEXT("NO"));
	}

	if (World)
	{
		UArcMassEntityReplicationProxySubsystem* Subsystem = World->GetSubsystem<UArcMassEntityReplicationProxySubsystem>();
		if (Subsystem)
		{
			Subsystem->RegisterDescriptorSet(DescriptorSet.Hash, &DescriptorSet);
			UE_LOG(LogTemp, Log, TEXT("[%s] ArcProxy::EnsureDescriptorSet - Registered with subsystem"), NetModeStr);
		}
	}
}

const ArcMassReplication::FArcMassReplicationDescriptorSet& AArcMassEntityReplicationProxy::GetDescriptorSet()
{
	EnsureDescriptorSet();
	return DescriptorSet;
}

void AArcMassEntityReplicationProxy::SetEntityFragments(FArcMassNetId NetId, const TArray<FInstancedStruct>& FragmentData)
{
	EnsureDescriptorSet();

	int32 Index = ReplicatedEntities.FindIndexByNetId(NetId);
	if (Index == INDEX_NONE)
	{
		Index = ReplicatedEntities.AddEntity(NetId, DescriptorSet.FragmentTypes);
		MARK_PROPERTY_DIRTY_FROM_NAME(AArcMassEntityReplicationProxy, ReplicatedEntities, this);
	}

	FArcIrisReplicatedEntity& Entity = ReplicatedEntities.Entities[Index];
	for (int32 SlotIdx = 0; SlotIdx < FragmentData.Num() && SlotIdx < Entity.FragmentSlots.Num(); ++SlotIdx)
	{
		const FInstancedStruct& SrcSlot = FragmentData[SlotIdx];
		FInstancedStruct& DstSlot = Entity.FragmentSlots[SlotIdx];
		if (SrcSlot.IsValid() && DstSlot.IsValid() && SrcSlot.GetScriptStruct() == DstSlot.GetScriptStruct())
		{
			const UScriptStruct* StructType = SrcSlot.GetScriptStruct();
			if (!StructType->CompareScriptStruct(DstSlot.GetMemory(), SrcSlot.GetMemory(), PPF_None))
			{
				StructType->CopyScriptStruct(DstSlot.GetMutableMemory(), SrcSlot.GetMemory());
				ReplicatedEntities.MarkFragmentDirty(Index, SlotIdx);
				MARK_PROPERTY_DIRTY_FROM_NAME(AArcMassEntityReplicationProxy, ReplicatedEntities, this);
			}
		}
	}
}

void AArcMassEntityReplicationProxy::RemoveEntity(FArcMassNetId NetId)
{
	ReplicatedEntities.RemoveEntity(NetId);
	MARK_PROPERTY_DIRTY_FROM_NAME(AArcMassEntityReplicationProxy, ReplicatedEntities, this);
}

void AArcMassEntityReplicationProxy::OnClientEntityAdded(FArcMassNetId NetId, const TArray<FInstancedStruct>& FragmentSlots)
{
	EnsureDescriptorSet();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UArcMassEntityReplicationProxySubsystem* Subsystem = World->GetSubsystem<UArcMassEntityReplicationProxySubsystem>();
	if (Subsystem)
	{
		Subsystem->OnClientEntityAdded(NetId, FragmentSlots, &DescriptorSet, GetEffectiveConfigAsset());
	}
}

void AArcMassEntityReplicationProxy::OnClientEntityChanged(FArcMassNetId NetId, const TArray<FInstancedStruct>& FragmentSlots, uint32 ChangedFragmentMask)
{
	EnsureDescriptorSet();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UArcMassEntityReplicationProxySubsystem* Subsystem = World->GetSubsystem<UArcMassEntityReplicationProxySubsystem>();
	if (Subsystem)
	{
		Subsystem->OnClientEntityChanged(NetId, FragmentSlots, ChangedFragmentMask, &DescriptorSet);
	}
}

void AArcMassEntityReplicationProxy::OnClientEntityRemoved(FArcMassNetId NetId)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UArcMassEntityReplicationProxySubsystem* Subsystem = World->GetSubsystem<UArcMassEntityReplicationProxySubsystem>();
	if (Subsystem)
	{
		Subsystem->OnClientEntityRemoved(NetId);
	}
}
