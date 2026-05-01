// Copyright Lukasz Baran. All Rights Reserved.

#include "MassProcessorBrowser/SArcMassProcessorBrowser.h"
#include "MassProcessorBrowser/ArcMassProcessorBrowserConfig.h"
#include "MassProcessor.h"
#include "MassSignalProcessorBase.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Styling/AppStyle.h"

// Access private RegisteredSignals via explicit template instantiation hack
namespace ArcMassProcessorBrowserPrivate
{
	template<typename Tag, typename Tag::MemberType Ptr>
	struct AccessPrivate
	{
		friend typename Tag::MemberType GetPrivateMember(Tag) { return Ptr; }
	};

	struct RegisteredSignalsTag
	{
		using MemberType = TArray<FName> UMassSignalProcessorBase::*;
		friend MemberType GetPrivateMember(RegisteredSignalsTag);
	};

	template struct AccessPrivate<RegisteredSignalsTag, &UMassSignalProcessorBase::RegisteredSignals>;
}

#define LOCTEXT_NAMESPACE "ArcMassProcessorBrowser"

void SArcMassProcessorBrowser::Construct(const FArguments& InArgs)
{
	Model.Build(ConfigAsset.Get());
	FilteredRootItems = Model.GetRootItems();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bShowOptions = false;
	DetailsView = PropertyModule.CreateDetailView(DetailsArgs);

	InfoPanelBox = SNew(SVerticalBox);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Refresh", "Refresh"))
				.OnClicked_Lambda([this]()
				{
					OnRefreshClicked();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(SearchBox, SSearchBox)
				.OnTextChanged(this, &SArcMassProcessorBrowser::OnSearchTextChanged)
				.HintText(LOCTEXT("SearchHint", "Search processors..."))
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			+ SSplitter::Slot()
			.Value(0.3f)
			[
				SAssignNew(TreeView, STreeView<TSharedPtr<FArcProcessorTreeItem>>)
				.TreeItemsSource(&FilteredRootItems)
				.OnGenerateRow(this, &SArcMassProcessorBrowser::OnGenerateRow)
				.OnGetChildren(this, &SArcMassProcessorBrowser::OnGetChildren)
				.OnSelectionChanged(this, &SArcMassProcessorBrowser::OnSelectionChanged)
				.SelectionMode(ESelectionMode::Single)
			]
			+ SSplitter::Slot()
			.Value(0.7f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)
				+ SSplitter::Slot()
				.Value(0.6f)
				[
					DetailsView.ToSharedRef()
				]
				+ SSplitter::Slot()
				.Value(0.4f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						InfoPanelBox.ToSharedRef()
					]
				]
			]
		]
	];
}

TSharedRef<ITableRow> SArcMassProcessorBrowser::OnGenerateRow(
	TSharedPtr<FArcProcessorTreeItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	FSlateFontInfo Font = Item->IsLeaf()
		? FAppStyle::GetFontStyle("NormalFont")
		: FAppStyle::GetFontStyle("BoldFont");

	bool bModified = Item->IsLeaf() && Item->ProcessorCDO.IsValid()
		&& FArcMassProcessorBrowserModel::HasModifiedConfigProperties(Item->ProcessorCDO.Get());

	return SNew(STableRow<TSharedPtr<FArcProcessorTreeItem>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 1.0f)
			[
				SNew(STextBlock)
				.Text(Item->DisplayName)
				.Font(Font)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(bModified ? FText::FromString(TEXT("*")) : FText::GetEmpty())
				.Font(FAppStyle::GetFontStyle("BoldFont"))
				.ColorAndOpacity(FLinearColor(1.0f, 0.7f, 0.3f))
			]
		];
}

void SArcMassProcessorBrowser::OnGetChildren(
	TSharedPtr<FArcProcessorTreeItem> Item,
	TArray<TSharedPtr<FArcProcessorTreeItem>>& OutChildren)
{
	if (Item.IsValid())
	{
		OutChildren = Item->Children;
	}
}

void SArcMassProcessorBrowser::OnSelectionChanged(
	TSharedPtr<FArcProcessorTreeItem> Item,
	ESelectInfo::Type SelectInfo)
{
	if (!Item.IsValid() || !Item->IsLeaf())
	{
		DetailsView->SetObject(nullptr);
		SelectedProcessor = nullptr;
		InfoPanelBox->ClearChildren();
		return;
	}

	UMassProcessor* ProcessorCDO = Item->ProcessorCDO.Get();
	SelectedProcessor = ProcessorCDO;

	if (ProcessorCDO)
	{
		DetailsView->SetObject(ProcessorCDO);
		RefreshInfoPanel(ProcessorCDO);
	}
}

void SArcMassProcessorBrowser::RefreshInfoPanel(UMassProcessor* ProcessorCDO)
{
	InfoPanelBox->ClearChildren();
	if (!ProcessorCDO)
	{
		return;
	}

	UEnum* PhaseEnum = StaticEnum<EMassProcessingPhase>();
	FString PhaseName = PhaseEnum ? PhaseEnum->GetNameStringByValue(static_cast<int64>(ProcessorCDO->GetProcessingPhase())) : TEXT("Unknown");

	InfoPanelBox->AddSlot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("PhaseInfo", "Phase: {0}  |  Priority: {1}"),
				FText::FromString(PhaseName),
				FText::AsNumber(ProcessorCDO->GetExecutionPriority())))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		];

	const FMassProcessorExecutionOrder& Order = ProcessorCDO->GetExecutionOrder();

	// Helper: add execution order section
	struct Local
	{
		static void AddExecOrderSection(SArcMassProcessorBrowser* Self, SVerticalBox* Box, const FText& Header, const TArray<FName>& Names)
		{
			if (Names.IsEmpty()) return;

			Box->AddSlot()
				.AutoHeight()
				.Padding(4.0f, 8.0f, 4.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text(Header)
					.Font(FAppStyle::GetFontStyle("BoldFont"))
				];

			for (const FName& Name : Names)
			{
				Box->AddSlot()
					.AutoHeight()
					.Padding(12.0f, 1.0f)
					[
						SNew(SHyperlink)
						.Text(FText::FromName(Name))
						.OnNavigate_Lambda([Self, Name]()
						{
							UClass* FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/*/U%s"), *Name.ToString()));
							if (!FoundClass)
							{
								FoundClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/*/%s"), *Name.ToString()));
							}
							if (FoundClass)
							{
								Self->NavigateToProcessor(FoundClass);
							}
						})
					];
			}
		}

		static void AddReqSection(SVerticalBox* Box, const FText& Header, const TArray<FName>& ReadNames, const TArray<FName>& WriteNames)
		{
			if (ReadNames.IsEmpty() && WriteNames.IsEmpty()) return;

			Box->AddSlot()
				.AutoHeight()
				.Padding(4.0f, 8.0f, 4.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text(Header)
					.Font(FAppStyle::GetFontStyle("BoldFont"))
				];

			for (const FName& Name : WriteNames)
			{
				Box->AddSlot()
					.AutoHeight()
					.Padding(12.0f, 1.0f)
					[
						SNew(STextBlock)
						.Text(FText::Format(LOCTEXT("ReqRW", "{0} [ReadWrite]"), FText::FromName(Name)))
						.ColorAndOpacity(FLinearColor(1.0f, 0.7f, 0.3f))
					];
			}

			for (const FName& Name : ReadNames)
			{
				Box->AddSlot()
					.AutoHeight()
					.Padding(12.0f, 1.0f)
					[
						SNew(STextBlock)
						.Text(FText::Format(LOCTEXT("ReqR", "{0} [Read]"), FText::FromName(Name)))
						.ColorAndOpacity(FLinearColor(0.5f, 0.8f, 1.0f))
					];
			}
		}

		static void AddTagSection(SVerticalBox* Box, const FText& Header, const TArray<FName>& Names)
		{
			if (Names.IsEmpty()) return;

			Box->AddSlot()
				.AutoHeight()
				.Padding(4.0f, 8.0f, 4.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text(Header)
					.Font(FAppStyle::GetFontStyle("BoldFont"))
				];

			for (const FName& Name : Names)
			{
				Box->AddSlot()
					.AutoHeight()
					.Padding(12.0f, 1.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromName(Name))
					];
			}
		}
	};

	Local::AddExecOrderSection(this, InfoPanelBox.Get(), LOCTEXT("ExecuteAfter", "Execute After:"), Order.ExecuteAfter);
	Local::AddExecOrderSection(this, InfoPanelBox.Get(), LOCTEXT("ExecuteBefore", "Execute Before:"), Order.ExecuteBefore);

	// Signal subscriptions (for signal processors)
	UMassSignalProcessorBase* SignalProcessor = Cast<UMassSignalProcessorBase>(ProcessorCDO);
	if (SignalProcessor)
	{
		const TArray<FName>& Signals = SignalProcessor->*GetPrivateMember(ArcMassProcessorBrowserPrivate::RegisteredSignalsTag{});
		Local::AddTagSection(InfoPanelBox.Get(), LOCTEXT("Signals", "Subscribed Signals:"), Signals);
	}

	FArcProcessorRequirementsInfo ReqInfo = FArcMassProcessorBrowserModel::ExtractRequirements(ProcessorCDO);

	Local::AddReqSection(InfoPanelBox.Get(), LOCTEXT("Fragments", "Fragments:"), ReqInfo.FragmentsRead, ReqInfo.FragmentsWrite);
	Local::AddReqSection(InfoPanelBox.Get(), LOCTEXT("SharedFragments", "Shared Fragments:"), ReqInfo.SharedFragmentsRead, ReqInfo.SharedFragmentsWrite);
	Local::AddReqSection(InfoPanelBox.Get(), LOCTEXT("ConstSharedFragments", "Const Shared Fragments:"), ReqInfo.ConstSharedFragmentsRead, TArray<FName>());
	Local::AddReqSection(InfoPanelBox.Get(), LOCTEXT("ChunkFragments", "Chunk Fragments:"), ReqInfo.ChunkFragmentsRead, ReqInfo.ChunkFragmentsWrite);
	Local::AddReqSection(InfoPanelBox.Get(), LOCTEXT("Sparse", "Sparse Elements:"), ReqInfo.SparseRead, ReqInfo.SparseWrite);
	Local::AddReqSection(InfoPanelBox.Get(), LOCTEXT("Subsystems", "Subsystems:"), ReqInfo.SubsystemsRead, ReqInfo.SubsystemsWrite);
	Local::AddTagSection(InfoPanelBox.Get(), LOCTEXT("TagsAll", "Tags (All):"), ReqInfo.TagsAll);
	Local::AddTagSection(InfoPanelBox.Get(), LOCTEXT("TagsAny", "Tags (Any):"), ReqInfo.TagsAny);
	Local::AddTagSection(InfoPanelBox.Get(), LOCTEXT("TagsNone", "Tags (None):"), ReqInfo.TagsNone);
}

void SArcMassProcessorBrowser::NavigateToProcessor(const UClass* ProcessorClass)
{
	TSharedPtr<FArcProcessorTreeItem> Item = Model.FindItemForProcessor(ProcessorClass);
	if (!Item.IsValid())
	{
		return;
	}

	TSharedPtr<FArcProcessorTreeItem> Current = Item->Parent.Pin();
	while (Current.IsValid())
	{
		TreeView->SetItemExpansion(Current, true);
		Current = Current->Parent.Pin();
	}

	TreeView->SetSelection(Item);
	TreeView->RequestScrollIntoView(Item);
}

void SArcMassProcessorBrowser::OnSearchTextChanged(const FText& InText)
{
	FilterText = InText.ToString();
	RebuildFilteredTree();
}

void SArcMassProcessorBrowser::RebuildFilteredTree()
{
	if (FilterText.IsEmpty())
	{
		FilteredRootItems = Model.GetRootItems();
		TreeView->RequestTreeRefresh();
		return;
	}

	FilteredRootItems.Empty();

	for (const TSharedPtr<FArcProcessorTreeItem>& RootItem : Model.GetRootItems())
	{
		if (DoesItemMatchFilter(RootItem))
		{
			FilteredRootItems.Add(RootItem);
		}
	}

	for (const TSharedPtr<FArcProcessorTreeItem>& Item : FilteredRootItems)
	{
		TreeView->SetItemExpansion(Item, true);
		for (const TSharedPtr<FArcProcessorTreeItem>& Child : Item->Children)
		{
			TreeView->SetItemExpansion(Child, true);
		}
	}

	TreeView->RequestTreeRefresh();
}

bool SArcMassProcessorBrowser::DoesItemMatchFilter(const TSharedPtr<FArcProcessorTreeItem>& Item) const
{
	if (!Item.IsValid())
	{
		return false;
	}

	if (Item->IsLeaf())
	{
		return Item->DisplayName.ToString().Contains(FilterText);
	}

	if (Item->DisplayName.ToString().Contains(FilterText))
	{
		return true;
	}

	for (const TSharedPtr<FArcProcessorTreeItem>& Child : Item->Children)
	{
		if (DoesItemMatchFilter(Child))
		{
			return true;
		}
	}

	return false;
}

void SArcMassProcessorBrowser::OnRefreshClicked()
{
	Model.Build(ConfigAsset.Get());
	FilteredRootItems = Model.GetRootItems();
	FilterText.Empty();
	// SSearchBox has no SetText — trigger the delegate path instead
	OnSearchTextChanged(FText::GetEmpty());
	TreeView->RequestTreeRefresh();
	DetailsView->SetObject(nullptr);
	InfoPanelBox->ClearChildren();
	SelectedProcessor = nullptr;
}

#undef LOCTEXT_NAMESPACE
