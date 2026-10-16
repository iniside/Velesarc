// Copyright Lukasz Baran. All Rights Reserved.

#include "Details/ArcWaterRiverModifierDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "Generation/ArcWaterRiverGeneration.h"
#include "River/ArcWaterRiverModifier.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcWaterRiverModifierDetails"

namespace ArcWaterRiverModifierDetailsPrivate_0d91
{
	class FReachGroups
	{
	public:
		explicit FReachGroups(int32 Count)
		{
			Parents.SetNumUninitialized(Count);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Parents[Index] = Index;
			}
		}

		int32 Find(int32 Index)
		{
			int32 Root = Index;
			while (Parents[Root] != Root)
			{
				Root = Parents[Root];
			}
			while (Parents[Index] != Index)
			{
				const int32 Next = Parents[Index];
				Parents[Index] = Root;
				Index = Next;
			}
			return Root;
		}

		void Union(int32 A, int32 B)
		{
			const int32 RootA = Find(A);
			const int32 RootB = Find(B);
			if (RootA != RootB)
			{
				Parents[FMath::Max(RootA, RootB)] = FMath::Min(RootA, RootB);
			}
		}

	private:
		TArray<int32> Parents;
	};

	int32 CountSourceComponents(const FArcWaterRiverGeneratedData& Data)
	{
		if (Data.Reaches.IsEmpty())
		{
			return 0;
		}
		TMap<FVector2D, int32> JunctionByPosition;
		for (int32 JunctionIndex = 0; JunctionIndex < Data.JunctionPositions.Num(); ++JunctionIndex)
		{
			JunctionByPosition.Add(Data.JunctionPositions[JunctionIndex], JunctionIndex);
		}
		FReachGroups Groups(Data.Reaches.Num());
		TArray<int32> FirstReachAtJunction;
		FirstReachAtJunction.Init(INDEX_NONE, Data.JunctionPositions.Num());
		for (int32 ReachIndex = 0; ReachIndex < Data.Reaches.Num(); ++ReachIndex)
		{
			for (const FVector& Point : Data.Reaches[ReachIndex].SplinePoints)
			{
				const int32* const JunctionIndex = JunctionByPosition.Find(FVector2D(Point.X, Point.Y));
				if (JunctionIndex == nullptr)
				{
					continue;
				}
				int32& FirstReach = FirstReachAtJunction[*JunctionIndex];
				if (FirstReach == INDEX_NONE)
				{
					FirstReach = ReachIndex;
				}
				else
				{
					Groups.Union(FirstReach, ReachIndex);
				}
			}
		}
		TSet<int32> Roots;
		for (int32 ReachIndex = 0; ReachIndex < Data.Reaches.Num(); ++ReachIndex)
		{
			Roots.Add(Groups.Find(ReachIndex));
		}
		return Roots.Num();
	}
}

TSharedRef<IDetailCustomization> FArcWaterRiverModifierDetails::MakeInstance()
{
	return MakeShared<FArcWaterRiverModifierDetails>();
}

void FArcWaterRiverModifierDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		return;
	}
	Modifier = Cast<UArcWaterRiverModifier>(Objects[0].Get());
	if (!Modifier.IsValid())
	{
		return;
	}
	RefreshResultStats();

	IDetailCategoryBuilder& Actions = DetailBuilder.EditCategory(
		TEXT("ArcWater River Actions"), LOCTEXT("ActionsCategory", "ArcWater River Actions"),
		ECategoryPriority::Important);
	Actions.AddCustomRow(LOCTEXT("RebuildFilter", "Rebuild Rivers"))
	.WholeRowContent()
	[
		SNew(SBox)
		.MinDesiredWidth(180.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Rebuild", "Rebuild Rivers"))
				.ToolTipText(LOCTEXT("RebuildTip", "Clear the current result, then read fresh Rivers sources and pre-stamp terrain heights."))
				.IsEnabled(this, &FArcWaterRiverModifierDetails::CanRebuildRivers)
				.OnClicked(this, &FArcWaterRiverModifierDetails::RebuildRivers)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Clear", "Clear"))
				.ToolTipText(LOCTEXT("ClearTip", "Remove the generated river result."))
				.IsEnabled(this, &FArcWaterRiverModifierDetails::CanRebuildRivers)
				.OnClicked(this, &FArcWaterRiverModifierDetails::ClearRivers)
			]
		]
	];
	Actions.AddCustomRow(LOCTEXT("PreviewFilter", "Show Preview Refresh Preview Through Terrain"))
	.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.IsChecked(this, &FArcWaterRiverModifierDetails::GetRiverPreviewVisibilityState)
			.OnCheckStateChanged(this, &FArcWaterRiverModifierDetails::SetRiverPreviewVisible)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ShowPreview", "Show Preview"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(12.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("RefreshPreview", "Refresh Preview"))
			.ToolTipText(LOCTEXT("RefreshPreviewTip", "Refresh the overlay from the built result, or compute it without changing terrain when the result is empty."))
			.IsEnabled(this, &FArcWaterRiverModifierDetails::CanRebuildRivers)
			.OnClicked(this, &FArcWaterRiverModifierDetails::RefreshRiverPreview)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(12.0f, 0.0f, 0.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.IsChecked(this, &FArcWaterRiverModifierDetails::GetRiverPreviewThroughTerrainState)
			.OnCheckStateChanged(this, &FArcWaterRiverModifierDetails::SetRiverPreviewThroughTerrain)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ThroughTerrain", "Through Terrain"))
			]
		]
	];
	Actions.AddCustomRow(LOCTEXT("PreviewLegendFilter", "Preview legend"))
	.WholeRowContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("PreviewLegend", "Blue: channel width  Orange: falloff boundary  White: surface center  Violet: bottom center  Yellow: profile sections"))
		.AutoWrapText(true)
	];

	IDetailCategoryBuilder& Results = DetailBuilder.EditCategory(
		TEXT("ArcWater River Result"), LOCTEXT("ResultCategory", "ArcWater River Result"),
		ECategoryPriority::Important);
	Results.AddCustomRow(LOCTEXT("ResultFilter", "River generation result"))
	.WholeRowContent()
	[
		SNew(STextBlock)
		.Text(this, &FArcWaterRiverModifierDetails::GetResultText)
		.AutoWrapText(true)
	];
	Results.AddCustomRow(LOCTEXT("ErrorFilter", "River generation error"))
	.Visibility(TAttribute<EVisibility>(this, &FArcWaterRiverModifierDetails::GetErrorVisibility))
	.WholeRowContent()
	[
		SNew(STextBlock)
		.Text(this, &FArcWaterRiverModifierDetails::GetErrorText)
		.ColorAndOpacity(FLinearColor(1.0f, 0.2f, 0.1f))
		.AutoWrapText(true)
	];
}

FReply FArcWaterRiverModifierDetails::RebuildRivers()
{
	LastError = FText::GetEmpty();
	if (UArcWaterRiverModifier* const SelectedModifier = Modifier.Get())
	{
		ArcWaterRiverGeneration::Rebuild(*SelectedModifier, LastError);
		RefreshResultStats();
		if (GEditor != nullptr)
		{
			GEditor->RedrawLevelEditingViewports(true);
		}
	}
	return FReply::Handled();
}

FReply FArcWaterRiverModifierDetails::ClearRivers()
{
	LastError = FText::GetEmpty();
	if (UArcWaterRiverModifier* const SelectedModifier = Modifier.Get())
	{
		SelectedModifier->ClearGeneratedRivers();
		RefreshResultStats();
		if (GEditor != nullptr)
		{
			GEditor->RedrawLevelEditingViewports(true);
		}
	}
	return FReply::Handled();
}

FReply FArcWaterRiverModifierDetails::RefreshRiverPreview()
{
	LastError = FText::GetEmpty();
	if (UArcWaterRiverModifier* const SelectedModifier = Modifier.Get())
	{
		const FArcWaterRiverGeneratedData& BuiltData = SelectedModifier->GetGeneratedData();
		if (!SelectedModifier->DoesGeneratedDataNeedRebuild() && !BuiltData.Reaches.IsEmpty())
		{
			SelectedModifier->SetRiverPreviewData(BuiltData, SelectedModifier->GetComponentTransform());
			SelectedModifier->SetRiverPreviewVisible(true);
		}
		else
		{
			FArcWaterRiverGeneratedData PreviewData;
			FTransform PreviewFrame;
			if (ArcWaterRiverGeneration::Compute(*SelectedModifier, PreviewData, PreviewFrame, LastError))
			{
				SelectedModifier->SetRiverPreviewData(PreviewData, PreviewFrame);
				SelectedModifier->SetRiverPreviewVisible(true);
			}
			else
			{
				SelectedModifier->ClearRiverPreview();
			}
		}
		if (SelectedModifier->IsRiverPreviewTruncated())
		{
			LastError = FText::Format(
				LOCTEXT("PreviewTruncated", "Preview drew {0} of {1} channel segments and {2} of {3} profile sections to stay within DebugDraw capacity."),
				FText::AsNumber(SelectedModifier->GetRiverPreviewDrawnSegmentCount()),
				FText::AsNumber(SelectedModifier->GetRiverPreviewSourceSegmentCount()),
				FText::AsNumber(SelectedModifier->GetRiverPreviewDrawnCrossSectionCount()),
				FText::AsNumber(SelectedModifier->GetRiverPreviewSourceCrossSectionCount()));
		}
		if (GEditor != nullptr)
		{
			GEditor->RedrawLevelEditingViewports(true);
		}
	}
	return FReply::Handled();
}

void FArcWaterRiverModifierDetails::SetRiverPreviewVisible(ECheckBoxState CheckState)
{
	if (UArcWaterRiverModifier* const SelectedModifier = Modifier.Get())
	{
		if (CheckState == ECheckBoxState::Checked && !SelectedModifier->HasRiverPreviewData())
		{
			RefreshRiverPreview();
			return;
		}
		SelectedModifier->SetRiverPreviewVisible(CheckState == ECheckBoxState::Checked);
		if (GEditor != nullptr)
		{
			GEditor->RedrawLevelEditingViewports(true);
		}
	}
}

void FArcWaterRiverModifierDetails::SetRiverPreviewThroughTerrain(ECheckBoxState CheckState)
{
	if (UArcWaterRiverModifier* const SelectedModifier = Modifier.Get())
	{
		SelectedModifier->SetRiverPreviewThroughTerrain(CheckState == ECheckBoxState::Checked);
		if (GEditor != nullptr)
		{
			GEditor->RedrawLevelEditingViewports(true);
		}
	}
}

ECheckBoxState FArcWaterRiverModifierDetails::GetRiverPreviewVisibilityState() const
{
	const UArcWaterRiverModifier* const SelectedModifier = Modifier.Get();
	return SelectedModifier != nullptr && SelectedModifier->IsRiverPreviewVisible()
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

ECheckBoxState FArcWaterRiverModifierDetails::GetRiverPreviewThroughTerrainState() const
{
	const UArcWaterRiverModifier* const SelectedModifier = Modifier.Get();
	return SelectedModifier != nullptr && SelectedModifier->IsRiverPreviewThroughTerrain()
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

bool FArcWaterRiverModifierDetails::CanRebuildRivers() const
{
	const UArcWaterRiverModifier* const SelectedModifier = Modifier.Get();
	const AActor* const Owner = SelectedModifier != nullptr ? SelectedModifier->GetOwner() : nullptr;
	return SelectedModifier != nullptr && !SelectedModifier->IsTemplate()
		&& Owner != nullptr && !Owner->IsTemplate() && SelectedModifier->GetWorld() != nullptr;
}

FText FArcWaterRiverModifierDetails::GetResultText() const
{
	using namespace ArcWaterRiverModifierDetailsPrivate_0d91;

	const UArcWaterRiverModifier* const SelectedModifier = Modifier.Get();
	if (SelectedModifier == nullptr)
	{
		return FText::GetEmpty();
	}
	return FText::Format(
		LOCTEXT("ResultSummary",
			"State: {0}\nSource graph components: {1}\nReaches: {2}\nJunctions: {3}\nSections: {4}\nTotal length: {5} m\nSpline terrain modifiers: {6}; maximum field {7} x {8}, spacing {9} cm, estimated pair bytes {10}\nExternal Rivers or terrain edits require Rebuild Rivers."),
		SelectedModifier->DoesGeneratedDataNeedRebuild()
			? LOCTEXT("Empty", "Empty")
			: LOCTEXT("Built", "Built"),
		FText::AsNumber(SourceComponentCount),
		FText::AsNumber(ReachCount),
		FText::AsNumber(JunctionCount),
		FText::AsNumber(SectionCount),
		FText::AsNumber(TotalLengthCm * 0.01),
		FText::AsNumber(SplineModifierCount),
		FText::AsNumber(MaxFieldDims.X),
		FText::AsNumber(MaxFieldDims.Y),
		FText::AsNumber(FieldSpacingCm),
		FText::AsNumber(EstimatedRasterBytes));
}

FText FArcWaterRiverModifierDetails::GetErrorText() const
{
	return LastError;
}

EVisibility FArcWaterRiverModifierDetails::GetErrorVisibility() const
{
	return LastError.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

void FArcWaterRiverModifierDetails::RefreshResultStats()
{
	using namespace ArcWaterRiverModifierDetailsPrivate_0d91;

	SourceComponentCount = 0;
	ReachCount = 0;
	JunctionCount = 0;
	SectionCount = 0;
	TotalLengthCm = 0.0;
	SplineModifierCount = 0;
	MaxFieldDims = FIntPoint::ZeroValue;
	FieldSpacingCm = 0.0;
	EstimatedRasterBytes = 0;
	const UArcWaterRiverModifier* const SelectedModifier = Modifier.Get();
	if (SelectedModifier == nullptr)
	{
		return;
	}
	const FArcWaterRiverGeneratedData& Data = SelectedModifier->GetGeneratedData();
	SourceComponentCount = CountSourceComponents(Data);
	ReachCount = Data.Reaches.Num();
	JunctionCount = Data.JunctionPositions.Num();
	SplineModifierCount = SelectedModifier->GetGeneratedSplineModifierCount();
	MaxFieldDims = SelectedModifier->GetGeneratedMaxFieldDims();
	FieldSpacingCm = SelectedModifier->GetGeneratedFieldSpacingCm();
	EstimatedRasterBytes = SelectedModifier->GetGeneratedEstimatedRasterBytes();
	for (const FArcWaterRiverGeneratedReach& Reach : Data.Reaches)
	{
		SectionCount += Reach.Sections.Num();
		TotalLengthCm += Reach.LengthCm;
	}
}

#undef LOCTEXT_NAMESPACE
