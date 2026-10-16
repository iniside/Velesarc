// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "ArcNamedPrimaryAssetId.h"

#include "ArcUIModelsTestsSettings.generated.h"

UCLASS(config = ArcTests, defaultconfig)
class ARCUIMODELSTEST_API UArcUIModelsTestsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Arc Game")); }

	/** Plain item definition fixture (no fragments) — guaranteed non-null def for item-VM construction. */
	UPROPERTY(EditDefaultsOnly, Category = "UI Models Tests", Config)
	FArcNamedPrimaryAssetId BasicItem;
};
