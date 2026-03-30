// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCPToolset.h"
#include "ArcMCPToolsetUtils.h"

// Engine - Widget Blueprint
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditorUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"

// Engine - UMG Components
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanel.h"

// Engine - Named Slot Interface
#include "Components/NamedSlotInterface.h"

// Engine - Blueprint utilities
#include "ScopedTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor/UMGEditor/Private/Utility/WidgetSlotPair.h"

// Engine - JSON
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace ArcMCPWidgetHelpers
{
	UWidgetBlueprint* LoadWidgetBP(const FString& WidgetPath)
	{
		return LoadObject<UWidgetBlueprint>(nullptr, *WidgetPath);
	}

	void EnsureAllWidgetGuids(UWidgetBlueprint* WidgetBlueprint)
	{
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
		{
			return;
		}
		WidgetBlueprint->WidgetTree->ForEachWidget([WidgetBlueprint](UWidget* Widget)
		{
			if (Widget && !WidgetBlueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
			{
				WidgetBlueprint->OnVariableAdded(Widget->GetFName());
			}
		});
	}

	TArray<TSharedPtr<FJsonValue>> BuildWidgetHierarchyJson(UWidgetTree* WidgetTree)
	{
		TArray<TSharedPtr<FJsonValue>> Widgets;
		if (!WidgetTree)
		{
			return Widgets;
		}
		WidgetTree->ForEachWidget([&Widgets](UWidget* Widget)
		{
			TSharedPtr<FJsonObject> WidgetJson = MakeShared<FJsonObject>();
			WidgetJson->SetStringField(TEXT("name"), Widget->GetName());
			WidgetJson->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
			WidgetJson->SetBoolField(TEXT("is_variable"), Widget->bIsVariable);
			if (Widget->GetParent())
			{
				WidgetJson->SetStringField(TEXT("parent"), Widget->GetParent()->GetName());
			}
			Widgets.Add(MakeShared<FJsonValueObject>(WidgetJson));
		});
		return Widgets;
	}
} // namespace ArcMCPWidgetHelpers

//------------------------------------------------------------------------------
// ExportWidgetToText
//------------------------------------------------------------------------------
FString UArcMCPToolset::ExportWidgetToText(const FString& WidgetPath, const FString& WidgetName)
{
	if (WidgetPath.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: WidgetPath"));
	}

	UWidgetBlueprint* WidgetBlueprint = ArcMCPWidgetHelpers::LoadWidgetBP(WidgetPath);
	if (!WidgetBlueprint)
	{
		return ArcMCPToolsetPrivate::MakeError(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Widget Blueprint has no root widget"));
	}

	TArray<UWidget*> WidgetsToExport;
	if (!WidgetName.IsEmpty())
	{
		UWidget* Target = WidgetTree->FindWidget(FName(*WidgetName));
		if (!Target)
		{
			return ArcMCPToolsetPrivate::MakeError(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
		}
		WidgetsToExport.Add(Target);
	}
	else
	{
		// Export root widget; ExportWidgetsToText recurses into children automatically
		WidgetsToExport.Add(WidgetTree->RootWidget);
	}

	FString ExportedText;
	FWidgetBlueprintEditorUtils::ExportWidgetsToText(WidgetsToExport, ExportedText);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("widget_path"), WidgetPath);
	Result->SetStringField(TEXT("widget_text"), ExportedText);
	return ArcMCPToolsetPrivate::JsonObjToString(Result);
}

//------------------------------------------------------------------------------
// ImportWidgetFromText
//------------------------------------------------------------------------------
FString UArcMCPToolset::ImportWidgetFromText(const FString& WidgetPath, const FString& WidgetText, const FString& ParentName, bool ClearParent)
{
	if (WidgetPath.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: WidgetPath"));
	}

	if (WidgetText.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: WidgetText"));
	}

	UWidgetBlueprint* WidgetBlueprint = ArcMCPWidgetHelpers::LoadWidgetBP(WidgetPath);
	bool bCreatedNew = false;

	if (!WidgetBlueprint)
	{
		FString PackagePath;
		FString AssetName;
		WidgetPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

		if (AssetName.IsEmpty())
		{
			return ArcMCPToolsetPrivate::MakeError(FString::Printf(TEXT("Invalid WidgetPath: %s"), *WidgetPath));
		}

		UPackage* Package = CreatePackage(*WidgetPath);
		if (!Package)
		{
			return ArcMCPToolsetPrivate::MakeError(TEXT("Failed to create package"));
		}

		WidgetBlueprint = CastChecked<UWidgetBlueprint>(
			FKismetEditorUtilities::CreateBlueprint(
				UUserWidget::StaticClass(),
				Package,
				*AssetName,
				BPTYPE_Normal,
				UWidgetBlueprint::StaticClass(),
				UBlueprintGeneratedClass::StaticClass()
			)
		);

		if (!WidgetBlueprint)
		{
			return ArcMCPToolsetPrivate::MakeError(TEXT("Failed to create Widget Blueprint"));
		}

		UWidgetTree* NewWidgetTree = WidgetBlueprint->WidgetTree;
		if (NewWidgetTree)
		{
			UCanvasPanel* RootCanvas = NewWidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
			NewWidgetTree->RootWidget = RootCanvas;
		}

		FAssetRegistryModule::AssetCreated(WidgetBlueprint);
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
		bCreatedNew = true;
	}

	FScopedTransaction Transaction(NSLOCTEXT("ArcMCP", "ImportWidgetFromText", "Import Widget From Text"));
	WidgetBlueprint->Modify();

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Widget Blueprint has no widget tree"));
	}

	UPanelWidget* ParentWidget = nullptr;
	if (!ParentName.IsEmpty())
	{
		UWidget* Found = WidgetTree->FindWidget(FName(*ParentName));
		if (!Found)
		{
			return ArcMCPToolsetPrivate::MakeError(FString::Printf(TEXT("Parent widget not found: %s"), *ParentName));
		}
		ParentWidget = Cast<UPanelWidget>(Found);
		if (!ParentWidget)
		{
			return ArcMCPToolsetPrivate::MakeError(FString::Printf(
				TEXT("Parent widget '%s' is not a panel widget (class: %s). Cannot add children to it."),
				*ParentName, *Found->GetClass()->GetName()));
		}
	}

	// Teardown phase: remove existing widgets if replacing
	if (ParentWidget && ClearParent)
	{
		TSet<UWidget*> ChildrenToRemove;
		for (int32 i = 0; i < ParentWidget->GetChildrenCount(); i++)
		{
			UWidget* Child = ParentWidget->GetChildAt(i);
			if (Child)
			{
				ChildrenToRemove.Add(Child);
			}
		}
		if (ChildrenToRemove.Num() > 0)
		{
			FWidgetBlueprintEditorUtils::DeleteWidgets(WidgetBlueprint, ChildrenToRemove,
				FWidgetBlueprintEditorUtils::EDeleteWidgetWarningType::DeleteSilently);
		}
	}
	else if (!ParentWidget && !bCreatedNew)
	{
		// Full tree replacement — tear down existing root
		if (WidgetTree->RootWidget)
		{
			TSet<UWidget*> AllWidgets;
			WidgetTree->ForEachWidget([&AllWidgets](UWidget* Widget)
			{
				AllWidgets.Add(Widget);
			});
			if (AllWidgets.Num() > 0)
			{
				FWidgetBlueprintEditorUtils::DeleteWidgets(WidgetBlueprint, AllWidgets,
					FWidgetBlueprintEditorUtils::EDeleteWidgetWarningType::DeleteSilently);
			}
			WidgetTree->RootWidget = nullptr;
		}
	}

	// Import phase: deserialize T3D text into widgets
	TSet<UWidget*> ImportedWidgetSet;
	TMap<FName, UWidgetSlotPair*> PastedExtraSlotData;
	FWidgetBlueprintEditorUtils::ImportWidgetsFromText(WidgetBlueprint, WidgetText, ImportedWidgetSet, PastedExtraSlotData);

	if (ImportedWidgetSet.Num() == 0)
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("No widgets were imported from the provided text. Check that the T3D format is valid."));
	}

	// Register GUIDs for every imported widget (matches engine's own paste path in PasteWidgetsInternal)
	for (UWidget* Widget : ImportedWidgetSet)
	{
		WidgetBlueprint->OnVariableAdded(Widget->GetFName());
	}

	// Find root widgets: those without parents in the imported set, and not held in a named slot
	TArray<UWidget*> ImportedRoots;
	for (UWidget* Widget : ImportedWidgetSet)
	{
		if (Widget->GetParent() && ImportedWidgetSet.Contains(Widget->GetParent()))
		{
			continue;
		}

		bool bIsNamedSlotContent = false;
		for (UWidget* Other : ImportedWidgetSet)
		{
			INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(Other);
			if (NamedSlotHost && NamedSlotHost->ContainsContent(Widget))
			{
				bIsNamedSlotContent = true;
				break;
			}
		}
		if (bIsNamedSlotContent)
		{
			continue;
		}

		ImportedRoots.Add(Widget);
	}

	// Post-import wiring
	if (ParentWidget)
	{
		for (UWidget* Root : ImportedRoots)
		{
			UPanelSlot* NewSlot = ParentWidget->AddChild(Root);
			if (!NewSlot)
			{
				return ArcMCPToolsetPrivate::MakeError(FString::Printf(
					TEXT("Failed to add imported widget '%s' to parent '%s'. Parent may not accept more children."),
					*Root->GetName(), *ParentName));
			}

			UWidgetSlotPair* SlotData = PastedExtraSlotData.FindRef(Root->GetFName());
			if (SlotData && NewSlot)
			{
				TMap<FName, FString> SlotProperties;
				SlotData->GetSlotProperties(SlotProperties);
				FWidgetBlueprintEditorUtils::ImportPropertiesFromText(NewSlot, SlotProperties);
			}
		}
	}
	else
	{
		// Full tree replacement — set first imported root as the tree root
		if (ImportedRoots.Num() > 0)
		{
			WidgetTree->RootWidget = ImportedRoots[0];

			if (ImportedRoots.Num() > 1)
			{
				UE_LOG(LogTemp, Warning, TEXT("ImportWidgetFromText: Multiple root widgets imported. Only '%s' set as root."),
					*ImportedRoots[0]->GetName());
			}
		}
	}

	// Ensure all widgets have GUIDs registered (safety net)
	ArcMCPWidgetHelpers::EnsureAllWidgetGuids(WidgetBlueprint);

	// Compile
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	WidgetBlueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("widget_path"), WidgetPath);
	Result->SetBoolField(TEXT("created_new"), bCreatedNew);
	Result->SetNumberField(TEXT("imported_count"), ImportedWidgetSet.Num());
	Result->SetArrayField(TEXT("widgets"), ArcMCPWidgetHelpers::BuildWidgetHierarchyJson(WidgetTree));
	if (WidgetTree->RootWidget)
	{
		Result->SetStringField(TEXT("root_widget"), WidgetTree->RootWidget->GetName());
	}
	return ArcMCPToolsetPrivate::JsonObjToString(Result);
}
