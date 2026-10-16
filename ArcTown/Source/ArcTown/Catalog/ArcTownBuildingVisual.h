// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

#include "ArcTownBuildingVisual.generated.h"

class AActor;
class UStaticMesh;

USTRUCT()
struct ARCTOWN_API FArcTownBuildingVisual
{
	GENERATED_BODY()

	virtual ~FArcTownBuildingVisual() = default;

	/** False is an error. True with a message is a warning; true clears the message. */
	virtual bool IsValid(FText& OutMessage) const;

	/** The asset this representation loads. The frozen catalog carries this one path, and it is what tells the variants apart. */
	virtual FSoftObjectPath GetAssetPath() const;

	virtual FVector2D ResolveDoorForwardXY(FName DoorSocketOverride) const;
};

USTRUCT()
struct ARCTOWN_API FArcTownBuildingVisual_StaticMesh : public FArcTownBuildingVisual
{
	GENERATED_BODY()

	virtual bool IsValid(FText& OutMessage) const override;
	virtual FSoftObjectPath GetAssetPath() const override;
	virtual FVector2D ResolveDoorForwardXY(FName DoorSocketOverride) const override;

	UPROPERTY(EditAnywhere, Category = "Visual")
	TSoftObjectPtr<UStaticMesh> Mesh;
};

USTRUCT()
struct ARCTOWN_API FArcTownBuildingVisual_Actor : public FArcTownBuildingVisual
{
	GENERATED_BODY()

	virtual bool IsValid(FText& OutMessage) const override;
	virtual FSoftObjectPath GetAssetPath() const override;
	virtual FVector2D ResolveDoorForwardXY(FName DoorSocketOverride) const override;

	UPROPERTY(EditAnywhere, Category = "Visual")
	TSoftClassPtr<AActor> ActorClass;
};

USTRUCT()
struct ARCTOWN_API FArcTownBuildingVisual_MassEntity : public FArcTownBuildingVisual
{
	GENERATED_BODY()

	virtual bool IsValid(FText& OutMessage) const override;
	virtual FSoftObjectPath GetAssetPath() const override;

	/** FSoftObjectPath, not TSoftObjectPtr<UArcMassEntityConfigAsset>: ArcTown loads at PostConfigInit and ArcMass at Default, so this module cannot take the dependency the typed pointer would need. */
	UPROPERTY(EditAnywhere, Category = "Visual",
		meta = (AllowedClasses = "/Script/ArcMass.ArcMassEntityConfigAsset"))
	FSoftObjectPath EntityConfig;
};
