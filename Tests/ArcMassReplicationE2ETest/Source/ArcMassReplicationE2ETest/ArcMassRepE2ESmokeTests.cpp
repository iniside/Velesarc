// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassRepE2ESmokeTests.h"

#include "E2E/MTFEnv.h"

void UArcMassRepE2E_Suite::DeclareTopology(FMTFEnvBuilder& Builder)
{
	Builder.RequireServer();
	Builder.RequireClient();
}

TSubclassOf<UModularTestSuite> UArcMassRepE2E_ProcessesLaunch::GetTestSuite_Implementation() const
{
	return UArcMassRepE2E_Suite::StaticClass();
}

FString UArcMassRepE2E_ProcessesLaunch::GetTags_Implementation() const
{
	return TEXT("[Smoke]");
}

void UArcMassRepE2E_ProcessesLaunch::RunTest_E2E_Implementation(UMTFEnv* Env, UMTFServerRole* Server, UMTFClientRole* Client)
{
	const bool bConnected = Env->TickUntilRolesReady();
	TEST_REQUIRE_TRUE(TEXT("Server role ready and client connected"), bConnected);
}
