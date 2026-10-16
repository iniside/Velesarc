// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulator/SArcTacticsSimulator.h"

#include "ResponseCurve/SArcResponseCurvePreview.h"
#include "Styling/AppStyle.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcTacticsSimulator"

namespace ArcTacticsSimulatorWidgetPrivate
{
	const FLinearColor AcceptedColor(0.35f, 0.85f, 0.45f, 1.0f);
	const FLinearColor BelowMinScoreColor(0.95f, 0.80f, 0.30f, 1.0f);
	const FLinearColor RejectedColor(0.95f, 0.40f, 0.35f, 1.0f);

	FText FormatPercent(const float Value)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 1;
		Options.MaximumFractionalDigits = 1;
		return FText::AsPercent(Value, &Options);
	}

	FText FormatScore(const float Value)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 3;
		Options.MaximumFractionalDigits = 3;
		return FText::AsNumber(Value, &Options);
	}
}

SArcTacticsSimulator::~SArcTacticsSimulator()
{
	if (UArcTacticsConfig* ConfigPtr = Config.Get())
	{
		ConfigPtr->OnAuthoringChanged().RemoveAll(this);
	}
}

void SArcTacticsSimulator::Construct(const FArguments& InArgs, UArcTacticsConfig* InConfig)
{
	Config = InConfig;
	Simulator.Rebuild(InConfig);
	if (InConfig)
	{
		InConfig->OnAuthoringChanged().AddSP(this, &SArcTacticsSimulator::HandleAuthoringChanged);
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			MakeHeader()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(RowsBox, SVerticalBox)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.0f, 4.0f)
		[
			SNew(STextBlock)
			.Text(this, &SArcTacticsSimulator::GetNothingPickedText)
		]
	];

	SyncRowWidgets(true);
}

void SArcTacticsSimulator::HandleAuthoringChanged(const EArcTacticsAuthoringChange Change)
{
	if (Change == EArcTacticsAuthoringChange::Structure)
	{
		Simulator.Rebuild(Config.Get());
		SyncRowWidgets(true);
		return;
	}
	Simulator.Recompute();
	SyncRowWidgets(false);
}

void SArcTacticsSimulator::GatherStructure(TArray<FGuid>& OutStructure) const
{
	OutStructure.Reset();
	for (const FArcTacticsSimulatorRow& Row : Simulator.GetRows())
	{
		OutStructure.Add(Row.NodeID);
		for (const FArcTacticsSimulatorConsideration& Consideration : Row.Considerations)
		{
			OutStructure.Add(Consideration.NodeID);
		}
		OutStructure.Add(FGuid());
	}
}

void SArcTacticsSimulator::SyncRowWidgets(const bool bForce)
{
	if (!RowsBox.IsValid())
	{
		return;
	}
	TArray<FGuid> Structure;
	GatherStructure(Structure);
	if (!bForce && Structure == BuiltStructure)
	{
		return;
	}
	BuiltStructure = MoveTemp(Structure);

	RowsBox->ClearChildren();
	for (const FArcTacticsSimulatorRow& Row : Simulator.GetRows())
	{
		RowsBox->AddSlot()
		.AutoHeight()
		.Padding(2.0f)
		[
			MakeRowWidget(Row)
		];
	}
	if (Simulator.GetRows().IsEmpty())
	{
		RowsBox->AddSlot()
		.AutoHeight()
		.Padding(6.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoRows", "No actions or selectors to simulate."))
		];
	}
}

TSharedRef<SWidget> SArcTacticsSimulator::MakeHeader()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SelectionLabel", "Selection:"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 12.0f, 0.0f)
			[
				SNew(SSegmentedControl<EArcTacticsSelectionMode>)
				.ToolTipText(LOCTEXT("SelectionTooltip", "Selection mode used by the simulation. Runtime selection comes from the entity's ArcTactics trait settings."))
				.Value_Lambda([this]()
				{
					return Simulator.GetSelectionMode();
				})
				.OnValueChanged_Lambda([this](const EArcTacticsSelectionMode Mode)
				{
					Simulator.SetSelectionMode(Mode);
				})
				+ SSegmentedControl<EArcTacticsSelectionMode>::Slot(EArcTacticsSelectionMode::HighestScore)
				.Text(LOCTEXT("ModeHighestScore", "Highest score"))
				+ SSegmentedControl<EArcTacticsSelectionMode>::Slot(EArcTacticsSelectionMode::RandomFromTopPercent)
				.Text(LOCTEXT("ModeRandomFromTopPercent", "Random from top percent"))
				+ SSegmentedControl<EArcTacticsSelectionMode>::Slot(EArcTacticsSelectionMode::WeightedRandom)
				.Text(LOCTEXT("ModeWeightedRandom", "Weighted random"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TopPercentLabel", "Top percent:"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 12.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(80.0f)
				[
					SNew(SSpinBox<float>)
					.MinValue(0.0f)
					.MaxValue(1.0f)
					.MinSliderValue(0.0f)
					.MaxSliderValue(1.0f)
					.Delta(0.01f)
					.MinFractionalDigits(2)
					.MaxFractionalDigits(2)
					.IsEnabled_Lambda([this]()
					{
						return Simulator.GetSelectionMode() == EArcTacticsSelectionMode::RandomFromTopPercent;
					})
					.Value_Lambda([this]()
					{
						return Simulator.GetTopPercent();
					})
					.OnValueChanged_Lambda([this](const float Value)
					{
						Simulator.SetTopPercent(Value);
					})
					.OnValueCommitted_Lambda([this](const float Value, ETextCommit::Type)
					{
						Simulator.SetTopPercent(Value);
					})
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SArcTacticsSimulator::GetMinScoreText)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("ResetInputs", "Reset inputs"))
				.ToolTipText(LOCTEXT("ResetInputsTooltip", "Reset every slider, veto and lock to its default."))
				.OnClicked_Lambda([this]()
				{
					Simulator.ResetInputs();
					SyncRowWidgets(false);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("RunMonteCarlo", "Randomize (Monte Carlo)"))
				.ToolTipText(FText::Format(
					LOCTEXT("RunMonteCarloTooltip", "Sample every unlocked input uniformly {0} times and show the mean chance and eligible share per row. Vetoes and locked inputs keep their values."),
					FText::AsNumber(FArcTacticsSimulator::MonteCarloSamples)))
				.OnClicked_Lambda([this]()
				{
					Simulator.RunMonteCarlo();
					SyncRowWidgets(false);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ClearMonteCarlo", "Clear Monte Carlo"))
				.IsEnabled_Lambda([this]()
				{
					return Simulator.HasMonteCarlo();
				})
				.OnClicked_Lambda([this]()
				{
					Simulator.ClearMonteCarlo();
					return FReply::Handled();
				})
			]
		];
}

TSharedRef<SWidget> SArcTacticsSimulator::MakeRowWidget(const FArcTacticsSimulatorRow& Row)
{
	const FGuid RowID = Row.NodeID;
	const TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	for (const FArcTacticsSimulatorConsideration& Consideration : Row.Considerations)
	{
		Body->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 1.0f)
		[
			MakeConsiderationWidget(RowID, Consideration.NodeID)
		];
	}
	if (Row.Considerations.IsEmpty())
	{
		Body->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoConsiderations", "No considerations."))
		];
	}

	return SNew(SExpandableArea)
		.InitiallyCollapsed(!ExpandedRows.Contains(RowID))
		.AllowAnimatedTransition(false)
		.OnAreaExpansionChanged_Lambda([this, RowID](const bool bExpanded)
		{
			if (bExpanded)
			{
				ExpandedRows.Add(RowID);
			}
			else
			{
				ExpandedRows.Remove(RowID);
			}
		})
		.HeaderContent()
		[
			MakeRowHeader(RowID)
		]
		.BodyContent()
		[
			SNew(SBox)
			.Padding(FMargin(16.0f, 2.0f, 4.0f, 4.0f))
			[
				Body
			]
		];
}

TSharedRef<SWidget> SArcTacticsSimulator::MakeRowHeader(const FGuid& RowID)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(this, &SArcTacticsSimulator::GetRowNameText, RowID)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(6.0f, 0.0f)
		[
			SNew(SBox)
			.MinDesiredWidth(70.0f)
			[
				SNew(STextBlock)
				.Text(this, &SArcTacticsSimulator::GetRowPriorityText, RowID)
				.ToolTipText(LOCTEXT("PriorityTooltip", "Branch priority. Only eligible rows of the highest priority present get a chance."))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(6.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(110.0f)
			.HeightOverride(16.0f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SProgressBar)
					.Percent(this, &SArcTacticsSimulator::GetRowScorePercent, RowID)
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SArcTacticsSimulator::GetRowScoreText, RowID)
				]
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(6.0f, 0.0f)
		[
			SNew(SBox)
			.MinDesiredWidth(140.0f)
			[
				SNew(STextBlock)
				.Text(this, &SArcTacticsSimulator::GetRowOutcomeText, RowID)
				.ColorAndOpacity(this, &SArcTacticsSimulator::GetRowOutcomeColor, RowID)
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(6.0f, 0.0f)
		[
			SNew(SBox)
			.MinDesiredWidth(90.0f)
			[
				SNew(STextBlock)
				.Text(this, &SArcTacticsSimulator::GetRowChanceText, RowID)
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(6.0f, 0.0f)
		[
			SNew(STextBlock)
			.Visibility(this, &SArcTacticsSimulator::GetMonteCarloVisibility)
			.Text(this, &SArcTacticsSimulator::GetRowMonteCarloText, RowID)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(6.0f, 0.0f)
		[
			SNew(STextBlock)
			.Visibility(this, &SArcTacticsSimulator::GetRowBoundVisibility, RowID)
			.Text(LOCTEXT("BoundDefaultUsed", "bound: default used"))
			.ToolTipText(LOCTEXT("BoundDefaultUsedTooltip", "A branch weight, priority or response curve is bound. The simulator ignores bindings and uses the authored default."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
}

TSharedRef<SWidget> SArcTacticsSimulator::MakeConsiderationWidget(const FGuid& RowID, const FGuid& ConsiderationID)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SArcTacticsSimulator::GetConsiderationNameText, RowID, ConsiderationID)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Visibility(this, &SArcTacticsSimulator::GetConsiderationBoundVisibility, RowID, ConsiderationID)
				.Text(LOCTEXT("ConsiderationBound", "(bound)"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(120.0f)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.0f)
				.MaxValue(1.0f)
				.MinSliderValue(0.0f)
				.MaxSliderValue(1.0f)
				.Delta(0.01f)
				.MinFractionalDigits(2)
				.MaxFractionalDigits(3)
				.Value(this, &SArcTacticsSimulator::GetConsiderationInput, ConsiderationID)
				.OnValueChanged_Lambda([this, ConsiderationID](const float Value)
				{
					Simulator.SetInput(ConsiderationID, Value);
				})
				.OnValueCommitted_Lambda([this, ConsiderationID](const float Value, ETextCommit::Type)
				{
					Simulator.SetInput(ConsiderationID, Value);
				})
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.0f, 0.0f)
		[
			SNew(SCheckBox)
			.IsChecked(this, &SArcTacticsSimulator::GetVetoState, ConsiderationID)
			.OnCheckStateChanged_Lambda([this, ConsiderationID](const ECheckBoxState State)
			{
				Simulator.SetVeto(ConsiderationID, State == ECheckBoxState::Checked);
			})
			.ToolTipText(LOCTEXT("VetoTooltip", "Simulate this consideration rejecting the candidate."))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Veto", "Veto"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.0f, 0.0f)
		[
			SNew(SCheckBox)
			.IsChecked(this, &SArcTacticsSimulator::GetLockState, ConsiderationID)
			.OnCheckStateChanged_Lambda([this, ConsiderationID](const ECheckBoxState State)
			{
				Simulator.SetLocked(ConsiderationID, State == ECheckBoxState::Checked);
			})
			.ToolTipText(LOCTEXT("LockTooltip", "Keep this input fixed during Monte Carlo sampling."))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Lock", "Lock"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.0f, 0.0f)
		[
			SNew(SArcResponseCurvePreview)
			.DesiredSize(FVector2D(72.0f, 40.0f))
			.bShowLabels(false)
			.OnGetCurve(this, &SArcTacticsSimulator::GetConsiderationCurve, ConsiderationID)
			.Marker(this, &SArcTacticsSimulator::GetConsiderationMarker, ConsiderationID)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.0f, 0.0f)
		[
			SNew(SBox)
			.MinDesiredWidth(200.0f)
			[
				SNew(STextBlock)
				.Text(this, &SArcTacticsSimulator::GetConsiderationValuesText, RowID, ConsiderationID)
			]
		];
}

const FArcTacticsSimulatorConsideration* SArcTacticsSimulator::FindConsideration(
	const FGuid& RowID,
	const FGuid& ConsiderationID) const
{
	const FArcTacticsSimulatorRow* Row = Simulator.FindRow(RowID);
	if (!Row)
	{
		return nullptr;
	}
	return Row->Considerations.FindByPredicate([&ConsiderationID](const FArcTacticsSimulatorConsideration& Consideration)
	{
		return Consideration.NodeID == ConsiderationID;
	});
}

const FArcResponseCurve* SArcTacticsSimulator::GetConsiderationCurve(const FGuid ConsiderationID) const
{
	const UArcTacticsConfig* ConfigPtr = Config.Get();
	const FArcTacticsNodeDefinition* Definition = ConfigPtr ? ConfigPtr->FindNodeByID(ConsiderationID) : nullptr;
	const FArcUtilityConsideration* Consideration = Definition
		? Definition->InstanceData.GetPtr<FArcUtilityConsideration>()
		: nullptr;
	return Consideration ? &Consideration->ResponseCurve : nullptr;
}

FText SArcTacticsSimulator::GetMinScoreText() const
{
	return FText::Format(LOCTEXT("MinScoreFormat", "Min score: {0}"),
		ArcTacticsSimulatorWidgetPrivate::FormatScore(Simulator.GetMinScore()));
}

FText SArcTacticsSimulator::GetNothingPickedText() const
{
	const FText Chance = ArcTacticsSimulatorWidgetPrivate::FormatPercent(Simulator.GetNothingPickedChance());
	if (!Simulator.HasMonteCarlo())
	{
		return FText::Format(LOCTEXT("NothingPickedFormat", "Nothing picked: {0}"), Chance);
	}
	return FText::Format(LOCTEXT("NothingPickedMonteCarloFormat", "Nothing picked: {0}   Monte Carlo mean: {1}"),
		Chance,
		ArcTacticsSimulatorWidgetPrivate::FormatPercent(Simulator.GetMonteCarloNothingPickedChance()));
}

FText SArcTacticsSimulator::GetRowNameText(const FGuid RowID) const
{
	const FArcTacticsSimulatorRow* Row = Simulator.FindRow(RowID);
	if (!Row)
	{
		return FText::GetEmpty();
	}
	return Row->bFromSelector
		? FText::Format(LOCTEXT("SelectorRowNameFormat", "{0}  ·  via {1}"), Row->Name, Row->SelectorName)
		: Row->Name;
}

FText SArcTacticsSimulator::GetRowPriorityText(const FGuid RowID) const
{
	const FArcTacticsSimulatorRow* Row = Simulator.FindRow(RowID);
	return Row
		? FText::Format(LOCTEXT("PriorityFormat", "Priority {0}"), FText::AsNumber(Row->Priority))
		: FText::GetEmpty();
}

FText SArcTacticsSimulator::GetRowScoreText(const FGuid RowID) const
{
	const FArcTacticsSimulatorRow* Row = Simulator.FindRow(RowID);
	return Row ? ArcTacticsSimulatorWidgetPrivate::FormatScore(Row->Score) : FText::GetEmpty();
}

TOptional<float> SArcTacticsSimulator::GetRowScorePercent(const FGuid RowID) const
{
	const FArcTacticsSimulatorRow* Row = Simulator.FindRow(RowID);
	return Row ? FMath::Clamp(Row->Score, 0.0f, 1.0f) : 0.0f;
}

FText SArcTacticsSimulator::GetRowOutcomeText(const FGuid RowID) const
{
	const FArcTacticsSimulatorRow* Row = Simulator.FindRow(RowID);
	if (!Row)
	{
		return FText::GetEmpty();
	}
	const FText Reason = FText::FromName(Row->Reason);
	switch (Row->Outcome)
	{
	case EArcTacticsSimulatorOutcome::Accepted:
		return LOCTEXT("OutcomeAccepted", "Accepted");
	case EArcTacticsSimulatorOutcome::BelowMinScore:
		return LOCTEXT("OutcomeBelowMinScore", "Below min score");
	case EArcTacticsSimulatorOutcome::Vetoed:
		return Row->Reason.IsNone()
			? LOCTEXT("OutcomeVetoed", "Vetoed")
			: FText::Format(LOCTEXT("OutcomeVetoedFormat", "Vetoed: {0}"), Reason);
	case EArcTacticsSimulatorOutcome::Invalid:
		return Row->Reason.IsNone()
			? LOCTEXT("OutcomeInvalid", "Invalid")
			: FText::Format(LOCTEXT("OutcomeInvalidFormat", "Invalid: {0}"), Reason);
	}
	return FText::GetEmpty();
}

FSlateColor SArcTacticsSimulator::GetRowOutcomeColor(const FGuid RowID) const
{
	const FArcTacticsSimulatorRow* Row = Simulator.FindRow(RowID);
	if (!Row)
	{
		return FSlateColor::UseForeground();
	}
	switch (Row->Outcome)
	{
	case EArcTacticsSimulatorOutcome::Accepted:
		return FSlateColor(ArcTacticsSimulatorWidgetPrivate::AcceptedColor);
	case EArcTacticsSimulatorOutcome::BelowMinScore:
		return FSlateColor(ArcTacticsSimulatorWidgetPrivate::BelowMinScoreColor);
	case EArcTacticsSimulatorOutcome::Vetoed:
	case EArcTacticsSimulatorOutcome::Invalid:
		return FSlateColor(ArcTacticsSimulatorWidgetPrivate::RejectedColor);
	}
	return FSlateColor::UseForeground();
}

FText SArcTacticsSimulator::GetRowChanceText(const FGuid RowID) const
{
	const FArcTacticsSimulatorRow* Row = Simulator.FindRow(RowID);
	if (!Row)
	{
		return FText::GetEmpty();
	}
	return FText::Format(LOCTEXT("ChanceFormat", "Chance {0}"),
		ArcTacticsSimulatorWidgetPrivate::FormatPercent(Row->Chance));
}

FText SArcTacticsSimulator::GetRowMonteCarloText(const FGuid RowID) const
{
	const FArcTacticsSimulatorRow* Row = Simulator.FindRow(RowID);
	if (!Row)
	{
		return FText::GetEmpty();
	}
	return FText::Format(LOCTEXT("MonteCarloRowFormat", "MC mean {0}   eligible {1}"),
		ArcTacticsSimulatorWidgetPrivate::FormatPercent(Row->MonteCarloMeanChance),
		ArcTacticsSimulatorWidgetPrivate::FormatPercent(Row->MonteCarloEligibleShare));
}

EVisibility SArcTacticsSimulator::GetMonteCarloVisibility() const
{
	return Simulator.HasMonteCarlo() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SArcTacticsSimulator::GetRowBoundVisibility(const FGuid RowID) const
{
	const FArcTacticsSimulatorRow* Row = Simulator.FindRow(RowID);
	return Row && Row->bHasBoundDefaults ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SArcTacticsSimulator::GetConsiderationNameText(const FGuid RowID, const FGuid ConsiderationID) const
{
	const FArcTacticsSimulatorConsideration* Consideration = FindConsideration(RowID, ConsiderationID);
	return Consideration ? Consideration->Name : FText::GetEmpty();
}

EVisibility SArcTacticsSimulator::GetConsiderationBoundVisibility(const FGuid RowID, const FGuid ConsiderationID) const
{
	const FArcTacticsSimulatorConsideration* Consideration = FindConsideration(RowID, ConsiderationID);
	return Consideration && Consideration->bBound ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SArcTacticsSimulator::GetConsiderationValuesText(const FGuid RowID, const FGuid ConsiderationID) const
{
	const FArcTacticsSimulatorConsideration* Consideration = FindConsideration(RowID, ConsiderationID);
	if (!Consideration)
	{
		return FText::GetEmpty();
	}
	if (!Consideration->bEvaluated)
	{
		return LOCTEXT("NotEvaluated", "not evaluated");
	}
	return FText::Format(LOCTEXT("ConsiderationValuesFormat", "input {0}   curved {1}"),
		ArcTacticsSimulatorWidgetPrivate::FormatScore(GetConsiderationInput(ConsiderationID)),
		ArcTacticsSimulatorWidgetPrivate::FormatScore(Consideration->CurvedScore));
}

float SArcTacticsSimulator::GetConsiderationInput(const FGuid ConsiderationID) const
{
	const FArcTacticsSimulatorInputState* State = Simulator.FindState(ConsiderationID);
	return State ? State->Input : 0.0f;
}

TOptional<float> SArcTacticsSimulator::GetConsiderationMarker(const FGuid ConsiderationID) const
{
	const FArcTacticsSimulatorInputState* State = Simulator.FindState(ConsiderationID);
	return State ? TOptional<float>(State->Input) : TOptional<float>();
}

ECheckBoxState SArcTacticsSimulator::GetVetoState(const FGuid ConsiderationID) const
{
	const FArcTacticsSimulatorInputState* State = Simulator.FindState(ConsiderationID);
	return State && State->bVeto ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState SArcTacticsSimulator::GetLockState(const FGuid ConsiderationID) const
{
	const FArcTacticsSimulatorInputState* State = Simulator.FindState(ConsiderationID);
	return State && State->bLocked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

#undef LOCTEXT_NAMESPACE
