// Copyright Lukasz Baran. All Rights Reserved.

#include "Config/ArcMassEntityConfigHelpers.h"

#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "Editor.h"
#include "IContentBrowserSingleton.h"
#include "IDetailCustomization.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "ArcMassEntityConfigHelpers"

namespace ArcMassConfigPicker_Private
{
	// No helpers needed at this time
}

// ── SArcMassTraitMergePreview widget ──────────────────────────────────────────

class SArcMassTraitMergePreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcMassTraitMergePreview)
		: _MergeMode(EArcMassConfigMergeMode::Update)
	{}
	SLATE_ARGUMENT(EArcMassConfigMergeMode, MergeMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// Set the variants and selected template; (re)builds the preview.
	void SetObjects(TArray<UArcMassEntityConfigAsset*> InVariants, UArcMassEntityConfigTemplate* InTemplate);

	virtual ~SArcMassTraitMergePreview() override;

private:
	EArcMassConfigMergeMode MergeMode;
	TArray<TWeakObjectPtr<UArcMassEntityConfigAsset>> SourceVariants;
	TWeakObjectPtr<UArcMassEntityConfigTemplate> Template;
	TArray<TWeakObjectPtr<UArcMassEntityConfigAsset>> PreviewObjects;
	TSharedPtr<SVerticalBox> ContentBox;

	void RebuildPreview();

	// Garbage-mark and drop every transient preview duplicate we created.
	void ClearPreviewObjects();
};

SArcMassTraitMergePreview::~SArcMassTraitMergePreview()
{
	ClearPreviewObjects();
}

void SArcMassTraitMergePreview::ClearPreviewObjects()
{
	for (const TWeakObjectPtr<UArcMassEntityConfigAsset>& PreviewObject : PreviewObjects)
	{
		if (PreviewObject.IsValid())
		{
			PreviewObject->MarkAsGarbage();
		}
	}
	PreviewObjects.Empty();
}

void SArcMassTraitMergePreview::Construct(const FArguments& InArgs)
{
	MergeMode = InArgs._MergeMode;

	ChildSlot
	[
		SAssignNew(ContentBox, SVerticalBox)
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PickTemplatePrompt", "Pick a template to preview"))
		]
	];
}

void SArcMassTraitMergePreview::SetObjects(TArray<UArcMassEntityConfigAsset*> InVariants, UArcMassEntityConfigTemplate* InTemplate)
{
	// Garbage-mark the previous preview duplicates
	ClearPreviewObjects();

	SourceVariants.Reset();
	for (UArcMassEntityConfigAsset* Variant : InVariants)
	{
		SourceVariants.Add(Variant);
	}
	Template = InTemplate;

	RebuildPreview();
}

void SArcMassTraitMergePreview::RebuildPreview()
{
	ContentBox->ClearChildren();

	// Drop any prior batch of preview duplicates before building a fresh one.
	ClearPreviewObjects();

	if (SourceVariants.Num() == 0 || !Template.IsValid())
	{
		ContentBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PickTemplatePrompt", "Pick a template to preview"))
		];
		return;
	}

	UArcMassEntityConfigTemplate* LocalTemplate = Template.Get();

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bShowScrollBar = true;

	TSharedRef<SScrollBox> VariantScrollBox = SNew(SScrollBox)
		.Orientation(Orient_Vertical);

	int32 NumValidVariants = 0;
	for (const TWeakObjectPtr<UArcMassEntityConfigAsset>& WeakVariant : SourceVariants)
	{
		UArcMassEntityConfigAsset* OriginalVariant = WeakVariant.Get();
		if (!OriginalVariant)
		{
			continue;
		}
		++NumValidVariants;

		// Duplicate this variant onto a faithful transient copy and apply the same
		// merge the apply path runs, so the "After Merge" column reflects the real result.
		UArcMassEntityConfigAsset* DuplicatedVariant = DuplicateObject<UArcMassEntityConfigAsset>(OriginalVariant, GetTransientPackage());
		DuplicatedVariant->SetFlags(RF_Standalone | RF_Transient);
		PreviewObjects.Add(DuplicatedVariant);

		switch (MergeMode)
		{
			case EArcMassConfigMergeMode::Update:
				LocalTemplate->UpdateFromTemplate(DuplicatedVariant);
				break;
			case EArcMassConfigMergeMode::SetNewOrReplace:
				LocalTemplate->SetNewOrReplaceTemplate(DuplicatedVariant);
				break;
			case EArcMassConfigMergeMode::Set:
				LocalTemplate->SetTemplate(DuplicatedVariant);
				break;
			default:
				break;
		}

		TSharedRef<IDetailsView> TemplateDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		TemplateDetailsView->SetObject(LocalTemplate);

		TSharedRef<IDetailsView> MergedDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		MergedDetailsView->SetObject(DuplicatedVariant, true);

		TSharedRef<IDetailsView> OriginalDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		OriginalDetailsView->SetObject(OriginalVariant);

		VariantScrollBox->AddSlot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(4.f, 8.f, 4.f, 2.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(OriginalVariant->GetName()))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.MinDesiredHeight(400.f)
				[
					SNew(SSplitter)
					.Orientation(Orient_Horizontal)
					+ SSplitter::Slot()
					.Value(0.333f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TemplatePanelLabel", "Template"))
						]
						+ SVerticalBox::Slot()
						[
							TemplateDetailsView
						]
					]
					+ SSplitter::Slot()
					.Value(0.333f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AfterMergePanelLabel", "After Merge"))
						]
						+ SVerticalBox::Slot()
						[
							MergedDetailsView
						]
					]
					+ SSplitter::Slot()
					.Value(0.334f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("CurrentVariantPanelLabel", "Current Variant"))
						]
						+ SVerticalBox::Slot()
						[
							OriginalDetailsView
						]
					]
				]
			]
		];
	}

	if (NumValidVariants == 0)
	{
		ContentBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("InvalidVariantPrompt", "Invalid variant"))
		];
		return;
	}

	ContentBox->AddSlot()
	[
		VariantScrollBox
	];
}

// ── FArcMassEntityConfigHelpers ───────────────────────────────────────────────

bool FArcMassEntityConfigHelpers::PickTemplateWithPreview(
	UArcMassEntityConfigTemplate*& OutTemplate,
	TArray<UArcMassEntityConfigAsset*> InSelectedVariants,
	EArcMassConfigMergeMode MergeMode)
{
	IContentBrowserSingleton& CBS = IContentBrowserSingleton::Get();

	FAssetPickerConfig PickerConfig;
	PickerConfig.Filter.ClassPaths.Add(UArcMassEntityConfigTemplate::StaticClass()->GetClassPathName());
	PickerConfig.Filter.bRecursiveClasses = false;
	PickerConfig.Filter.bRecursivePaths = false;
	PickerConfig.SelectionMode = ESelectionMode::Single;
	PickerConfig.InitialAssetViewType = EAssetViewType::Column;
	PickerConfig.bAllowNullSelection = true;
	PickerConfig.bShowBottomToolbar = false;
	PickerConfig.bShowPathInColumnView = false;
	PickerConfig.bShowTypeInColumnView = false;
	PickerConfig.bAddFilterUI = true;
	PickerConfig.HiddenColumnNames.Append({"Class", "Type"});

	bool bPressedOk = false;
	FAssetData OutAssetData;

	TSharedPtr<SArcMassTraitMergePreview> MergePreview = SNew(SArcMassTraitMergePreview).MergeMode(MergeMode);

	PickerConfig.OnAssetSelected.BindLambda([MergePreview, &OutAssetData, InSelectedVariants](const FAssetData& AssetData)
	{
		if (AssetData != OutAssetData && AssetData.IsValid())
		{
			UArcMassEntityConfigTemplate* SelectedTemplate = Cast<UArcMassEntityConfigTemplate>(AssetData.GetAsset());
			MergePreview->SetObjects(InSelectedVariants, SelectedTemplate);
		}
		OutAssetData = AssetData;
	});

	PickerConfig.OnAssetDoubleClicked.BindLambda([MergePreview, &OutAssetData, InSelectedVariants](const FAssetData& AssetData)
	{
		if (AssetData != OutAssetData && AssetData.IsValid())
		{
			UArcMassEntityConfigTemplate* SelectedTemplate = Cast<UArcMassEntityConfigTemplate>(AssetData.GetAsset());
			MergePreview->SetObjects(InSelectedVariants, SelectedTemplate);
		}
		OutAssetData = AssetData;
	});

	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
		.Title(LOCTEXT("PickTemplateWindowTitle", "Pick Entity Config Template"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(1400.f, 800.f))
		.MinHeight(600.f)
		.MinWidth(900)
		.SupportsMaximize(true)
		.SupportsMinimize(false);

	TFunction<FReply()> OnClickedOk = [&bPressedOk, PickerWindow]() -> FReply
	{
		bPressedOk = true;
		PickerWindow->RequestDestroyWindow();
		return FReply::Handled();
	};

	TFunction<FReply()> OnClickedCancel = [&bPressedOk, PickerWindow]() -> FReply
	{
		bPressedOk = false;
		PickerWindow->RequestDestroyWindow();
		return FReply::Handled();
	};

	TSharedPtr<SWidget> AssetPicker = CBS.CreateAssetPicker(PickerConfig);

	TSharedPtr<SWidget> WindowContent =
		SNew(SSplitter)
		+ SSplitter::Slot()
		.SizeRule(SSplitter::FractionOfParent)
		.Value(0.65f)
		[
			MergePreview.ToSharedRef()
		]
		+ SSplitter::Slot()
		.SizeRule(SSplitter::FractionOfParent)
		.Value(0.35f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Fill)
			[
				SNew(SBox)
				.MinDesiredHeight(400.f)
				[
					AssetPicker.ToSharedRef()
				]
			]
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(FOnClicked::CreateLambda(OnClickedOk))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("OkButton", "Ok"))
					]
				]
				+ SHorizontalBox::Slot()
				.Padding(FMargin(10.f, 0.f, 0.f, 0.f))
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(FOnClicked::CreateLambda(OnClickedCancel))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CancelButton", "Cancel"))
					]
				]
			]
		];

	PickerWindow->SetContent(WindowContent.ToSharedRef());
	GEditor->EditorAddModalWindow(PickerWindow);

	if (OutAssetData.IsValid())
	{
		OutTemplate = Cast<UArcMassEntityConfigTemplate>(OutAssetData.GetAsset());
	}
	else
	{
		OutTemplate = nullptr;
	}

	return bPressedOk;
}

bool FArcMassEntityConfigHelpers::PreviewUpdate(
	UArcMassEntityConfigTemplate* InTemplate,
	TArray<UArcMassEntityConfigAsset*> InSelectedVariants)
{
	TSharedPtr<SArcMassTraitMergePreview> MergePreview = SNew(SArcMassTraitMergePreview).MergeMode(EArcMassConfigMergeMode::Update);
	MergePreview->SetObjects(InSelectedVariants, InTemplate);

	bool bPressedOk = false;

	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
		.Title(LOCTEXT("PreviewUpdateWindowTitle", "Preview Update from Template"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(1200.f, 700.f))
		.MinHeight(600.f)
		.MinWidth(900)
		.SupportsMaximize(true)
		.SupportsMinimize(false);

	TFunction<FReply()> OnClickedOk = [&bPressedOk, PickerWindow]() -> FReply
	{
		bPressedOk = true;
		PickerWindow->RequestDestroyWindow();
		return FReply::Handled();
	};

	TFunction<FReply()> OnClickedCancel = [&bPressedOk, PickerWindow]() -> FReply
	{
		bPressedOk = false;
		PickerWindow->RequestDestroyWindow();
		return FReply::Handled();
	};

	TSharedPtr<SWidget> WindowContent =
		SNew(SSplitter)
		+ SSplitter::Slot()
		.SizeRule(SSplitter::FractionOfParent)
		.Value(0.65f)
		[
			MergePreview.ToSharedRef()
		]
		+ SSplitter::Slot()
		.SizeRule(SSplitter::FractionOfParent)
		.Value(0.35f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(FOnClicked::CreateLambda(OnClickedOk))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("OkButton", "Ok"))
					]
				]
				+ SHorizontalBox::Slot()
				.Padding(FMargin(10.f, 0.f, 0.f, 0.f))
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(FOnClicked::CreateLambda(OnClickedCancel))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CancelButton", "Cancel"))
					]
				]
			]
		];

	PickerWindow->SetContent(WindowContent.ToSharedRef());
	GEditor->EditorAddModalWindow(PickerWindow);

	return bPressedOk;
}

#undef LOCTEXT_NAMESPACE
