// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "ArcPersistenceSettings.generated.h"

USTRUCT(BlueprintType)
struct ARCPERSISTENCE_API FArcPersistenceClassEntry
{
	GENERATED_BODY()

	/** The class to allow for persistence. */
	UPROPERTY(EditAnywhere, config)
	TSoftClassPtr<UObject> Class;

	/** If true, all child classes are also allowed. */
	UPROPERTY(EditAnywhere, config)
	bool bIncludeChildren = true;
};

/**
 * Settings for ArcPersistence. Configurable in Project Settings > Plugins > Arc Persistence.
 * Also editable via DefaultArcPersistence.ini.
 */
UCLASS(config = ArcPersistence, defaultconfig, meta = (DisplayName = "Arc Persistence"))
class ARCPERSISTENCE_API UArcPersistenceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UArcPersistenceSettings();

	/** Classes allowed to participate in world actor persistence. */
	UPROPERTY(EditAnywhere, config, Category = "Persistence|Classes")
	TArray<FArcPersistenceClassEntry> AllowedClasses;

	virtual FName GetCategoryName() const override { return FName("Plugins"); }
};
