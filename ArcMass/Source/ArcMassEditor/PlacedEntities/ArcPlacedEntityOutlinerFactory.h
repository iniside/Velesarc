// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "DataStorage/Handles.h"
#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/ObjectMacros.h"

#include "ArcPlacedEntityOutlinerFactory.generated.h"

namespace UE::Editor::DataStorage
{
	class ICoreProvider;
} // namespace UE::Editor::DataStorage

/**
 * TEDS factory for the placed-entity outliner feature.
 *
 * Registers two tables used by the row-manager subsystem:
 *   - ConfigGroupTable  — one row per FArcPlacedEntityEntry (config-group node)
 *   - InstanceTable     — one row per transform slot (instance node, carries a TypedElementHandle for selection)
 *
 * Auto-discovered by TEDS; no manual registration required.
 * GetOrder() is left at the default (127); no widget constructor is provided.
 */
UCLASS()
class UArcPlacedEntityOutlinerFactory : public UEditorDataStorageFactory
{
	GENERATED_BODY()

public:

	~UArcPlacedEntityOutlinerFactory() override = default;

	/** Table handle for config-group rows (one row per FArcPlacedEntityEntry). */
	UE::Editor::DataStorage::TableHandle ConfigGroupTable = UE::Editor::DataStorage::InvalidTableHandle;

	/** Table handle for per-instance rows (one row per transform slot). */
	UE::Editor::DataStorage::TableHandle InstanceTable = UE::Editor::DataStorage::InvalidTableHandle;

	/** Registers both outliner tables with TEDS. */
	void RegisterTables(UE::Editor::DataStorage::ICoreProvider& DataStorage) override;

	/** Purges all rows owned by this feature on shutdown. */
	void PreShutdown(UE::Editor::DataStorage::ICoreProvider& DataStorage) override;
};
