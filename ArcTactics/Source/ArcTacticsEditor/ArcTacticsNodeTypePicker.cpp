// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTacticsNodeTypePicker.h"

#include "ArcTacticsAuthoring.h"
#include "ArcTacticsEditorViewModel.h"
#include "ArcTacticsModule.h"
#include "Decision/ArcTacticsConfig.h"
#include "Framework/Application/SlateApplication.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "StructViewerFilter.h"
#include "StructViewerModule.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Layout/SBox.h"

namespace ArcTacticsNodeTypePickerPrivate
{
	class FNodeStructFilter final : public IStructViewerFilter
	{
	public:
		FNodeStructFilter(
			TWeakPtr<FArcTacticsEditorViewModel> InViewModel,
			const EArcTacticsEditorItemKind InKind,
			const FArcTacticsEditorSelection& InTarget,
			const bool bInReplace)
			: ViewModel(InViewModel)
			, Kind(InKind)
			, Target(InTarget)
			, bReplace(bInReplace)
		{
		}

		virtual bool IsStructAllowed(
			const FStructViewerInitializationOptions& InInitOptions,
			const UScriptStruct* InStruct,
			TSharedRef<FStructViewerFilterFuncs> InFilterFuncs) override
		{
			const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
			return Model.IsValid()
				&& !ArcTacticsAuthoring::IsNodeTypeHidden(InStruct)
				&& (bReplace
					? Model->CanReplaceType(Target, InStruct)
					: Model->CanAdd(Kind, Target, InStruct));
		}

		virtual bool IsUnloadedStructAllowed(
			const FStructViewerInitializationOptions& InInitOptions,
			const FSoftObjectPath& InStructPath,
			TSharedRef<FStructViewerFilterFuncs> InFilterFuncs) override
		{
			return false;
		}

	private:
		TWeakPtr<FArcTacticsEditorViewModel> ViewModel;
		EArcTacticsEditorItemKind Kind;
		FArcTacticsEditorSelection Target;
		bool bReplace = false;
	};

	const TCHAR* GetSurfaceName(const EArcTacticsNodeTypePickerSurface Surface)
	{
		return Surface == EArcTacticsNodeTypePickerSurface::Details ? TEXT("Details") : TEXT("View");
	}

	void HandleTypePicked(
		const UScriptStruct* StructType,
		const TWeakPtr<FArcTacticsEditorViewModel>& WeakViewModel,
		const EArcTacticsEditorItemKind Kind,
		const FArcTacticsEditorSelection& Target,
		const bool bReplace,
		const EArcTacticsNodeTypePickerSurface Surface,
		const bool bKeepParentSelected)
	{
		UE_LOGF(
			LogArcTactics,
			Log,
			"[ArcTacticsPicker] NodeType Surface=%ls Select Kind=%u Target=%ls Replace=%ls Type=%ls Dismiss=AllMenus",
			GetSurfaceName(Surface),
			static_cast<uint8>(Kind),
			*Target.ID.ToString(),
			bReplace ? TEXT("true") : TEXT("false"),
			StructType ? *StructType->GetPathName() : TEXT("<null>"));
		if (const TSharedPtr<FArcTacticsEditorViewModel> Model = WeakViewModel.Pin())
		{
			if (bReplace)
			{
				const UArcTacticsConfig* Config = Model->GetConfig();
				const FArcTacticsNodeDefinition* Node = Config ? Config->FindNodeByID(Target.ID) : nullptr;
				if (Node && Node->Node.GetScriptStruct() != StructType)
				{
					Model->ReplaceType(Target, StructType);
				}
			}
			else if (Model->Add(Kind, Target, StructType) && bKeepParentSelected)
			{
				Model->SetSelection(Target);
			}
		}
		FSlateApplication::Get().DismissAllMenus();
	}
}

TSharedRef<SWidget> ArcTacticsNodeTypePicker::MakeNodeTypePicker(
	TWeakPtr<FArcTacticsEditorViewModel> ViewModel,
	const EArcTacticsEditorItemKind Kind,
	const FArcTacticsEditorSelection& Target,
	const bool bReplace,
	const EArcTacticsNodeTypePickerSurface Surface,
	const bool bKeepParentSelected)
{
	UE_LOGF(
		LogArcTactics,
		Log,
		"[ArcTacticsPicker] NodeType Surface=%ls Generate Kind=%u Target=%ls Replace=%ls",
		ArcTacticsNodeTypePickerPrivate::GetSurfaceName(Surface),
		static_cast<uint8>(Kind),
		*Target.ID.ToString(),
		bReplace ? TEXT("true") : TEXT("false"));
	FStructViewerInitializationOptions Options;
	Options.bShowNoneOption = false;
	Options.bShowUnloadedStructs = false;
	Options.bEnableStructDynamicLoading = false;
	Options.StructFilter = MakeShared<ArcTacticsNodeTypePickerPrivate::FNodeStructFilter>(
		ViewModel, Kind, Target, bReplace);
	Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
	Options.DisplayMode = EStructViewerDisplayMode::ListView;
	Options.bAllowViewOptions = true;
	if (bReplace)
	{
		const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
		const UArcTacticsConfig* Config = Model.IsValid() ? Model->GetConfig() : nullptr;
		const FArcTacticsNodeDefinition* Node = Config ? Config->FindNodeByID(Target.ID) : nullptr;
		Options.SelectedStruct = Node ? Node->Node.GetScriptStruct() : nullptr;
	}
	const FOnStructPicked OnPicked = FOnStructPicked::CreateLambda(
		[ViewModel, Kind, Target, bReplace, Surface, bKeepParentSelected](const UScriptStruct* StructType)
		{
			ArcTacticsNodeTypePickerPrivate::HandleTypePicked(
				StructType, ViewModel, Kind, Target, bReplace, Surface, bKeepParentSelected);
		});
	return SNew(SBox)
		.WidthOverride(320.0f)
		.MaxDesiredHeight(500.0f)
		[
			FModuleManager::LoadModuleChecked<FStructViewerModule>(TEXT("StructViewer"))
				.CreateStructViewer(Options, OnPicked)
		];
}

const FSlateBrush* ArcTacticsNodeTypePicker::GetAddConsiderationIcon()
{
	if (FSlateStyleRegistry::FindSlateStyle(TEXT("StateTreeEditorStyle")))
	{
		return FSlateIcon(TEXT("StateTreeEditorStyle"), TEXT("StateTreeEditor.Conditions")).GetIcon();
	}
	return FAppStyle::GetBrush(TEXT("Icons.PlusCircle"));
}
