// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"
#include "MassEntityTemplateRegistry.h"
#include "Fragments/ArcMassReplicationConfigFragment.h"
#include "Replication/ArcMassEntityReplicationProxy.h"
#include "Traits/ArcMassEntityReplicationTrait.h"
#include "ArcMassTestPayloadFragment.h"
#include "ArcMassTestStatsFragment.h"
#include "ArcMassTestSparseFragment.h"
#include "ArcMassTestItemFragment.h"
#include "MassEntityManager.h"
#include "MassEntityUtils.h"
#include "MassEntityConfigAsset.h"
#include "ArcMassTestReplicationTrait.generated.h"

USTRUCT()
struct FArcMassTestTemplateTag : public FMassTag
{
	GENERATED_BODY()
};

UCLASS()
class UArcMassTestReplicationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
		const UWorld& World) const override
	{
		BuildContext.AddFragment<FArcMassTestPayloadFragment>();
		BuildContext.AddTag<FArcMassTestTemplateTag>();
	}
};

UCLASS()
class UArcMassTestSparseReplicationTrait : public UArcMassEntityReplicationTrait
{
	GENERATED_BODY()

public:
	UArcMassTestSparseReplicationTrait()
	{
		FArcMassReplicatedFragmentEntry PayloadEntry;
		PayloadEntry.FragmentType = FArcMassTestPayloadFragment::StaticStruct();
		ReplicatedFragments.Add(PayloadEntry);

		FArcMassReplicatedFragmentEntry SparseEntry;
		SparseEntry.FragmentType = FArcMassTestSparseFragment::StaticStruct();
		ReplicatedFragments.Add(SparseEntry);
	}

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
		const UWorld& World) const override
	{
		BuildContext.AddFragment<FArcMassTestPayloadFragment>();
		Super::BuildTemplate(BuildContext, World);
	}
};

UCLASS()
class AArcMassTestSparseProxy : public AArcMassEntityReplicationProxy
{
	GENERATED_BODY()

public:
	AArcMassTestSparseProxy()
	{
		UMassEntityConfigAsset* Config = NewObject<UMassEntityConfigAsset>(this, TEXT("SparseConfig"));
		UArcMassTestSparseReplicationTrait* Trait = NewObject<UArcMassTestSparseReplicationTrait>(Config);
		Config->GetMutableConfig().AddTrait(*Trait);
		LocalEntityConfigAsset = Config;
	}
};

UCLASS()
class UArcMassTestPayloadReplicationTrait : public UArcMassEntityReplicationTrait
{
	GENERATED_BODY()
public:
	UArcMassTestPayloadReplicationTrait()
	{
		FArcMassReplicatedFragmentEntry Entry;
		Entry.FragmentType = FArcMassTestPayloadFragment::StaticStruct();
		ReplicatedFragments.Add(Entry);
	}

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
		const UWorld& World) const override
	{
		BuildContext.AddFragment<FArcMassTestPayloadFragment>();
		Super::BuildTemplate(BuildContext, World);
	}
};

UCLASS()
class AArcMassTestPayloadProxy : public AArcMassEntityReplicationProxy
{
	GENERATED_BODY()
public:
	AArcMassTestPayloadProxy()
	{
		UMassEntityConfigAsset* Config = NewObject<UMassEntityConfigAsset>(this, TEXT("PayloadConfig"));
		UArcMassTestPayloadReplicationTrait* Trait = NewObject<UArcMassTestPayloadReplicationTrait>(Config);
		Config->GetMutableConfig().AddTrait(*Trait);
		LocalEntityConfigAsset = Config;
	}
};

UCLASS()
class UArcMassTestStatsReplicationTrait : public UArcMassEntityReplicationTrait
{
	GENERATED_BODY()
public:
	UArcMassTestStatsReplicationTrait()
	{
		FArcMassReplicatedFragmentEntry Entry;
		Entry.FragmentType = FArcMassTestStatsFragment::StaticStruct();
		ReplicatedFragments.Add(Entry);
	}

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
		const UWorld& World) const override
	{
		BuildContext.AddFragment<FArcMassTestStatsFragment>();
		Super::BuildTemplate(BuildContext, World);
	}
};

UCLASS()
class AArcMassTestStatsProxy : public AArcMassEntityReplicationProxy
{
	GENERATED_BODY()
public:
	AArcMassTestStatsProxy()
	{
		UMassEntityConfigAsset* Config = NewObject<UMassEntityConfigAsset>(this, TEXT("StatsConfig"));
		UArcMassTestStatsReplicationTrait* Trait = NewObject<UArcMassTestStatsReplicationTrait>(Config);
		Config->GetMutableConfig().AddTrait(*Trait);
		LocalEntityConfigAsset = Config;
	}
};

UCLASS()
class UArcMassTestMultiFragmentReplicationTrait : public UArcMassEntityReplicationTrait
{
	GENERATED_BODY()
public:
	UArcMassTestMultiFragmentReplicationTrait()
	{
		FArcMassReplicatedFragmentEntry PayloadEntry;
		PayloadEntry.FragmentType = FArcMassTestPayloadFragment::StaticStruct();
		ReplicatedFragments.Add(PayloadEntry);

		FArcMassReplicatedFragmentEntry StatsEntry;
		StatsEntry.FragmentType = FArcMassTestStatsFragment::StaticStruct();
		ReplicatedFragments.Add(StatsEntry);
	}

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
		const UWorld& World) const override
	{
		BuildContext.AddFragment<FArcMassTestPayloadFragment>();
		BuildContext.AddFragment<FArcMassTestStatsFragment>();
		Super::BuildTemplate(BuildContext, World);
	}
};

UCLASS()
class AArcMassTestMultiFragmentProxy : public AArcMassEntityReplicationProxy
{
	GENERATED_BODY()
public:
	AArcMassTestMultiFragmentProxy()
	{
		UMassEntityConfigAsset* Config = NewObject<UMassEntityConfigAsset>(this, TEXT("MultiFragConfig"));
		UArcMassTestMultiFragmentReplicationTrait* Trait = NewObject<UArcMassTestMultiFragmentReplicationTrait>(Config);
		Config->GetMutableConfig().AddTrait(*Trait);
		LocalEntityConfigAsset = Config;
	}
};

UCLASS()
class UArcMassTestItemReplicationTrait : public UArcMassEntityReplicationTrait
{
	GENERATED_BODY()
public:
	UArcMassTestItemReplicationTrait()
	{
		FArcMassReplicatedFragmentEntry Entry;
		Entry.FragmentType = FArcMassTestItemFragment::StaticStruct();
		ReplicatedFragments.Add(Entry);
	}

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
		const UWorld& World) const override
	{
		BuildContext.AddFragment<FArcMassTestItemFragment>();
		Super::BuildTemplate(BuildContext, World);
	}
};

UCLASS()
class AArcMassTestItemProxy : public AArcMassEntityReplicationProxy
{
	GENERATED_BODY()
public:
	AArcMassTestItemProxy()
	{
		UMassEntityConfigAsset* Config = NewObject<UMassEntityConfigAsset>(this, TEXT("ItemConfig"));
		UArcMassTestItemReplicationTrait* Trait = NewObject<UArcMassTestItemReplicationTrait>(Config);
		Config->GetMutableConfig().AddTrait(*Trait);
		LocalEntityConfigAsset = Config;
	}
};

UCLASS()
class UArcMassTestNestedItemReplicationTrait : public UArcMassEntityReplicationTrait
{
	GENERATED_BODY()
public:
	UArcMassTestNestedItemReplicationTrait()
	{
		FArcMassReplicatedFragmentEntry Entry;
		Entry.FragmentType = FArcMassTestNestedItemFragment::StaticStruct();
		ReplicatedFragments.Add(Entry);
	}

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
		const UWorld& World) const override
	{
		BuildContext.AddFragment<FArcMassTestNestedItemFragment>();
		Super::BuildTemplate(BuildContext, World);
	}
};

UCLASS()
class AArcMassTestNestedItemProxy : public AArcMassEntityReplicationProxy
{
	GENERATED_BODY()
public:
	AArcMassTestNestedItemProxy()
	{
		UMassEntityConfigAsset* Config = NewObject<UMassEntityConfigAsset>(this, TEXT("NestedItemConfig"));
		UArcMassTestNestedItemReplicationTrait* Trait = NewObject<UArcMassTestNestedItemReplicationTrait>(Config);
		Config->GetMutableConfig().AddTrait(*Trait);
		LocalEntityConfigAsset = Config;
	}
};

UCLASS()
class UArcMassTestEntityHandleItemReplicationTrait : public UArcMassEntityReplicationTrait
{
	GENERATED_BODY()
public:
	UArcMassTestEntityHandleItemReplicationTrait()
	{
		FArcMassReplicatedFragmentEntry Entry;
		Entry.FragmentType = FArcMassTestEntityHandleItemFragment::StaticStruct();
		ReplicatedFragments.Add(Entry);
	}

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
		const UWorld& World) const override
	{
		BuildContext.AddFragment<FArcMassTestEntityHandleItemFragment>();
		Super::BuildTemplate(BuildContext, World);
	}
};

UCLASS()
class AArcMassTestEntityHandleItemProxy : public AArcMassEntityReplicationProxy
{
	GENERATED_BODY()
public:
	AArcMassTestEntityHandleItemProxy()
	{
		UMassEntityConfigAsset* Config = NewObject<UMassEntityConfigAsset>(this, TEXT("EntityHandleItemConfig"));
		UArcMassTestEntityHandleItemReplicationTrait* Trait = NewObject<UArcMassTestEntityHandleItemReplicationTrait>(Config);
		Config->GetMutableConfig().AddTrait(*Trait);
		LocalEntityConfigAsset = Config;
	}
};

UCLASS()
class UArcMassTestInstancedStructItemReplicationTrait : public UArcMassEntityReplicationTrait
{
	GENERATED_BODY()
public:
	UArcMassTestInstancedStructItemReplicationTrait()
	{
		FArcMassReplicatedFragmentEntry Entry;
		Entry.FragmentType = FArcMassTestInstancedStructItemFragment::StaticStruct();
		ReplicatedFragments.Add(Entry);
	}

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
		const UWorld& World) const override
	{
		BuildContext.AddFragment<FArcMassTestInstancedStructItemFragment>();
		Super::BuildTemplate(BuildContext, World);
	}
};

UCLASS()
class AArcMassTestInstancedStructItemProxy : public AArcMassEntityReplicationProxy
{
	GENERATED_BODY()
public:
	AArcMassTestInstancedStructItemProxy()
	{
		UMassEntityConfigAsset* Config = NewObject<UMassEntityConfigAsset>(this, TEXT("InstancedStructItemConfig"));
		UArcMassTestInstancedStructItemReplicationTrait* Trait = NewObject<UArcMassTestInstancedStructItemReplicationTrait>(Config);
		Config->GetMutableConfig().AddTrait(*Trait);
		LocalEntityConfigAsset = Config;
	}
};
