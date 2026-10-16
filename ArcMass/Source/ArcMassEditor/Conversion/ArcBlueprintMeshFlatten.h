// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"

class UClass;
class UMaterialInterface;
class UStaticMesh;

struct FArcFlattenedMeshEntry
{
	UStaticMesh* Mesh = nullptr;
	FTransform RelativeTransform = FTransform::Identity;
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;
};

/** Returns false when the class yields no mesh entry. OutProblems is filled regardless. */
bool FlattenBlueprintStaticMeshes(
	const UClass& ActorClass,
	TArray<FArcFlattenedMeshEntry>& OutEntries,
	TArray<FText>& OutProblems);
