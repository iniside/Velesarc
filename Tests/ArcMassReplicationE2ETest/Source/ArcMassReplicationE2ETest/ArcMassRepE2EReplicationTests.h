// Copyright Lukasz Baran. All Rights Reserved.
//
// Real end-to-end vessel replication across process boundaries: server process
// spawns a Mass entity from the on-disk fixture config, writes a sentinel into
// its payload fragment; client process asserts the sentinel arrived in its own
// Mass entity via the typed vessel's OnRep -> ApplyReplicatedStateToEntity.
//
// The payload vessel/fragment classes live in the CQTest module
// (ArcMassReplicationTest), which does not export symbols — they are resolved
// by reflection path here, never linked.

#pragma once

#include "CoreMinimal.h"
#include "E2E/MTFE2ETest.h"
#include "E2E/MTFRoles.h"

#include "ArcMassRepE2EReplicationTests.generated.h"

UCLASS()
class UArcMassRepE2E_ServerOps : public UMTFServerOpHandler
{
	GENERATED_BODY()

public:
	/** Register the typed vessel class for the fixture config, spawn one entity
	 *  from it, write Sentinel into its FArcMassTestPayloadFragment. */
	UFUNCTION()
	bool SpawnPayloadEntity(uint32 Sentinel);

private:
	bool SpawnPayloadEntity_Impl(uint32 Sentinel);
};

UCLASS()
class UArcMassRepE2E_ClientOps : public UMTFClientOpHandler
{
	GENERATED_BODY()

public:
	/** Register the typed vessel class for the fixture config on the client so
	 *  the factory instantiates the payload vessel when the entity replicates.
	 *  Must run before the server spawns. */
	UFUNCTION()
	bool PrepareClient();

	/** Count client Mass entities whose payload fragment equals Sentinel,
	 *  found through the client-side vessels' EntityHandle — this exercises
	 *  the full chain incl. ApplyReplicatedStateToEntity. */
	UFUNCTION()
	int32 CountPayloadEntities(uint32 Sentinel);

private:
	bool PrepareClient_Impl();
	int32 CountPayloadEntities_Impl(uint32 Sentinel);
};

UCLASS()
class UArcMassRepE2E_EntityReplication : public UMTFE2ETest
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UModularTestSuite> GetTestSuite_Implementation() const override;
	virtual FString GetTags_Implementation() const override;
	virtual void RunTest_E2E_Implementation(UMTFEnv* Env, UMTFServerRole* Server, UMTFClientRole* Client) override;
};
