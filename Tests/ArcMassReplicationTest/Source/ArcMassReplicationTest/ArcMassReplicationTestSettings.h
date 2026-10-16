// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ArcMassReplicationTestSettings.generated.h"

/**
 * Fixture references for the ArcMassReplication test suite. Values live in
 * Config/DefaultArcMassReplicationTest.ini inside the test plugin, so the
 * fixture assets travel with the plugin (content via Lore).
 */
UCLASS(Config = ArcMassReplicationTest, DefaultConfig)
class UArcMassReplicationTestSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** On-disk UMassEntityConfigAsset used by the PIE smoke tests. Loaded by both
	 *  server and client blocks so the creation header's config reference resolves
	 *  to the same stably-named asset on both sides. */
	UPROPERTY(Config, EditAnywhere, Category = "Fixtures")
	FSoftObjectPath SmokeTestEntityConfig;
};
