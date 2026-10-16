// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "E2E/MTFE2ETest.h"
#include "E2E/ModularTestE2ESuite.h"

#include "ArcMassRepE2ESmokeTests.generated.h"

/** Server + one client. First MTF suite in the project — proves the framework
 *  boots our plugin pair and (in -Env.Mode=MultiProcess) launches real
 *  independent server/client processes. */
UCLASS()
class UArcMassRepE2E_Suite : public UE2EModularTestSuite
{
	GENERATED_BODY()

public:
	virtual void DeclareTopology(FMTFEnvBuilder& Builder) override;
};

/** Launch-only smoke: the roles come up and the client connects. No game logic. */
UCLASS()
class UArcMassRepE2E_ProcessesLaunch : public UMTFE2ETest
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UModularTestSuite> GetTestSuite_Implementation() const override;
	virtual FString GetTags_Implementation() const override;
	virtual void RunTest_E2E_Implementation(UMTFEnv* Env, UMTFServerRole* Server, UMTFClientRole* Client) override;
};
