// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Widgets/SceneOutlinerWidget.h"

#include "ArcPlacedEntityOutlinerWidgetFactory.generated.h"

/**
 * Widget constructor override for the level-editor TedsOutliner.
 *
 * Two responsibilities:
 *   (a) Expands the row query so rows carrying FArcPlacedEntityOutlinerTag pass
 *       the Any clause alongside the standard engine types.
 *   (b) Wraps the base expansion bridge so that rows tagged with
 *       FArcPlacedEntityOutlinerTag default to collapsed.
 */
USTRUCT()
struct FArcPlacedEntitySceneOutlinerWidgetConstructor : public FTedsSceneOutlinerWidgetConstructor
{
	GENERATED_BODY()

public:
	FArcPlacedEntitySceneOutlinerWidgetConstructor();
	~FArcPlacedEntitySceneOutlinerWidgetConstructor() override = default;

	/** Returns the base query extended with FArcPlacedEntityOutlinerTag in the Any clause. */
	ARCMASSEDITOR_API virtual UE::Editor::DataStorage::FQueryDescription GetQueryDescription() const override;

	/** Wraps the base expansion bridge to collapse rows tagged FArcPlacedEntityOutlinerTag. */
	ARCMASSEDITOR_API virtual void GetExpansionStateBridge(UE::Editor::Outliner::FTedsOutlinerParams& OutlinerParams) override;
};

/**
 * TEDS widget factory that registers FArcPlacedEntitySceneOutlinerWidgetConstructor for
 * the UniqueByName "TedsOutliner" purpose.
 *
 * GetOrder() returns 200 so this factory runs after Epic's order-127
 * UTedsOutlinerWidgetFactory, and last-wins on UniqueByName purposes means
 * our constructor replaces the engine default.
 */
UCLASS()
class UArcMassOutlinerWidgetFactory : public UEditorDataStorageFactory
{
	GENERATED_BODY()

public:
	~UArcMassOutlinerWidgetFactory() override = default;

	/** Run after the engine factory (order 127) so last-wins on UniqueByName takes effect. */
	ARCMASSEDITOR_API virtual uint8 GetOrder() const override;

	/** Registers FArcPlacedEntitySceneOutlinerWidgetConstructor for the TedsOutliner purpose. */
	ARCMASSEDITOR_API virtual void RegisterWidgetConstructors(
		UE::Editor::DataStorage::ICoreProvider& DataStorage,
		UE::Editor::DataStorage::IUiProvider& DataStorageUi) const override;
};
