// Copyright Lukasz Baran. All Rights Reserved.

#include "PlacedEntities/ArcPlacedEntityOutlinerFactory.h"

#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementHandleColumn.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "PlacedEntities/ArcPlacedEntityOutlinerColumns.h"

namespace ArcPlacedEntityOutlinerFactory_Private
{
	// ConfigGroupTable column list (no TypedElementHandle — group rows are not individually selectable)
	// InstanceTable column list (adds FTedsTypedElementColumn for editor selection support)
} // namespace ArcPlacedEntityOutlinerFactory_Private

// ----------------------------------------------------------------------------
// UArcPlacedEntityOutlinerFactory
// ----------------------------------------------------------------------------

void UArcPlacedEntityOutlinerFactory::RegisterTables(UE::Editor::DataStorage::ICoreProvider& DataStorage)
{
	ConfigGroupTable = DataStorage.RegisterTable<
		FArcPlacedEntityOutlinerTag,
		FTypedElementLabelColumn,
		FTypedElementWorldColumn,
		FTypedElementUObjectColumn,
		FArcPlacedEntityOutlinerInstanceColumn
	>(FName("ArcMass_PlacedEntityConfigGroupTable"));

	InstanceTable = DataStorage.RegisterTable<
		FArcPlacedEntityOutlinerTag,
		FTypedElementLabelColumn,
		FTypedElementWorldColumn,
		FTypedElementUObjectColumn,
		FArcPlacedEntityOutlinerInstanceColumn,
		FTedsTypedElementColumn
	>(FName("ArcMass_PlacedEntityInstanceTable"));
}

void UArcPlacedEntityOutlinerFactory::PreShutdown(UE::Editor::DataStorage::ICoreProvider& DataStorage)
{
	DataStorage.RemoveAllRowsWith<FArcPlacedEntityOutlinerTag>();
}
