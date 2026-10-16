// Copyright Lukasz Baran. All Rights Reserved.

#include "Metadata/ArcWaterRiverMetadataDetails.h"

#include "ComponentVisualizer.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "IDetailGroup.h"
#include "River/ArcWaterRiverComponent.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FArcWaterRiverMetadataDetails"

namespace ArcWaterRiverMetadataDetailsPrivate
{
	bool IsTextCommitValueSet(ETextCommit::Type CommitInfo)
	{
		return CommitInfo == ETextCommit::OnUserMovedFocus || CommitInfo == ETextCommit::OnEnter;
	}

	template <typename T>
	bool UpdateMultipleValue(TOptional<T>& CurrentValue, T InValue)
	{
		if (!CurrentValue.IsSet())
		{
			CurrentValue = InValue;
			return true;
		}

		if (CurrentValue.GetValue() != InValue)
		{
			CurrentValue.Reset();
			return false;
		}

		return true;
	}

	template <typename T>
	bool CheckIfDifferent(const FArcWaterRiverMetadataDetails& Details, const TArray<FInterpCurvePoint<T>>& Points, const T& NewValue)
	{
		bool bIsModified = false;
		for (int32 Index : Details.SelectedKeys)
		{
			bIsModified |= Points[Index].OutVal != NewValue;
		}

		return bIsModified;
	}

	template <typename T>
	void SetValues(FArcWaterRiverMetadataDetails& Details, TArray<FInterpCurvePoint<T>>& Points, const T& NewValue, ETextCommit::Type CommitInfo, const FText& TransactionText)
	{
		{
			const FScopedTransaction Transaction(TransactionText);
			Details.SplineComp->GetSplinePointsMetadata()->Modify();
			for (int32 Index : Details.SelectedKeys)
			{
				Points[Index].OutVal = NewValue;
			}
		}

		Details.SplineComp->UpdateSpline();
		Details.SplineComp->bSplineHasBeenEdited = true;

		const EPropertyChangeType::Type PropertyChangeType = IsTextCommitValueSet(CommitInfo) ? EPropertyChangeType::ValueSet : EPropertyChangeType::Interactive;

		TArray<FProperty*> SplineProperties;
		for (const FName& PropertyName : USplineComponent::GetSplinePropertyNames())
		{
			SplineProperties.Add(FindFProperty<FProperty>(USplineComponent::StaticClass(), PropertyName));
		}

		FComponentVisualizer::NotifyPropertiesModified(Details.SplineComp, SplineProperties, PropertyChangeType);
		Details.Update(Details.SplineComp, Details.SelectedKeys);

		GEditor->RedrawLevelEditingViewports(true);
	}
}

UArcWaterRiverMetadataDetailsFactory::UArcWaterRiverMetadataDetailsFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UClass* UArcWaterRiverMetadataDetailsFactory::GetMetadataClass() const
{
	return UArcWaterRiverMetadata::StaticClass();
}

TSharedPtr<ISplineMetadataDetails> UArcWaterRiverMetadataDetailsFactory::Create()
{
	return MakeShared<FArcWaterRiverMetadataDetails>();
}

FText FArcWaterRiverMetadataDetails::GetDisplayName() const
{
	return LOCTEXT("ArcWaterRiverMetadataDetails", "ArcWater River");
}

void FArcWaterRiverMetadataDetails::Update(USplineComponent* InSplineComponent, const TSet<int32>& InSelectedKeys)
{
	SplineComp = InSplineComponent;
	SelectedKeys = InSelectedKeys;
	HalfWidthValue.Reset();
	FlowSpeedValue.Reset();

	UArcWaterRiverMetadata* const Metadata = GetMetadata();
	if (Metadata == nullptr)
	{
		return;
	}

	bool bUpdateHalfWidth = true;
	bool bUpdateFlowSpeed = true;

	for (int32 Index : SelectedKeys)
	{
		if (bUpdateHalfWidth)
		{
			bUpdateHalfWidth = ArcWaterRiverMetadataDetailsPrivate::UpdateMultipleValue(HalfWidthValue, Metadata->HalfWidth.Points[Index].OutVal);
		}

		if (bUpdateFlowSpeed)
		{
			bUpdateFlowSpeed = ArcWaterRiverMetadataDetailsPrivate::UpdateMultipleValue(FlowSpeedValue, Metadata->FlowSpeed.Points[Index].OutVal);
		}
	}
}

void FArcWaterRiverMetadataDetails::OnBeginSliderMovement()
{
	EditSliderValueTransaction = MakeUnique<FScopedTransaction>(LOCTEXT("EditRiverProperty", "Edit ArcWater River Property"));
}

void FArcWaterRiverMetadataDetails::OnEndSliderMovement(float NewValue)
{
	EditSliderValueTransaction.Reset();
}

UArcWaterRiverMetadata* FArcWaterRiverMetadataDetails::GetMetadata() const
{
	return SplineComp != nullptr ? Cast<UArcWaterRiverMetadata>(SplineComp->GetSplinePointsMetadata()) : nullptr;
}

void FArcWaterRiverMetadataDetails::OnSetHalfWidth(float NewValue, ETextCommit::Type CommitInfo)
{
	UArcWaterRiverMetadata* const Metadata = GetMetadata();
	if (Metadata == nullptr)
	{
		return;
	}

	if (ArcWaterRiverMetadataDetailsPrivate::CheckIfDifferent(*this, Metadata->HalfWidth.Points, NewValue)
		|| ArcWaterRiverMetadataDetailsPrivate::IsTextCommitValueSet(CommitInfo))
	{
		ArcWaterRiverMetadataDetailsPrivate::SetValues(*this, Metadata->HalfWidth.Points, NewValue, CommitInfo, LOCTEXT("SetSplineHalfWidth", "Set spline point water half width"));
	}
}

void FArcWaterRiverMetadataDetails::OnSetFlowSpeed(float NewValue, ETextCommit::Type CommitInfo)
{
	UArcWaterRiverMetadata* const Metadata = GetMetadata();
	if (Metadata == nullptr)
	{
		return;
	}

	if (ArcWaterRiverMetadataDetailsPrivate::CheckIfDifferent(*this, Metadata->FlowSpeed.Points, NewValue)
		|| ArcWaterRiverMetadataDetailsPrivate::IsTextCommitValueSet(CommitInfo))
	{
		ArcWaterRiverMetadataDetailsPrivate::SetValues(*this, Metadata->FlowSpeed.Points, NewValue, CommitInfo, LOCTEXT("SetSplineFlowSpeed", "Set spline point flow speed"));
	}
}

void FArcWaterRiverMetadataDetails::GenerateChildContent(IDetailGroup& InGroup)
{
	InGroup.AddWidgetRow()
		.RowTag("HalfWidth")
		.FilterString(LOCTEXT("HalfWidth", "Water Half Width"))
		.Visibility(TAttribute<EVisibility>(this, &FArcWaterRiverMetadataDetails::IsEnabled))
		.NameContent()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HalfWidth", "Water Half Width"))
			.ToolTipText(LOCTEXT("HalfWidth_Tooltip", "Half width of the water sheet at this spline point, in centimetres."))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(125.0f)
		.MaxDesiredWidth(125.0f)
		[
			SNew(SNumericEntryBox<float>)
			.Value(this, &FArcWaterRiverMetadataDetails::GetHalfWidth)
			.AllowSpin(true)
			.MinValue(1.0f)
			.MaxValue(TOptional<float>())
			.MinSliderValue(1.0f)
			.MaxSliderValue(TOptional<float>())
			.OnBeginSliderMovement(this, &FArcWaterRiverMetadataDetails::OnBeginSliderMovement)
			.OnEndSliderMovement(this, &FArcWaterRiverMetadataDetails::OnEndSliderMovement)
			.UndeterminedString(LOCTEXT("Multiple", "Multiple"))
			.OnValueCommitted(this, &FArcWaterRiverMetadataDetails::OnSetHalfWidth)
			.OnValueChanged(this, &FArcWaterRiverMetadataDetails::OnSetHalfWidth, ETextCommit::Default)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];

	InGroup.AddWidgetRow()
		.RowTag("FlowSpeed")
		.FilterString(LOCTEXT("FlowSpeed", "Flow Speed"))
		.Visibility(TAttribute<EVisibility>(this, &FArcWaterRiverMetadataDetails::IsEnabled))
		.NameContent()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("FlowSpeed", "Flow Speed"))
			.ToolTipText(LOCTEXT("FlowSpeed_Tooltip", "Flow speed along the spline tangent at this point, in centimetres per second; negative flows against the tangent. Drives the surface material."))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(125.0f)
		.MaxDesiredWidth(125.0f)
		[
			SNew(SNumericEntryBox<float>)
			.Value(this, &FArcWaterRiverMetadataDetails::GetFlowSpeed)
			.AllowSpin(true)
			.MinValue(TOptional<float>())
			.MaxValue(TOptional<float>())
			.MinSliderValue(TOptional<float>())
			.MaxSliderValue(TOptional<float>())
			.OnBeginSliderMovement(this, &FArcWaterRiverMetadataDetails::OnBeginSliderMovement)
			.OnEndSliderMovement(this, &FArcWaterRiverMetadataDetails::OnEndSliderMovement)
			.UndeterminedString(LOCTEXT("Multiple", "Multiple"))
			.OnValueCommitted(this, &FArcWaterRiverMetadataDetails::OnSetFlowSpeed)
			.OnValueChanged(this, &FArcWaterRiverMetadataDetails::OnSetFlowSpeed, ETextCommit::Default)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
}

#undef LOCTEXT_NAMESPACE
