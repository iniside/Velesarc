// Copyright Lukasz Baran. All Rights Reserved.

#include "Conversion/ArcBlueprintToEntityConfig.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "Blueprint/BlueprintSupport.h"
#include "ContentBrowserMenuContexts.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/PackageName.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "ArcBlueprintToEntityConfigMenu"

DEFINE_LOG_CATEGORY_STATIC(LogArcBlueprintToEntityConfigMenu, Log, All);

namespace ArcBlueprintToEntityConfigMenu_Private
{
	static bool IsConvertActionVisible(const FToolMenuContext& MenuContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
		if (Context == nullptr)
		{
			return false;
		}

		if (Context->SelectedAssets.IsEmpty())
		{
			return false;
		}

		for (const FAssetData& Asset : Context->SelectedAssets)
		{
			if (!Asset.IsInstanceOf(UBlueprint::StaticClass()))
			{
				return false;
			}

			FString NativeParentClassPath;
			if (!Asset.GetTagValue(FBlueprintTags::NativeParentClassPath, NativeParentClassPath) || NativeParentClassPath.IsEmpty())
			{
				return false;
			}

			// Names the first native ancestor even for Blueprint-derived-from-Blueprint chains (BlueprintSupport.h:33-34); no package load, class is already resident since it is native.
			const UClass* NativeParentClass = UClass::TryFindTypeSlow<UClass>(FPackageName::ExportTextPathToObjectPath(NativeParentClassPath));
			if (NativeParentClass == nullptr || !NativeParentClass->IsChildOf(AActor::StaticClass()))
			{
				return false;
			}
		}

		return true;
	}

	static void HandleOnConvertBlueprintToEntityConfig(const FToolMenuContext& MenuContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
		if (Context == nullptr)
		{
			return;
		}

		TArray<UBlueprint*> Blueprints = Context->LoadSelectedObjects<UBlueprint>();

		int32 NumConverted = 0;
		int32 NumFailed = 0;

		for (const UBlueprint* Blueprint : Blueprints)
		{
			if (Blueprint == nullptr)
			{
				continue;
			}

			TArray<FText> Problems;
			UArcMassEntityConfigAsset* ConfigAsset = ConvertBlueprintToEntityConfig(*Blueprint, Problems);
			if (ConfigAsset != nullptr)
			{
				++NumConverted;
			}
			else
			{
				++NumFailed;
			}

			for (const FText& Problem : Problems)
			{
				UE_LOGF(LogArcBlueprintToEntityConfigMenu, Warning,"%ls: %ls", *Blueprint->GetName(), *Problem.ToString());
			}
		}

		FNotificationInfo Info(FText::Format(
			LOCTEXT("ConvertResult", "Converted {0} Blueprint(s) to Mass Entity Config ({1} failed)"),
			FText::AsNumber(NumConverted),
			FText::AsNumber(NumFailed)));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []
	{
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]
		{
			FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UBlueprint::StaticClass());

			FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");

			Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateStatic([](FToolMenuSection& InSection)
			{
				FToolUIAction UIAction;
				UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&HandleOnConvertBlueprintToEntityConfig);
				UIAction.IsActionVisibleDelegate = FToolMenuIsActionButtonVisible::CreateStatic(&IsConvertActionVisible);
				InSection.AddMenuEntry(
					"ArcMassConfig_ConvertBlueprintToEntityConfig",
					LOCTEXT("ConvertBlueprintToEntityConfigMenu", "Convert to Mass Entity Config"),
					LOCTEXT("ConvertBlueprintToEntityConfigMenuTip", "Writes a Mass entity config carrying the Blueprint's static meshes as a flat list"),
					FSlateIcon(),
					UIAction);
			}));
		}));
	});
} // namespace ArcBlueprintToEntityConfigMenu_Private

#undef LOCTEXT_NAMESPACE
