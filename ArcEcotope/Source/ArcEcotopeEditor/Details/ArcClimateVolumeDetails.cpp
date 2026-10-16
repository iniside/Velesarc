// Copyright Lukasz Baran. All Rights Reserved.

#include "Details/ArcClimateVolumeDetails.h"

#include "ArcEcotopeModule.h"
#include "Climate/ArcClimateBaker.h"
#include "Climate/ArcClimateVolume.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/World.h"
#include "IPropertyUtilities.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "ArcClimateVolumeDetails"

namespace ArcClimateVolumeDetailsPrivate
{
	const FName ActionsCategoryName(TEXT("ArcClimate Actions"));

	TSharedRef<SWidget> MakeButton(
		const FText& Label,
		const FText& ToolTip,
		const FOnClicked& OnClicked)
	{
		return SNew(SBox)
			.MinDesiredWidth(150.0f)
			[
				SNew(SButton)
					.Text(Label)
					.ToolTipText(ToolTip)
					.OnClicked(OnClicked)
			];
	}
}

TSharedRef<IDetailCustomization> FArcClimateVolumeDetails::MakeInstance()
{
	return MakeShared<FArcClimateVolumeDetails>();
}

void FArcClimateVolumeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		return;
	}

	SelectedVolume = Cast<AArcClimateVolume>(Objects[0].Get());
	if (!SelectedVolume.IsValid())
	{
		return;
	}

	PropertyUtilities = DetailBuilder.GetPropertyUtilities();
	IDetailCategoryBuilder& ActionsCategory = DetailBuilder.EditCategory(
		ArcClimateVolumeDetailsPrivate::ActionsCategoryName,
		LOCTEXT("ActionsCategory", "ArcClimate Actions"),
		ECategoryPriority::Important);
	ActionsCategory.AddCustomRow(LOCTEXT("BakeClimateFilter", "Bake Climate"))
	.WholeRowContent()[ArcClimateVolumeDetailsPrivate::MakeButton(
		LOCTEXT("BakeClimate", "Bake Climate"),
		LOCTEXT("BakeClimateTip", "Bake and atomically replace this volume's climate product. This action cannot be undone."),
		FOnClicked::CreateSP(this, &FArcClimateVolumeDetails::BakeClimate))];
	ActionsCategory.AddCustomRow(LOCTEXT("ClearClimateFilter", "Clear Climate"))
	.WholeRowContent()[ArcClimateVolumeDetailsPrivate::MakeButton(
		LOCTEXT("ClearClimate", "Clear Climate"),
		LOCTEXT("ClearClimateTip", "Remove this volume's baked climate product. This action cannot be undone."),
		FOnClicked::CreateSP(this, &FArcClimateVolumeDetails::ClearClimate))];
	ActionsCategory.AddCustomRow(LOCTEXT("ValidateClimateFilter", "Validate Climate"))
	.WholeRowContent()[ArcClimateVolumeDetailsPrivate::MakeButton(
		LOCTEXT("ValidateClimate", "Validate Climate"),
		LOCTEXT("ValidateClimateTip", "Validate this volume's current baked climate product."),
		FOnClicked::CreateSP(this, &FArcClimateVolumeDetails::ValidateClimate))];
}

AArcClimateVolume* FArcClimateVolumeDetails::GetVolume() const
{
	return SelectedVolume.Get();
}

FReply FArcClimateVolumeDetails::BakeClimate()
{
	AArcClimateVolume* Volume = GetVolume();
	if (Volume == nullptr)
	{
		ReportFailure(LOCTEXT("BakeAction", "Climate bake"), LOCTEXT("MissingVolume", "No climate volume is selected."));
		return FReply::Handled();
	}

	const UWorld* World = Volume->GetWorld();
	if (World != nullptr && World->GetWorldPartition() != nullptr && Volume->GetIsSpatiallyLoaded())
	{
		ReportFailure(
			LOCTEXT("BakeAction", "Climate bake"),
			LOCTEXT("SpatiallyLoaded", "Climate volumes must be always-loaded in World Partition worlds before baking."));
		return FReply::Handled();
	}

	FText Error;
	if (!FArcClimateBaker::Bake(*Volume, Error))
	{
		ReportFailure(LOCTEXT("BakeAction", "Climate bake"), Error);
		return FReply::Handled();
	}

	Volume->MarkPackageDirty();
	RefreshDetails();
	UE_LOGF(LogArcEcotope, Log,"Climate bake succeeded for '%ls'.", *Volume->GetPathName());
	return FReply::Handled();
}

FReply FArcClimateVolumeDetails::ClearClimate()
{
	AArcClimateVolume* Volume = GetVolume();
	if (Volume == nullptr)
	{
		ReportFailure(LOCTEXT("ClearAction", "Climate clear"), LOCTEXT("MissingVolume", "No climate volume is selected."));
		return FReply::Handled();
	}

	Volume->ClearClimateProduct();
	Volume->MarkPackageDirty();
	RefreshDetails();
	UE_LOGF(LogArcEcotope, Log,"Cleared climate product for '%ls'.", *Volume->GetPathName());
	return FReply::Handled();
}

FReply FArcClimateVolumeDetails::ValidateClimate()
{
	const AArcClimateVolume* Volume = GetVolume();
	if (Volume == nullptr)
	{
		ReportFailure(LOCTEXT("ValidateAction", "Climate validation"), LOCTEXT("MissingVolume", "No climate volume is selected."));
		return FReply::Handled();
	}

	FText Error;
	if (!Volume->ValidateClimateProduct(Error))
	{
		ReportFailure(LOCTEXT("ValidateAction", "Climate validation"), Error);
		return FReply::Handled();
	}

	const FText Message = FText::Format(
		LOCTEXT("ValidationSucceeded", "Climate product on '{0}' is valid."),
		FText::FromString(Volume->GetPathName()));
	ReportSuccess(Message);
	return FReply::Handled();
}

void FArcClimateVolumeDetails::ReportFailure(const FText& Action, const FText& Error) const
{
	const FText Message = FText::Format(
		LOCTEXT("ActionFailed", "{0} failed:\n\n{1}"),
		Action,
		Error.IsEmpty() ? LOCTEXT("UnknownError", "Unknown error.") : Error);
	UE_LOGF(LogArcEcotope, Error,"%ls", *Message.ToString());
	FMessageDialog::Open(EAppMsgType::Ok, Message);
}

void FArcClimateVolumeDetails::ReportSuccess(const FText& Message) const
{
	UE_LOGF(LogArcEcotope, Log,"%ls", *Message.ToString());
	FMessageDialog::Open(EAppMsgType::Ok, Message);
}

void FArcClimateVolumeDetails::RefreshDetails() const
{
	if (PropertyUtilities.IsValid())
	{
		PropertyUtilities->ForceRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
