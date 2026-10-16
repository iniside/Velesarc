// Copyright Lukasz Baran. All Rights Reserved.

#include "Widgets/SArcTacticsCompileResults.h"

#include "ArcTacticsEditorViewModel.h"
#include "Decision/ArcTacticsConfig.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SArcTacticsCompileResults"

SArcTacticsCompileResults::~SArcTacticsCompileResults()
{
	if (const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin())
	{
		Model->OnCompileResultsChanged().RemoveAll(this);
	}
}

void SArcTacticsCompileResults::Construct(
	const FArguments& InArgs,
	const TSharedRef<FArcTacticsEditorViewModel> InViewModel)
{
	ViewModel = InViewModel;
	OnDiagnosticClicked = InArgs._OnDiagnosticClicked;
	InViewModel->OnCompileResultsChanged().AddSP(
		this,
		&SArcTacticsCompileResults::RebuildResults);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.0f, 4.0f)
		[
			SNew(SBorder)
			.Padding(6.0f)
			.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
			[
				SNew(STextBlock)
				.Text(this, &SArcTacticsCompileResults::GetArtifactStatusText)
				.ColorAndOpacity(this, &SArcTacticsCompileResults::GetArtifactStatusColor)
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(4.0f, 0.0f, 4.0f, 4.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(ResultsBox, SVerticalBox)
			]
		]
	];

	RebuildResults();
}

void SArcTacticsCompileResults::RebuildResults()
{
	if (!ResultsBox.IsValid())
	{
		return;
	}
	ResultsBox->ClearChildren();
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	if (!Model.IsValid())
	{
		return;
	}
	const TArray<FArcTacticsCompilerDiagnostic>& Diagnostics = Model->GetCompileDiagnostics();
	if (Diagnostics.IsEmpty())
	{
		ResultsBox->AddSlot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(STextBlock)
			.Text(this, &SArcTacticsCompileResults::GetEmptyResultsText)
			.AutoWrapText(true)
		];
		return;
	}

	const UArcTacticsConfig* Config = Model->GetConfig();
	for (const FArcTacticsCompilerDiagnostic& Diagnostic : Diagnostics)
	{
		const FText Location = Config && Diagnostic.NodeID.IsValid()
			? Config->GetAuthoringName(Diagnostic.NodeID)
			: LOCTEXT("ConfigurationLocation", "Configuration");
		ResultsBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
			.OnClicked(this, &SArcTacticsCompileResults::HandleDiagnosticClicked, Diagnostic.NodeID)
			[
				SNew(SBorder)
				.Padding(6.0f)
				.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(Diagnostic.bIsError
							? LOCTEXT("ErrorSeverity", "Error")
							: LOCTEXT("WarningSeverity", "Warning"))
						.ColorAndOpacity(Diagnostic.bIsError
							? FLinearColor::Red
							: FLinearColor::Yellow)
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(FText::Format(
							LOCTEXT("DiagnosticFormat", "{0}: {1}"),
							Location,
							Diagnostic.Message))
						.AutoWrapText(true)
					]
				]
			]
		];
	}
}

FText SArcTacticsCompileResults::GetArtifactStatusText() const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	return Model.IsValid() && Model->IsCompiledArtifactValid()
		? LOCTEXT("ArtifactValid", "Compiled artifact: valid")
		: LOCTEXT("ArtifactInvalid", "Compiled artifact: invalid");
}

FText SArcTacticsCompileResults::GetEmptyResultsText() const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	return Model.IsValid() && Model->IsCompiledArtifactValid()
		? LOCTEXT("NoDiagnostics", "The compiled artifact is valid and has no diagnostics.")
		: LOCTEXT("NoDiagnosticsInvalid", "The compiled artifact is invalid. Compile or save the asset to validate it.");
}

FSlateColor SArcTacticsCompileResults::GetArtifactStatusColor() const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	return Model.IsValid() && Model->IsCompiledArtifactValid()
		? FSlateColor(FLinearColor(0.2f, 0.8f, 0.35f))
		: FSlateColor(FLinearColor::Red);
}

FReply SArcTacticsCompileResults::HandleDiagnosticClicked(const FGuid NodeID) const
{
	OnDiagnosticClicked.ExecuteIfBound(NodeID);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
