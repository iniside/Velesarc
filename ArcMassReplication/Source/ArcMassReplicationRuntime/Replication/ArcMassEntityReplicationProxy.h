// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mass/EntityHandle.h"
#include "Fragments/ArcMassNetId.h"
#include "StructUtils/InstancedStruct.h"
#include "Replication/ArcIrisEntityArray.h"
#include "Replication/ArcMassReplicationDescriptorSet.h"
#include "Net/Core/PushModel/PushModel.h"
#include "ArcMassEntityReplicationProxy.generated.h"

class UMassEntityConfigAsset;

UCLASS(NotBlueprintable)
class ARCMASSREPLICATIONRUNTIME_API AArcMassEntityReplicationProxy : public AActor
{
	GENERATED_BODY()

public:
	AArcMassEntityReplicationProxy();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags) override;
	virtual void BeginPlay() override;

	void Init(UMassEntityConfigAsset* InEntityConfigAsset);

	void SetEntityFragments(FArcMassNetId NetId, const TArray<FInstancedStruct>& FragmentData);
	void RemoveEntity(FArcMassNetId NetId);

	void OnClientEntityAdded(FArcMassNetId NetId, const TArray<FInstancedStruct>& FragmentSlots);
	void OnClientEntityChanged(FArcMassNetId NetId, const TArray<FInstancedStruct>& FragmentSlots, uint32 ChangedFragmentMask);
	void OnClientEntityRemoved(FArcMassNetId NetId);

	int32 GetItemCount() const { return ReplicatedEntities.GetEntityCount(); }
	int32 FindItemIndexByNetId(FArcMassNetId NetId) const { return ReplicatedEntities.FindIndexByNetId(NetId); }

	const ArcMassReplication::FArcMassReplicationDescriptorSet& GetDescriptorSet();

	template<typename T>
	T* GetFragment(FArcMassNetId NetId)
	{
		EnsureDescriptorSet();
		const int32 EntityIndex = ReplicatedEntities.FindIndexByNetId(NetId);
		if (EntityIndex == INDEX_NONE)
		{
			return nullptr;
		}
		FArcIrisReplicatedEntity& Entity = ReplicatedEntities.Entities[EntityIndex];
		for (int32 SlotIdx = 0; SlotIdx < DescriptorSet.FragmentTypes.Num(); ++SlotIdx)
		{
			if (DescriptorSet.FragmentTypes[SlotIdx] == T::StaticStruct())
			{
				return Entity.FragmentSlots.IsValidIndex(SlotIdx)
					? Entity.FragmentSlots[SlotIdx].GetMutablePtr<T>()
					: nullptr;
			}
		}
		return nullptr;
	}

	template<typename T>
	void MarkFragmentDirty(FArcMassNetId NetId)
	{
		EnsureDescriptorSet();
		const int32 EntityIndex = ReplicatedEntities.FindIndexByNetId(NetId);
		UE_LOG(LogTemp, Log, TEXT("ArcProxy::MarkFragmentDirty<%s> NetId=%u EntityIndex=%d"),
			*T::StaticStruct()->GetName(), NetId.GetValue(), EntityIndex);
		if (EntityIndex == INDEX_NONE)
		{
			return;
		}
		for (int32 SlotIdx = 0; SlotIdx < DescriptorSet.FragmentTypes.Num(); ++SlotIdx)
		{
			if (DescriptorSet.FragmentTypes[SlotIdx] == T::StaticStruct())
			{
				ReplicatedEntities.MarkFragmentDirty(EntityIndex, SlotIdx);
				MARK_PROPERTY_DIRTY_FROM_NAME(AArcMassEntityReplicationProxy, ReplicatedEntities, this);
				UE_LOG(LogTemp, Log, TEXT("ArcProxy::MarkFragmentDirty<%s> NetId=%u SlotIdx=%d done (DirtyEntities=%d HasDirty=%d ReplicationKey=%u)"),
					*T::StaticStruct()->GetName(), NetId.GetValue(), SlotIdx,
					ReplicatedEntities.DirtyEntities.CountSetBits(),
					ReplicatedEntities.HasDirtyEntities() ? 1 : 0,
					ReplicatedEntities.Entities.IsValidIndex(EntityIndex) ? ReplicatedEntities.Entities[EntityIndex].ReplicationKey : 0u);
				return;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("ArcProxy::MarkFragmentDirty<%s> NetId=%u: fragment type not in DescriptorSet (FragmentTypes=%d)"),
			*T::StaticStruct()->GetName(), NetId.GetValue(), DescriptorSet.FragmentTypes.Num());
	}

protected:
	UPROPERTY(Replicated)
	TObjectPtr<UMassEntityConfigAsset> EntityConfigAsset = nullptr;

	TObjectPtr<UMassEntityConfigAsset> LocalEntityConfigAsset = nullptr;

private:
	void EnsureDescriptorSet();
	UMassEntityConfigAsset* GetEffectiveConfigAsset() const;

	UPROPERTY(Replicated)
	FArcIrisEntityArray ReplicatedEntities;

	ArcMassReplication::FArcMassReplicationDescriptorSet DescriptorSet;
	bool bDescriptorSetBuilt = false;
};
