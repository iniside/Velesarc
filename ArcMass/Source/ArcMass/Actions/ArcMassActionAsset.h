// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcMassActionAsset.generated.h"

UCLASS(BlueprintType)
class ARCMASS_API UArcMassStatelessActionAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Actions", meta = (BaseStruct = "/Script/ArcMass.ArcMassStatelessAction"))
	TArray<FInstancedStruct> Actions;
};

UCLASS(BlueprintType)
class ARCMASS_API UArcMassStatefulActionAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Actions", meta = (BaseStruct = "/Script/ArcMass.ArcMassStatefulAction"))
	TArray<FInstancedStruct> Actions;
};
