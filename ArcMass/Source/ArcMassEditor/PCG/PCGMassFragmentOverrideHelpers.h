// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityTypes.h"

class UPCGMetadata;
class UPCGBasePointData;
struct FPCGContext;

namespace PCGMassFragmentOverrides
{
	/**
	 * Scan metadata attribute names for the FragmentName.PropertyName pattern.
	 * Returns a map: FragmentName -> array of PropertyName.
	 */
	TMap<FName, TArray<FName>> DiscoverFragmentAttributes(const UPCGMetadata* Metadata);

	/**
	 * Resolve a fragment name (e.g. "FMyFragment") to its UScriptStruct*.
	 * Searches registered structs that inherit from FMassFragment.
	 * Returns nullptr if not found (logs warning).
	 */
	UScriptStruct* ResolveFragmentStruct(FName FragmentName, FPCGContext* Context);

	/**
	 * For a given point, assemble all fragment overrides from discovered attributes.
	 * Constructs FInstancedStruct per fragment, writes individual properties via reflection.
	 * Returns the per-instance overrides (empty if no relevant attributes have non-default values).
	 */
	FArcPlacedEntityFragmentOverrides AssembleOverridesForPoint(
		const UPCGBasePointData* PointData,
		int32 PointIndex,
		const TMap<FName, TArray<FName>>& FragmentAttributeMap,
		const TMap<FName, UScriptStruct*>& ResolvedStructs,
		FPCGContext* Context);

	/**
	 * Write a single PCG attribute value into an FProperty on a struct instance.
	 * Handles float, int, bool, FVector, FRotator, FString, FName, FSoftObjectPath.
	 * Returns false if type mismatch or property not writable.
	 */
	bool WriteAttributeToProperty(
		const UPCGBasePointData* PointData,
		int32 PointIndex,
		FName AttributeName,
		FProperty* TargetProperty,
		void* StructMemory,
		FPCGContext* Context);
}
