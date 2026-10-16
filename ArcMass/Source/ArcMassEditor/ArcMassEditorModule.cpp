// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEditorModule.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "PropertyEditorModule.h"
#include "Customization/ArcMassPhysicsBodyConfigCustomization.h"
#include "ArcMass/Physics/ArcMassPhysicsBodyConfig.h"
#include "Customization/ArcCollisionGeometryCustomization.h"
#include "ArcMass/Physics/ArcCollisionGeometry.h"
#include "Customization/ArcMassPhysicsBodyTraitCustomization.h"
#include "ArcMass/Physics/ArcMassPhysicsBodyTrait.h"
#include "PlacedEntities/ArcPlacedEntityActorDetails.h"
#include "PlacedEntities/ArcPlacedEntityProxyEditingObject.h"
#include "PlacedEntities/ArcPlacedEntityProxyDetails.h"
#include "PlacedEntities/ArcPlacedEntityPlacementFactory.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ArcMass/PlacedEntities/ArcEntityRef.h"
#include "PlacedEntities/ArcEntityRefCustomization.h"
#include "EditorModeRegistry.h"
#include "Editor.h"
#include "Subsystems/PlacementSubsystem.h"
#include "PlacedEntities/ArcPlacedEntityEditorModeCommands.h"
#include "EditorVisualization/ArcViewportDebugVizMenu.h"
#include "EditorVisualization/Contributors/ArcPlacedEntityCollisionDrawContributor.h"
#include "EditorVisualization/Contributors/ArcWorkAreaBoundsDrawContributor.h"
#include "EditorVisualization/ArcMassEditorVisualizationRegistry.h"
#include "Preview/ArcMobileVisEntityPreviewContributor.h"
#include "Preview/ArcMassPreviewContributor.h"
#include "Preview/ArcNiagaraVisPreviewContributor.h"
#include "Preview/ArcPhysicsBodyTraitPreviewContributor.h"
#include "Preview/ArcWorkAreaBoundsPreviewContributor.h"
#include "Preview/ArcUnifiedMeshVisualizationPreviewContributor.h"
#include "ToolMenus.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementData.h"
#include "Elements/MassEntity/ArcMassEntityElementEditorWorldInterface.h"
#include "Elements/MassEntity/ArcMassEntityElementEditorSelectionInterface.h"
#include "Elements/MassEntity/ArcMassEntityElementDetailsInterface.h"
#include "Elements/Framework/TypedElementRegistry.h"

#include "ThumbnailRendering/ThumbnailManager.h"
#include "ObjectTools.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "Misc/ObjectThumbnail.h"

#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "MassEntityConfigAsset.h"
#include "Thumbnail/ArcMassEntityConfigThumbnailRenderer.h"

#define LOCTEXT_NAMESPACE "ArcMassEditor"

namespace ArcMassEditorModule_PreviewContributorIds
{
	const TCHAR* const FastGeo = TEXT("FArcFastGeoVisualizationPreviewContributor");
	const TCHAR* const MobileVis = TEXT("FArcMobileVisEntityPreviewContributor");
	const TCHAR* const UnifiedMesh = TEXT("FArcUnifiedMeshVisualizationPreviewContributor");
	const TCHAR* const PhysicsBody = TEXT("FArcPhysicsBodyTraitPreviewContributor");
	const TCHAR* const Niagara = TEXT("FArcNiagaraVisPreviewContributor");
	const TCHAR* const WorkAreaBounds = TEXT("FArcWorkAreaBoundsPreviewContributor");

	const TCHAR* const All[] = { FastGeo, MobileVis, UnifiedMesh, PhysicsBody, Niagara, WorkAreaBounds };
}

namespace ArcMassEditorModule_DrawContributorIds
{
	const TCHAR* const PlacedEntityCollision = TEXT("FArcPlacedEntityCollisionDrawContributor");
	const TCHAR* const WorkAreaBounds = TEXT("FArcWorkAreaBoundsDrawContributor");

	const TCHAR* const All[] = { PlacedEntityCollision, WorkAreaBounds };
}

EAssetTypeCategories::Type FArcMassEditorModule::ArcMassAssetCategory;

void FArcMassEditorModule::StartupModule()
{
    if (IsRunningGame())
    {
        return;
    }

	{
		FArcMassEditorVisualizationRegistry& VisualizationRegistry = FArcMassEditorVisualizationRegistry::Get();
		const EArcPreviewContext BothContexts = EArcPreviewContext::Viewport | EArcPreviewContext::Thumbnail;

		VisualizationRegistry.RegisterPreviewContributor(ArcMassEditorModule_PreviewContributorIds::FastGeo, BothContexts,
			[]() -> TUniquePtr<IArcMassPreviewContributor> { return MakeUnique<FArcFastGeoVisualizationPreviewContributor>(); });
		VisualizationRegistry.RegisterPreviewContributor(ArcMassEditorModule_PreviewContributorIds::MobileVis, BothContexts,
			[]() -> TUniquePtr<IArcMassPreviewContributor> { return MakeUnique<FArcMobileVisEntityPreviewContributor>(); });
		VisualizationRegistry.RegisterPreviewContributor(ArcMassEditorModule_PreviewContributorIds::UnifiedMesh, BothContexts,
			[]() -> TUniquePtr<IArcMassPreviewContributor> { return MakeUnique<FArcUnifiedMeshVisualizationPreviewContributor>(); });

		// Viewport only: the physics body draws through a PDI the thumbnail path has none of, and Niagara Activate()s a system in a scene the thumbnail never ticks.
		VisualizationRegistry.RegisterPreviewContributor(ArcMassEditorModule_PreviewContributorIds::PhysicsBody, EArcPreviewContext::Viewport,
			[]() -> TUniquePtr<IArcMassPreviewContributor> { return MakeUnique<FArcPhysicsBodyTraitPreviewContributor>(); });
		VisualizationRegistry.RegisterPreviewContributor(ArcMassEditorModule_PreviewContributorIds::Niagara, EArcPreviewContext::Viewport,
			[]() -> TUniquePtr<IArcMassPreviewContributor> { return MakeUnique<FArcNiagaraVisPreviewContributor>(); });
		VisualizationRegistry.RegisterPreviewContributor(ArcMassEditorModule_PreviewContributorIds::WorkAreaBounds, EArcPreviewContext::Viewport,
			[]() -> TUniquePtr<IArcMassPreviewContributor> { return MakeUnique<FArcWorkAreaBoundsPreviewContributor>(); });

		VisualizationRegistry.RegisterDrawContributor(ArcMassEditorModule_DrawContributorIds::PlacedEntityCollision,
			MakeShared<FArcPlacedEntityCollisionDrawContributor>());
		VisualizationRegistry.RegisterDrawContributor(ArcMassEditorModule_DrawContributorIds::WorkAreaBounds,
			MakeShared<FArcWorkAreaBoundsDrawContributor>());
	}

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    ArcMassAssetCategory = AssetTools.RegisterAdvancedAssetCategory(
        FName(TEXT("Arc Mass")),
        FText::FromString("Arc Mass"));

    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

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

    PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddLambda([this]()
    {
        if (GEditor)
        {
            if (UPlacementSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>())
            {
                PlacementSubsystem->RegisterAssetFactory(TScriptInterface<IAssetFactoryInterface>(PlacementFactory));
            }
        }
    });

	FArcPlacedEntityEditorModeCommands::Register();

	// Register MassEntity editor interface overrides
	if (UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance())
	{
		Registry->RegisterElementInterface<ITypedElementWorldInterface>(NAME_ArcMassEntity, NewObject<UArcMassEntityElementEditorWorldInterface>(), /*bAllowOverride*/true);
		Registry->RegisterElementInterface<ITypedElementSelectionInterface>(NAME_ArcMassEntity, NewObject<UArcMassEntityElementEditorSelectionInterface>(), /*bAllowOverride*/true);
		Registry->RegisterElementInterface<ITypedElementDetailsInterface>(NAME_ArcMassEntity, NewObject<UArcMassEntityElementDetailsInterface>());
	}

	ViewportDebugVizStartupHandle = UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateStatic(&ArcMassEditor::ViewportDebugViz::Register));

	UThumbnailManager::Get().RegisterCustomRenderer(
		UArcMassEntityConfigAsset::StaticClass(),
		UArcMassEntityConfigThumbnailRenderer::StaticClass());

	UThumbnailManager::Get().RegisterCustomRenderer(
		UArcMassEntityConfigTemplate::StaticClass(),
		UArcMassEntityConfigThumbnailRenderer::StaticClass());

	if (GEditor != nullptr)
	{
		if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			OnAssetOpenedHandle = AssetEditorSubsystem->OnAssetOpenedInEditor().AddRaw(
				this, &FArcMassEditorModule::OnAssetOpenedInEditor);
		}
	}

	OnPackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddRaw(
		this, &FArcMassEditorModule::OnPackageSaved);
}

namespace ArcMassEditor_Thumbnail
{
	static void DirtyConfigThumbnail(UMassEntityConfigAsset* Config)
	{
		if (Config == nullptr)
		{
			return;
		}

		// Public path to "mark this asset's cached thumbnail as stale so renderer fires next display."
		// Mirrors what private UThumbnailManager::DirtyThumbnailForObject does for non-actor objects.
		if (FObjectThumbnail* Thumb = ThumbnailTools::GetThumbnailForObject(Config))
		{
			Thumb->MarkAsDirty();
		}
		else
		{
			// No thumbnail entry yet — seed an empty one so the package picks it up on next save
			// and the renderer is invoked to fill it.
			if (UPackage* Package = Config->GetOutermost())
			{
				ThumbnailTools::CacheEmptyThumbnail(Config->GetFullName(), Package);
			}
		}
	}
}

void FArcMassEditorModule::OnAssetOpenedInEditor(UObject* Asset, IAssetEditorInstance* /*EditorInstance*/)
{
	ArcMassEditor_Thumbnail::DirtyConfigThumbnail(Cast<UMassEntityConfigAsset>(Asset));
}

void FArcMassEditorModule::OnPackageSaved(const FString& /*PackageFileName*/, UPackage* Package, FObjectPostSaveContext /*SaveContext*/)
{
	if (Package == nullptr)
	{
		return;
	}

	ForEachObjectWithPackage(Package, [](UObject* Inner)
	{
		ArcMassEditor_Thumbnail::DirtyConfigThumbnail(Cast<UMassEntityConfigAsset>(Inner));
		return true;
	}, EGetObjectsFlags::None);
}

void FArcMassEditorModule::ShutdownModule()
{
	if (UObjectInitialized())
	{
		UThumbnailManager::Get().UnregisterCustomRenderer(UArcMassEntityConfigAsset::StaticClass());
		UThumbnailManager::Get().UnregisterCustomRenderer(UArcMassEntityConfigTemplate::StaticClass());
	}

	if (OnPackageSavedHandle.IsValid())
	{
		UPackage::PackageSavedWithContextEvent.Remove(OnPackageSavedHandle);
		OnPackageSavedHandle.Reset();
	}

	if (GEditor != nullptr && OnAssetOpenedHandle.IsValid())
	{
		if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AssetEditorSubsystem->OnAssetOpenedInEditor().Remove(OnAssetOpenedHandle);
		}
		OnAssetOpenedHandle.Reset();
	}

	FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);

	{
		FArcMassEditorVisualizationRegistry& VisualizationRegistry = FArcMassEditorVisualizationRegistry::Get();
		for (const TCHAR* const Id : ArcMassEditorModule_PreviewContributorIds::All)
		{
			VisualizationRegistry.UnregisterPreviewContributor(FName(Id));
		}
		for (const TCHAR* const Id : ArcMassEditorModule_DrawContributorIds::All)
		{
			VisualizationRegistry.UnregisterDrawContributor(FName(Id));
		}
	}

	UToolMenus::UnRegisterStartupCallback(ViewportDebugVizStartupHandle);
	ArcMassEditor::ViewportDebugViz::Unregister();

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

    if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
    {
        FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
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
