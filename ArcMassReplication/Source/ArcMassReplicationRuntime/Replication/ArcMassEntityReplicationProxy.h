// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mass/EntityHandle.h"
#include "Fragments/ArcMassNetId.h"
#include "StructUtils/InstancedStruct.h"
#include "Replication/ArcIrisEntityArray.h"
#include "Replication/ArcMassReplicationDescriptorSet.h"
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
