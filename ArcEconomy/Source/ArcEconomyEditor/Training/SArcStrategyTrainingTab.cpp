// Copyright Lukasz Baran. All Rights Reserved.

#include "SArcStrategyTrainingTab.h"
#include "SArcRewardChart.h"
#include "ML/ArcStrategyTrainingSubsystem.h"
#include "ML/ArcStrategyCurriculumAsset.h"
#include "ML/ArcArchetypeRewardWeights.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Images/SThrobber.h"
#include "Editor.h"

UArcStrategyTrainingSubsystem* SArcStrategyTrainingTab::GetTrainingSubsystem() const
{
	if (!GEditor)
	{
		return nullptr;
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	return World ? World->GetSubsystem<UArcStrategyTrainingSubsystem>() : nullptr;
}

bool SArcStrategyTrainingTab::IsTraining() const
{
	UArcStrategyTrainingSubsystem* Sub = GetTrainingSubsystem();
	return Sub && Sub->IsTraining();
}

bool SArcStrategyTrainingTab::IsNotTraining() const
{
	return !IsTraining();
}

FReply SArcStrategyTrainingTab::OnStartTraining()
{
	if (!SelectedCurriculum.IsValid())
	{
		StatusText->SetText(FText::FromString(TEXT("Select a Curriculum asset first")));
		StatusText->SetColorAndOpacity(FLinearColor(1.0f, 0.3f, 0.3f));
		return FReply::Handled();
	}
	if (!SelectedRewardWeights.IsValid())
	{
		StatusText->SetText(FText::FromString(TEXT("Select a Reward Weights asset first")));
		StatusText->SetColorAndOpacity(FLinearColor(1.0f, 0.3f, 0.3f));
		return FReply::Handled();
	}

	UArcStrategyTrainingSubsystem* Sub = GetTrainingSubsystem();
	if (!Sub)
	{
		StatusText->SetText(FText::FromString(TEXT("No training subsystem — open a PIE world")));
		StatusText->SetColorAndOpacity(FLinearColor(1.0f, 0.3f, 0.3f));
		return FReply::Handled();
	}

	Sub->SavePathPrefix = SavePath;
	Sub->StartTraining(SelectedCurriculum.Get(), SelectedRewardWeights.Get());

	StatusText->SetText(FText::FromString(TEXT("Starting...")));
	StatusText->SetColorAndOpacity(FLinearColor(0.3f, 1.0f, 0.5f));
	return FReply::Handled();
}

FReply SArcStrategyTrainingTab::OnStopTraining()
{
	UArcStrategyTrainingSubsystem* Sub = GetTrainingSubsystem();
	if (Sub)
	{
		Sub->StopTraining();
	}
	StatusText->SetText(FText::FromString(TEXT("Stopped")));
	StatusText->SetColorAndOpacity(FLinearColor::White);
	return FReply::Handled();
}

FReply SArcStrategyTrainingTab::OnSavePolicies()
{
	UArcStrategyTrainingSubsystem* Sub = GetTrainingSubsystem();
	if (Sub)
	{
		Sub->SavePoliciesNow();
	}
	return FReply::Handled();
}

void SArcStrategyTrainingTab::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Horizontal)

		// ----- Left Panel: Config -----
		+ SSplitter::Slot()
		.Value(0.3f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			.Padding(8.0f)
			[
				SNew(SVerticalBox)

				// Curriculum picker
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 4)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Curriculum")))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SObjectPropertyEntryBox)
						.AllowedClass(UArcStrategyCurriculumAsset::StaticClass())
						.ObjectPath(TAttribute<FString>::CreateLambda([this]() -> FString { return CurriculumObjectPath; }))
						.OnObjectChanged_Lambda([this](const FAssetData& AssetData)
						{
							SelectedCurriculum = Cast<UArcStrategyCurriculumAsset>(AssetData.GetAsset());
							CurriculumObjectPath = AssetData.GetObjectPathString();
						})
					]
				]

				// Reward weights picker
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 4)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Reward Weights")))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SObjectPropertyEntryBox)
						.AllowedClass(UArcArchetypeRewardWeights::StaticClass())
						.ObjectPath(TAttribute<FString>::CreateLambda([this]() -> FString { return RewardWeightsObjectPath; }))
						.OnObjectChanged_Lambda([this](const FAssetData& AssetData)
						{
							SelectedRewardWeights = Cast<UArcArchetypeRewardWeights>(AssetData.GetAsset());
							RewardWeightsObjectPath = AssetData.GetObjectPathString();
						})
					]
				]

				// Save path
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 4)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Save Path")))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SEditableTextBox)
						.Text(FText::FromString(SavePath))
						.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
						{
							SavePath = NewText.ToString();
						})
					]
				]

				// PPO Settings
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 8)
				[
					SNew(SExpandableArea)
					.AreaTitle(FText::FromString(TEXT("PPO Settings")))
					.BodyContent()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("Policy LR")))
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f)
							[
								SNew(SSpinBox<float>)
								.MinValue(0.000001f).MaxValue(0.01f)
								.Value(LearningRatePolicy)
								.OnValueChanged_Lambda([this](float Val) { LearningRatePolicy = Val; })
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("Critic LR")))
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f)
							[
								SNew(SSpinBox<float>)
								.MinValue(0.00001f).MaxValue(0.1f)
								.Value(LearningRateCritic)
								.OnValueChanged_Lambda([this](float Val) { LearningRateCritic = Val; })
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("Hidden Size")))
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f)
							[
								SNew(SSpinBox<int32>)
								.MinValue(32).MaxValue(512)
								.Value(HiddenLayerSize)
								.OnValueChanged_Lambda([this](int32 Val) { HiddenLayerSize = Val; })
							]
						]
					]
				]

				// Buttons
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 12)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Start Training")))
						.OnClicked(this, &SArcStrategyTrainingTab::OnStartTraining)
						.IsEnabled(this, &SArcStrategyTrainingTab::IsNotTraining)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4, 0, 0, 0)
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Stop Training")))
						.OnClicked(this, &SArcStrategyTrainingTab::OnStopTraining)
						.IsEnabled(this, &SArcStrategyTrainingTab::IsTraining)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Save Policies Now")))
					.OnClicked(this, &SArcStrategyTrainingTab::OnSavePolicies)
					.IsEnabled(this, &SArcStrategyTrainingTab::IsTraining)
				]
			]
		]

		// ----- Right Panel: Telemetry -----
		+ SSplitter::Slot()
		.Value(0.7f)
		[
			SNew(SVerticalBox)

			// Status bar
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
				[
					SAssignNew(ThrobberWidget, SCircularThrobber)
					.Radius(8.0f)
					.Visibility_Lambda([this]() -> EVisibility
					{
						return IsTraining() ? EVisibility::Visible : EVisibility::Collapsed;
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 16, 0)
				[
					SAssignNew(StatusText, STextBlock)
					.Text(FText::FromString(TEXT("Idle")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 16, 0)
				[
					SAssignNew(StageText, STextBlock).Text(FText::FromString(TEXT("")))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 16, 0)
				[
					SAssignNew(EpisodeText, STextBlock).Text(FText::FromString(TEXT("")))
				]
			]

			// Reward info
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 0.0f)
			[
				SAssignNew(RewardText, STextBlock)
				.Text(FText::FromString(TEXT("")))
				.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
			]

			// Reward chart
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(8.0f)
			[
				SAssignNew(RewardChart, SArcRewardChart)
			]
		]
	];
}

void SArcStrategyTrainingTab::Tick(
	const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	UArcStrategyTrainingSubsystem* Sub = GetTrainingSubsystem();
	if (!Sub)
	{
		StatusText->SetText(FText::FromString(TEXT("No subsystem")));
		return;
	}

	if (Sub->IsTraining())
	{
		StatusText->SetText(FText::FromString(TEXT("Training")));
		StatusText->SetColorAndOpacity(FLinearColor(0.3f, 1.0f, 0.5f));

		const UArcStrategyCurriculumAsset* Curriculum = Sub->GetActiveCurriculum();
		const int32 TotalStages = Curriculum ? Curriculum->Stages.Num() : 0;
		StageText->SetText(FText::FromString(
			FString::Printf(TEXT("Stage %d/%d"), Sub->GetCurrentStageIndex() + 1, TotalStages)));
		EpisodeText->SetText(FText::FromString(
			FString::Printf(TEXT("Ep %d  Step %d"), Sub->GetTotalEpisodes(), Sub->GetCurrentEpisodeStep())));

		// Show last reward values
		const FArcTrainingTelemetry Telemetry = Sub->GetPerChannelRewards();
		const TArray<float>& SettlementHistory = Sub->GetSettlementRewardHistory();
		const TArray<float>& FactionHistory = Sub->GetFactionRewardHistory();
		const float LastSettlement = SettlementHistory.Num() > 0 ? SettlementHistory.Last() : 0.0f;
		const float LastFaction = FactionHistory.Num() > 0 ? FactionHistory.Last() : 0.0f;
		RewardText->SetText(FText::FromString(
			FString::Printf(TEXT("Last Reward — Settlement: %.3f  Faction: %.3f"), LastSettlement, LastFaction)));

		RewardChart->UpdateData(SettlementHistory, FactionHistory, Sub->GetTotalEpisodes());
	}
	else
	{
		StatusText->SetColorAndOpacity(FLinearColor::White);
		if (!StatusText->GetText().ToString().Contains(TEXT("Select")) &&
			!StatusText->GetText().ToString().Contains(TEXT("Stopped")))
		{
			StatusText->SetText(FText::FromString(TEXT("Idle")));
		}
		StageText->SetText(FText::GetEmpty());
		EpisodeText->SetText(FText::GetEmpty());
		RewardText->SetText(FText::GetEmpty());
	}
}
