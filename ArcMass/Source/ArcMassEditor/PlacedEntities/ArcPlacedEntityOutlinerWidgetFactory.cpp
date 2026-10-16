// Copyright Lukasz Baran. All Rights Reserved.

#include "PlacedEntities/ArcPlacedEntityOutlinerWidgetFactory.h"

#include "ActorDesc/TedsActorDescColumns.h"
#include "ActorFolders/ActorFolderColumns.h"
#include "DataStorage/Features.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementUIColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "PlacedEntities/ArcPlacedEntityOutlinerColumns.h"
#include "TedsOutlinerImpl.h"

// ---------------------------------------------------------------------------
// FArcPlacedEntitySceneOutlinerWidgetConstructor
// ---------------------------------------------------------------------------

FArcPlacedEntitySceneOutlinerWidgetConstructor::FArcPlacedEntitySceneOutlinerWidgetConstructor()
{
	// Our direct base is FTedsSceneOutlinerWidgetConstructor (whose no-arg ctor runs implicitly
	// and seeds TypeInfo with the *base* StaticStruct). FSimpleWidgetConstructor is a non-virtual
	// grandparent, so it cannot be named in the initializer list. Instead overwrite the protected
	// TypeInfo member with our leaf StaticStruct() so GetTypeInfo() returns the concrete type.
	TypeInfo = StaticStruct();
}

UE::Editor::DataStorage::FQueryDescription FArcPlacedEntitySceneOutlinerWidgetConstructor::GetQueryDescription() const
{
	using namespace UE::Editor::DataStorage;
	using namespace UE::Editor::DataStorage::Queries;

	// Rebuild the full base query (Select().Where()...Compile()) and extend the
	// Any clause with FArcPlacedEntityOutlinerTag so our synthetic rows pass the filter.
	//
	// The base GetBaseOutlinerQuery() (SceneOutlinerWidget.cpp ~line 117) is:
	//   Select()
	//   .Where()
	//     .All<FTypedElementLabelColumn, FTypedElementWorldColumn>()
	//     .Any<FTypedElementUObjectColumn, FFolderCompatibilityColumn, FActorDescTag>()
	//   .Compile()
	//
	// The base GetQueryDescription() then appends a SimpleNone for FHideRowFromUITag by
	// mutating ConditionTypes / ConditionOperators directly (it cannot use .None<> in the
	// builder because the same base query is reused for observers).  We reproduce that
	// exact mutation here.
	FQueryDescription QueryDescription =
		Select()
		.Where()
			.All<FTypedElementLabelColumn, FTypedElementWorldColumn>()
			.Any<FTypedElementUObjectColumn, FFolderCompatibilityColumn, FActorDescTag, FArcPlacedEntityOutlinerTag>()
		.Compile();

	// Append the .None<FHideRowFromUITag>() condition the same way the engine base does.
	QueryDescription.ConditionTypes.Add(FQueryDescription::EOperatorType::SimpleNone);
	QueryDescription.ConditionOperators.AddZeroed_GetRef().Type = FHideRowFromUITag::StaticStruct();

	return QueryDescription;
}

void FArcPlacedEntitySceneOutlinerWidgetConstructor::GetExpansionStateBridge(UE::Editor::Outliner::FTedsOutlinerParams& OutlinerParams)
{
	using namespace UE::Editor::DataStorage;

	// Let the base install its bridge first (handles folders, standard actors, actor descs).
	Super::GetExpansionStateBridge(OutlinerParams);

	// Capture the base GetExpansionState so we can delegate to it for all rows except ours.
	TFunction<bool(RowHandle)> BaseGetExpansion = MoveTemp(OutlinerParams.ExpansionStateBridge.GetExpansionState);

	// Replace GetExpansionState: return false (collapsed) for our synthetic rows, else
	// delegate to the base.  ICoreProvider is obtained via the global feature accessor
	// (same pattern used in SceneOutlinerWidget.cpp GetExpansionStateBridge ~line 863).
	OutlinerParams.ExpansionStateBridge.GetExpansionState =
		[BaseGetExpansion = MoveTemp(BaseGetExpansion)](RowHandle Row) -> bool
		{
			if (ICoreProvider* Storage = GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName))
			{
				if (Storage->HasColumns<FArcPlacedEntityOutlinerTag>(Row))
				{
					return false;
				}
			}
			return BaseGetExpansion ? BaseGetExpansion(Row) : true;
		};

	// SetExpansionState is intentionally left as set by the base (folder persistence).
}

// ---------------------------------------------------------------------------
// UArcMassOutlinerWidgetFactory
// ---------------------------------------------------------------------------

uint8 UArcMassOutlinerWidgetFactory::GetOrder() const
{
	// 200 > engine default 127; last-wins on UniqueByName replaces the engine constructor.
	return 200;
}

void UArcMassOutlinerWidgetFactory::RegisterWidgetConstructors(
	UE::Editor::DataStorage::ICoreProvider& /*DataStorage*/,
	UE::Editor::DataStorage::IUiProvider& DataStorageUi) const
{
	using namespace UE::Editor::DataStorage;

	// The "TedsOutliner" purpose was registered by UTedsOutlinerWidgetFactory::RegisterWidgetPurposes
	// using the constants (SceneOutlinerWidget.cpp ~line 133):
	//   static FName LevelEditorModuleName  = "LevelEditor";
	//   static FName SceneOutlinerModuleName = "SceneOutliner";
	//   IUiProvider::FPurposeInfo(LevelEditorModuleName, SceneOutlinerModuleName, "TedsOutliner").GeneratePurposeID()
	//
	// We replicate the same literal FNames so GeneratePurposeID() hashes identically.
	// Do NOT call RegisterWidgetPurpose() — it is first-wins and the engine has already done it.
	const IUiProvider::FPurposeID TedsOutlinerPurposeID =
		IUiProvider::FPurposeInfo(FName("LevelEditor"), FName("SceneOutliner"), FName("TedsOutliner"))
		.GeneratePurposeID();

	const RowHandle PurposeRow = DataStorageUi.FindPurpose(TedsOutlinerPurposeID);
	DataStorageUi.RegisterWidgetFactory<FArcPlacedEntitySceneOutlinerWidgetConstructor>(PurposeRow);
}
