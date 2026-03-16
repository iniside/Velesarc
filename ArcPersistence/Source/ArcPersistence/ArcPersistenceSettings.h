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

UENUM()
enum class EArcPersistenceBackendType : uint8
{
	/** Filesystem-backed — one .json file per key under Saved/ArcPersistence/. */
	JsonFile,

	/** SQLite database — all data in a single .db file under Saved/ArcPersistence/. */
	SQLite
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

	/** Which storage backend to use. Changing this requires a restart. */
	UPROPERTY(EditAnywhere, config, Category = "Persistence|Storage")
	EArcPersistenceBackendType BackendType = EArcPersistenceBackendType::JsonFile;

	virtual FName GetCategoryName() const override { return FName("Plugins"); }
};
