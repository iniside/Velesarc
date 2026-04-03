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
#include "PlacedEntities/ArcPlacedEntityPlacementFactory.h"
#include "PlacedEntities/ArcPlacedCompositeMeshPlacementFactory.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "Editor.h"
#include "Subsystems/PlacementSubsystem.h"
#include "PlacedEntities/ArcPlacedEntityEditorModeCommands.h"

#define LOCTEXT_NAMESPACE "ArcMassEditor"

EAssetTypeCategories::Type FArcMassEditorModule::ArcMassAssetCategory;

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

    PropertyModule.RegisterCustomClassLayout(
        AArcPlacedEntityPartitionActor::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FArcPlacedEntityActorDetails::MakeInstance));

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
}

void FArcMassEditorModule::ShutdownModule()
{
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
        PropertyModule.UnregisterCustomClassLayout(AArcPlacedEntityPartitionActor::StaticClass()->GetFName());
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcMassEditorModule, ArcMassEditor)
