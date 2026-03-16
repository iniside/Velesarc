// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCP/Commands/ArcMCPWidgetClipboardCommands.h"

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

REGISTER_ECA_COMMAND(FArcMCPCommand_ExportWidgetToText)
REGISTER_ECA_COMMAND(FArcMCPCommand_ImportWidgetFromText)

//------------------------------------------------------------------------------
// Helper: Load widget blueprint
//------------------------------------------------------------------------------
static UWidgetBlueprint* LoadWidgetBP(const FString& WidgetPath)
{
	return LoadObject<UWidgetBlueprint>(nullptr, *WidgetPath);
}

//------------------------------------------------------------------------------
// Helper: Ensure all widgets have GUIDs (same pattern as ECAWidgetTreeCommands)
//------------------------------------------------------------------------------
static void EnsureAllWidgetGuids(UWidgetBlueprint* WidgetBlueprint)
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

//------------------------------------------------------------------------------
// Helper: Build a JSON array describing the widget hierarchy
//------------------------------------------------------------------------------
static TArray<TSharedPtr<FJsonValue>> BuildWidgetHierarchyJson(UWidgetTree* WidgetTree)
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

//------------------------------------------------------------------------------
// export_widget_to_text
//------------------------------------------------------------------------------
FECACommandResult FArcMCPCommand_ExportWidgetToText::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}

	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBP(WidgetPath);
	if (!WidgetBlueprint)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Widget Blueprint not found: %s"), *WidgetPath));
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		return FECACommandResult::Error(TEXT("Widget Blueprint has no root widget"));
	}

	// Determine which widget(s) to export
	TArray<UWidget*> WidgetsToExport;
	FString WidgetName;
	if (GetStringParam(Params, TEXT("widget_name"), WidgetName, false) && !WidgetName.IsEmpty())
	{
		UWidget* Target = WidgetTree->FindWidget(FName(*WidgetName));
		if (!Target)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
		}
		WidgetsToExport.Add(Target);
	}
	else
	{
		// Export root widget (ExportWidgetsToText recurses into children automatically)
		WidgetsToExport.Add(WidgetTree->RootWidget);
	}

	// Serialize to T3D text
	FString ExportedText;
	FWidgetBlueprintEditorUtils::ExportWidgetsToText(WidgetsToExport, ExportedText);

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("widget_path"), WidgetPath);
	Result->SetStringField(TEXT("widget_text"), ExportedText);
	return FECACommandResult::Success(Result);
}

//------------------------------------------------------------------------------
// import_widget_from_text
//------------------------------------------------------------------------------
FECACommandResult FArcMCPCommand_ImportWidgetFromText::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetPath;
	if (!GetStringParam(Params, TEXT("widget_path"), WidgetPath))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_path"));
	}

	FString WidgetText;
	if (!GetStringParam(Params, TEXT("widget_text"), WidgetText))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: widget_text"));
	}

	FString ParentName;
	GetStringParam(Params, TEXT("parent_name"), ParentName, false);

	bool bClearParent = false;
	GetBoolParam(Params, TEXT("clear_parent"), bClearParent, false);

	// Load or create the Widget Blueprint
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBP(WidgetPath);
	bool bCreatedNew = false;

	if (!WidgetBlueprint)
	{
		// Create new Widget Blueprint
		FString PackagePath, AssetName;
		WidgetPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

		if (AssetName.IsEmpty())
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Invalid widget_path: %s"), *WidgetPath));
		}

		UPackage* Package = CreatePackage(*WidgetPath);
		if (!Package)
		{
			return FECACommandResult::Error(TEXT("Failed to create package"));
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
			return FECACommandResult::Error(TEXT("Failed to create Widget Blueprint"));
		}

		// Add default CanvasPanel root
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
		return FECACommandResult::Error(TEXT("Widget Blueprint has no widget tree"));
	}

	// Determine target parent for injection
	UPanelWidget* ParentWidget = nullptr;
	if (!ParentName.IsEmpty())
	{
		UWidget* Found = WidgetTree->FindWidget(FName(*ParentName));
		if (!Found)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Parent widget not found: %s"), *ParentName));
		}
		ParentWidget = Cast<UPanelWidget>(Found);
		if (!ParentWidget)
		{
			return FECACommandResult::Error(FString::Printf(TEXT("Parent widget '%s' is not a panel widget (class: %s). Cannot add children to it."), *ParentName, *Found->GetClass()->GetName()));
		}
	}

	// Teardown phase: remove existing widgets if replacing
	if (ParentWidget && bClearParent)
	{
		// Remove all children of the specified parent
		TSet<UWidget*> ChildrenToRemove;
		for (int32 i = 0; i < ParentWidget->GetChildrenCount(); i++)
		{
			if (UWidget* Child = ParentWidget->GetChildAt(i))
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
		return FECACommandResult::Error(TEXT("No widgets were imported from the provided text. Check that the T3D format is valid."));
	}

	// Call OnVariableAdded for every imported widget unconditionally
	// (matches the engine's own paste path in PasteWidgetsInternal)
	for (UWidget* Widget : ImportedWidgetSet)
	{
		WidgetBlueprint->OnVariableAdded(Widget->GetFName());
	}

	// Find root widgets (those without parents in the imported set)
	// Also exclude widgets held in named slots
	TArray<UWidget*> ImportedRoots;
	for (UWidget* Widget : ImportedWidgetSet)
	{
		if (Widget->GetParent() && ImportedWidgetSet.Contains(Widget->GetParent()))
		{
			continue;
		}

		// Check if this widget is held in a named slot of another imported widget
		bool bIsNamedSlotContent = false;
		for (UWidget* Other : ImportedWidgetSet)
		{
			if (INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(Other))
			{
				if (NamedSlotHost->ContainsContent(Widget))
				{
					bIsNamedSlotContent = true;
					break;
				}
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
		// Inject under specified parent
		for (UWidget* Root : ImportedRoots)
		{
			UPanelSlot* NewSlot = ParentWidget->AddChild(Root);
			if (!NewSlot)
			{
				return FECACommandResult::Error(FString::Printf(
					TEXT("Failed to add imported widget '%s' to parent '%s'. Parent may not accept more children."),
					*Root->GetName(), *ParentName));
			}

			// Apply slot metadata if available
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
				UE_LOG(LogTemp, Warning, TEXT("import_widget_from_text: Multiple root widgets imported. Only '%s' set as root."),
					*ImportedRoots[0]->GetName());
			}
		}
	}

	// Ensure all widgets have GUIDs registered (safety net)
	EnsureAllWidgetGuids(WidgetBlueprint);

	// Compile
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	WidgetBlueprint->MarkPackageDirty();

	// Build result
	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetStringField(TEXT("widget_path"), WidgetPath);
	Result->SetBoolField(TEXT("created_new"), bCreatedNew);
	Result->SetNumberField(TEXT("imported_count"), ImportedWidgetSet.Num());
	Result->SetArrayField(TEXT("widgets"), BuildWidgetHierarchyJson(WidgetTree));
	if (WidgetTree->RootWidget)
	{
		Result->SetStringField(TEXT("root_widget"), WidgetTree->RootWidget->GetName());
	}
	return FECACommandResult::Success(Result);
}
