// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEditorModule.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "PropertyEditorModule.h"
#include "Customization/ArcEntityVisualizationTraitCustomization.h"
#include "Customization/ArcMeshEntityVisualizationTraitCustomization.h"
#include "Customization/ArcMassPhysicsBodyConfigCustomization.h"
#include "ArcMass/Visualization/ArcMassEntityVisualizationTrait.h"
#include "ArcMass/Visualization/ArcMeshEntityVisualizationTrait.h"
#include "ArcMass/Physics/ArcMassPhysicsBodyConfig.h"
#include "Customization/ArcCollisionGeometryCustomization.h"
#include "ArcMass/Physics/ArcCollisionGeometry.h"
#include "Customization/ArcCompositeMeshVisualizationTraitCustomization.h"
#include "ArcMass/CompositeVisualization/ArcCompositeMeshVisualizationTrait.h"
#include "Customization/ArcMassPhysicsBodyTraitCustomization.h"
#include "ArcMass/Physics/ArcMassPhysicsBodyTrait.h"
#include "PlacedEntities/ArcPlacedEntityActorDetails.h"
#include "PlacedEntities/ArcPlacedEntityProxyEditingObject.h"
#include "PlacedEntities/ArcPlacedEntityProxyDetails.h"
#include "PlacedEntities/ArcPlacedEntityPlacementFactory.h"
#include "PlacedEntities/ArcPlacedCompositeMeshPlacementFactory.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ArcMass/PlacedEntities/ArcEntityRef.h"
#include "PlacedEntities/ArcEntityRefCustomization.h"
#include "EditorModeRegistry.h"
#include "Editor.h"
#include "Subsystems/PlacementSubsystem.h"
#include "PlacedEntities/ArcPlacedEntityEditorModeCommands.h"
#include "EditorVisualization/SArcEntityVisualizationTab.h"
#include "EditorVisualization/ArcEditorEntityDrawSubsystem.h"
#include "Elements/SkMInstance/SkMInstanceElementDetailsInterface.h"
#include "Elements/SkMInstance/SkMInstanceElementEditorWorldInterface.h"
#include "Elements/SkMInstance/SkMInstanceElementEditorSelectionInterface.h"
#include "ArcMass/Elements/SkMInstance/SkMInstanceElementData.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "ArcMassEditor"

EAssetTypeCategories::Type FArcMassEditorModule::ArcMassAssetCategory;
const FName FArcMassEditorModule::EntityVisualizationTabId = FName("ArcEntityVisualization");

void FArcMassEditorModule::StartupModule()
{
    if (IsRunningGame())
    {
        return;
    }

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    ArcMassAssetCategory = AssetTools.RegisterAdvancedAssetCategory(
        FName(TEXT("Arc Mass")),
        FText::FromString("Arc Mass"));

    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

    PropertyModule.RegisterCustomClassLayout(
        UArcEntityVisualizationTrait::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FArcEntityVisualizationTraitCustomization::MakeInstance));

    PropertyModule.RegisterCustomClassLayout(
        UArcMeshEntityVisualizationTrait::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FArcMeshEntityVisualizationTraitCustomization::MakeInstance));

    PropertyModule.RegisterCustomClassLayout(
        UArcCompositeMeshVisualizationTrait::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FArcCompositeMeshVisualizationTraitCustomization::MakeInstance));

    PropertyModule.RegisterCustomClassLayout(
        UArcMassPhysicsBodyTrait::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FArcMassPhysicsBodyTraitCustomization::MakeInstance));

    PropertyModule.RegisterCustomPropertyTypeLayout(
        FArcMassPhysicsBodyConfigFragment::StaticStruct()->GetFName(),
        FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcMassPhysicsBodyConfigCustomization::MakeInstance));

    PropertyModule.RegisterCustomPropertyTypeLayout(
        FArcCollisionGeometry::StaticStruct()->GetFName(),
        FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcCollisionGeometryCustomization::MakeInstance));

    PropertyModule.RegisterCustomPropertyTypeLayout(
        FArcEntityRef::StaticStruct()->GetFName(),
        FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcEntityRefCustomization::MakeInstance));

    FEditorModeRegistry::Get().RegisterMode<FArcEntityPickerEdMode>(FArcEntityPickerEdMode::ModeId, LOCTEXT("ArcEntityPickerMode", "Entity Picker"), FSlateIcon(), false);

    PropertyModule.RegisterCustomClassLayout(
        AArcPlacedEntityPartitionActor::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FArcPlacedEntityActorDetails::MakeInstance));

    PropertyModule.RegisterCustomClassLayout(
        UArcPlacedEntityProxyEditingObject::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FArcPlacedEntityProxyDetails::MakeInstance));

    PlacementFactory = NewObject<UArcPlacedEntityPlacementFactory>();
    PlacementFactory->AddToRoot();

    CompositePlacementFactory = NewObject<UArcPlacedCompositeMeshPlacementFactory>();
    CompositePlacementFactory->AddToRoot();

    FCoreDelegates::GetOnPostEngineInit().AddLambda([this]()
    {
        if (GEditor)
        {
            if (UPlacementSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>())
            {
                PlacementSubsystem->RegisterAssetFactory(TScriptInterface<IAssetFactoryInterface>(PlacementFactory));
                PlacementSubsystem->RegisterAssetFactory(TScriptInterface<IAssetFactoryInterface>(CompositePlacementFactory));
            }
        }
    });

	FArcPlacedEntityEditorModeCommands::Register();

	// Register SkMInstance editor interface overrides
	if (UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance())
	{
		Registry->RegisterElementInterface<ITypedElementDetailsInterface>(NAME_SkMInstance, NewObject<USkMInstanceElementDetailsInterface>());
		Registry->RegisterElementInterface<ITypedElementWorldInterface>(NAME_SkMInstance, NewObject<USkMInstanceElementEditorWorldInterface>(), /*bAllowOverride*/true);
		Registry->RegisterElementInterface<ITypedElementSelectionInterface>(NAME_SkMInstance, NewObject<USkMInstanceElementEditorSelectionInterface>(), /*bAllowOverride*/true);
	}

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		EntityVisualizationTabId,
		FOnSpawnTab::CreateLambda([](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
		{
			UArcEditorEntityDrawSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UArcEditorEntityDrawSubsystem>() : nullptr;

			if (Subsystem)
			{
				UWorld* World = GEditor->GetEditorWorldContext().World();
				Subsystem->Activate(World);
			}

			TSharedRef<SDockTab> Tab = SNew(SDockTab)
				.TabRole(NomadTab)
				[
					SNew(SArcEntityVisualizationTab, Subsystem)
				];

			Tab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([](TSharedRef<SDockTab>)
			{
				UArcEditorEntityDrawSubsystem* Sub = GEditor ? GEditor->GetEditorSubsystem<UArcEditorEntityDrawSubsystem>() : nullptr;
				if (Sub)
				{
					Sub->Deactivate();
				}
			}));

			return Tab;
		}))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory())
		.SetDisplayName(LOCTEXT("EntityVisualizationTabTitle", "Entity Visualization"))
		.SetTooltipText(LOCTEXT("EntityVisualizationTabTooltip", "Toggle entity debug visualization overlays"));
}

void FArcMassEditorModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(EntityVisualizationTabId);

	FArcPlacedEntityEditorModeCommands::Unregister();

    if (PlacementFactory)
    {
        if (GEditor)
        {
            if (UPlacementSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>())
            {
                PlacementSubsystem->UnregisterAssetFactory(TScriptInterface<IAssetFactoryInterface>(PlacementFactory));
            }
        }
        //PlacementFactory->RemoveFromRoot();
        //PlacementFactory = nullptr;
    }

    if (CompositePlacementFactory)
    {
        if (GEditor)
        {
            if (UPlacementSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>())
            {
                PlacementSubsystem->UnregisterAssetFactory(TScriptInterface<IAssetFactoryInterface>(CompositePlacementFactory));
            }
        }
    }

    if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
    {
        FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
        PropertyModule.UnregisterCustomClassLayout(UArcEntityVisualizationTrait::StaticClass()->GetFName());
        PropertyModule.UnregisterCustomClassLayout(UArcMeshEntityVisualizationTrait::StaticClass()->GetFName());
        PropertyModule.UnregisterCustomClassLayout(UArcCompositeMeshVisualizationTrait::StaticClass()->GetFName());
        PropertyModule.UnregisterCustomClassLayout(UArcMassPhysicsBodyTrait::StaticClass()->GetFName());
        PropertyModule.UnregisterCustomPropertyTypeLayout(FArcMassPhysicsBodyConfigFragment::StaticStruct()->GetFName());
        PropertyModule.UnregisterCustomPropertyTypeLayout(FArcCollisionGeometry::StaticStruct()->GetFName());
        PropertyModule.UnregisterCustomPropertyTypeLayout(FArcEntityRef::StaticStruct()->GetFName());
        PropertyModule.UnregisterCustomClassLayout(AArcPlacedEntityPartitionActor::StaticClass()->GetFName());
        PropertyModule.UnregisterCustomClassLayout(UArcPlacedEntityProxyEditingObject::StaticClass()->GetFName());
    }
    FEditorModeRegistry::Get().UnregisterMode(FArcEntityPickerEdMode::ModeId);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcMassEditorModule, ArcMassEditor)
