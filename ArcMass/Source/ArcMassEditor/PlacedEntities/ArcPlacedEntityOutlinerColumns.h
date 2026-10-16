// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"

#include "ArcPlacedEntityOutlinerColumns.generated.h"

/**
 * Tag applied to every synthetic row this feature adds to the TEDS outliner
 * (both config-group rows and per-instance rows).  Use as an .Any<> gate in
 * TEDS queries to restrict to rows this feature owns.
 */
USTRUCT()
struct FArcPlacedEntityOutlinerTag final : public FEditorDataStorageTag
{
	GENERATED_BODY()
};

/**
 * Stable identity column written onto every synthetic outliner row.
 *
 * - TransformIndex == INDEX_NONE  →  this is a config-group row
 * - TransformIndex != INDEX_NONE  →  this is a per-instance row
 *
 * EntryIndex identifies the FArcPlacedEntityEntry inside the partition actor's
 * entry array; TransformIndex (when valid) identifies the specific transform
 * slot within that entry.
 */
USTRUCT()
struct FArcPlacedEntityOutlinerInstanceColumn final : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	TWeakObjectPtr<AArcPlacedEntityPartitionActor> PartitionActor;
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
};
