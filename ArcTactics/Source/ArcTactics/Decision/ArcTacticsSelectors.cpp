// Copyright Lukasz Baran. All Rights Reserved.

#include "Decision/ArcTacticsSelectors.h"

#if WITH_EDITOR
#include "PropertyBindingBindableStructDescriptor.h"
#include "PropertyBindingBindingCollection.h"
#include "PropertyBindingBindingCollectionOwner.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsSelectors)

namespace ArcTacticsSelectorsPrivate
{
#if WITH_EDITOR
	FString EscapeRichText(const FStringView Text)
	{
		FString Result(Text);
		Result.ReplaceInline(TEXT("&"), TEXT("&amp;"));
		Result.ReplaceInline(TEXT("<"), TEXT("&lt;"));
		Result.ReplaceInline(TEXT(">"), TEXT("&gt;"));
		Result.ReplaceInline(TEXT("\""), TEXT("&quot;"));
		return Result;
	}

	FText Emphasize(const FText& Text, const bool bRichText)
	{
		return bRichText
			? FText::FromString(FString::Printf(TEXT("<b>%s</>"), *EscapeRichText(Text.ToString())))
			: Text;
	}

	FText GetItemsSource(
		const FGuid& NodeID,
		const IPropertyBindingBindingCollectionOwner& BindingOwner,
		const bool bRichText)
	{
		const FName ItemsName = TEXT("Items");
		const FPropertyBindingBindingCollection* Bindings = BindingOwner.GetEditorPropertyBindings();
		const FPropertyBindingPath* SourcePath = Bindings
			? Bindings->GetBindingSource(FPropertyBindingPath(NodeID, ItemsName))
			: nullptr;
		if (!SourcePath)
		{
			return Emphasize(FText::FromName(ItemsName), bRichText);
		}
		FString SourceName;
		TInstancedStruct<FPropertyBindingBindableStructDescriptor> Descriptor;
		if (BindingOwner.GetBindableStructByID(SourcePath->GetStructID(), Descriptor))
		{
			SourceName = Descriptor.Get().Name.ToString();
		}
		const FString Path = SourcePath->ToString();
		if (!Path.IsEmpty())
		{
			if (!SourceName.IsEmpty())
			{
				SourceName += TEXT(".");
			}
			SourceName += Path;
		}
		return Emphasize(FText::FromString(SourceName), bRichText);
	}

	FText Describe(
		const FText& ItemKind,
		const FGuid& NodeID,
		const IPropertyBindingBindingCollectionOwner& BindingOwner,
		const bool bRichText)
	{
		return FText::Format(
			NSLOCTEXT("ArcTactics", "SelectorDescription", "Each {0} in {1}"),
			ItemKind,
			GetItemsSource(NodeID, BindingOwner, bRichText));
	}
#endif
}

EArcTacticsNodeRole FArcTacticsSelectorNode::GetRole() const
{
	return EArcTacticsNodeRole::Selector;
}

int32 FArcTacticsSelectorNode::GetItemCount(FConstStructView) const
{
	return 0;
}

bool FArcTacticsSelectorNode::SelectItem(FStructView, int32, FArcTacticsCandidateIdentity&) const
{
	return false;
}

const UScriptStruct* FArcTacticsEntitySelector::GetInstanceDataType() const
{
	return FArcTacticsEntitySelectorInstanceData::StaticStruct();
}

FText FArcTacticsEntitySelector::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "EntitySelector", "Entity Selector");
}

int32 FArcTacticsEntitySelector::GetItemCount(const FConstStructView InstanceData) const
{
	const FArcTacticsEntitySelectorInstanceData* Data =
		InstanceData.GetPtr<FArcTacticsEntitySelectorInstanceData>();
	return Data ? Data->Items.Num() : 0;
}

bool FArcTacticsEntitySelector::SelectItem(
	const FStructView InstanceData,
	const int32 ItemIndex,
	FArcTacticsCandidateIdentity& OutIdentity) const
{
	FArcTacticsEntitySelectorInstanceData* Data = InstanceData.GetPtr<FArcTacticsEntitySelectorInstanceData>();
	if (!Data || !Data->Items.IsValidIndex(ItemIndex))
	{
		return false;
	}
	Data->CurrentEntity = Data->Items[ItemIndex];
	OutIdentity = FArcTacticsCandidateIdentity();
	OutIdentity.Type = EArcTacticsValueIdentity::Entity;
	OutIdentity.Entity = Data->CurrentEntity;
	return true;
}

const UScriptStruct* FArcTacticsLocationSelector::GetInstanceDataType() const
{
	return FArcTacticsLocationSelectorInstanceData::StaticStruct();
}

FText FArcTacticsLocationSelector::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "LocationSelector", "Location Selector");
}

int32 FArcTacticsLocationSelector::GetItemCount(const FConstStructView InstanceData) const
{
	const FArcTacticsLocationSelectorInstanceData* Data =
		InstanceData.GetPtr<FArcTacticsLocationSelectorInstanceData>();
	return Data ? Data->Items.Num() : 0;
}

bool FArcTacticsLocationSelector::SelectItem(
	const FStructView InstanceData,
	const int32 ItemIndex,
	FArcTacticsCandidateIdentity& OutIdentity) const
{
	FArcTacticsLocationSelectorInstanceData* Data = InstanceData.GetPtr<FArcTacticsLocationSelectorInstanceData>();
	if (!Data || !Data->Items.IsValidIndex(ItemIndex))
	{
		return false;
	}
	Data->CurrentLocation = Data->Items[ItemIndex];
	OutIdentity = FArcTacticsCandidateIdentity();
	OutIdentity.Type = EArcTacticsValueIdentity::Vector;
	OutIdentity.Vector = Data->CurrentLocation;
	return true;
}

const UScriptStruct* FArcTacticsTargetSelector::GetInstanceDataType() const
{
	return FArcTacticsTargetSelectorInstanceData::StaticStruct();
}

FText FArcTacticsTargetSelector::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "TargetSelector", "Target Selector");
}

int32 FArcTacticsTargetSelector::GetItemCount(const FConstStructView InstanceData) const
{
	const FArcTacticsTargetSelectorInstanceData* Data =
		InstanceData.GetPtr<FArcTacticsTargetSelectorInstanceData>();
	return Data ? Data->Items.Num() : 0;
}

bool FArcTacticsTargetSelector::SelectItem(
	const FStructView InstanceData,
	const int32 ItemIndex,
	FArcTacticsCandidateIdentity& OutIdentity) const
{
	FArcTacticsTargetSelectorInstanceData* Data = InstanceData.GetPtr<FArcTacticsTargetSelectorInstanceData>();
	if (!Data || !Data->Items.IsValidIndex(ItemIndex))
	{
		return false;
	}
	Data->CurrentTarget = Data->Items[ItemIndex];
	OutIdentity = FArcTacticsCandidateIdentity();
	OutIdentity.Type = EArcTacticsValueIdentity::Entity;
	OutIdentity.Entity = Data->CurrentTarget.Entity;
	return true;
}

#if WITH_EDITOR
FName FArcTacticsEntitySelector::GetIconName() const
{
	return TEXT("AppStyle|Icons.Refresh");
}

FColor FArcTacticsEntitySelector::GetIconColor() const
{
	return FColor(196, 120, 220);
}

FText FArcTacticsEntitySelector::GetDescription(
	const FGuid& NodeID,
	FConstStructView,
	const IPropertyBindingBindingCollectionOwner& BindingOwner,
	const bool bRichText) const
{
	return ArcTacticsSelectorsPrivate::Describe(
		NSLOCTEXT("ArcTactics", "EntitySelectorItem", "entity"), NodeID, BindingOwner, bRichText);
}

FName FArcTacticsLocationSelector::GetIconName() const
{
	return TEXT("AppStyle|Icons.Refresh");
}

FColor FArcTacticsLocationSelector::GetIconColor() const
{
	return FColor(92, 178, 206);
}

FText FArcTacticsLocationSelector::GetDescription(
	const FGuid& NodeID,
	FConstStructView,
	const IPropertyBindingBindingCollectionOwner& BindingOwner,
	const bool bRichText) const
{
	return ArcTacticsSelectorsPrivate::Describe(
		NSLOCTEXT("ArcTactics", "LocationSelectorItem", "location"), NodeID, BindingOwner, bRichText);
}

FName FArcTacticsTargetSelector::GetIconName() const
{
	return TEXT("AppStyle|Icons.Refresh");
}

FColor FArcTacticsTargetSelector::GetIconColor() const
{
	return FColor(220, 96, 96);
}

FText FArcTacticsTargetSelector::GetDescription(
	const FGuid& NodeID,
	FConstStructView,
	const IPropertyBindingBindingCollectionOwner& BindingOwner,
	const bool bRichText) const
{
	return ArcTacticsSelectorsPrivate::Describe(
		NSLOCTEXT("ArcTactics", "TargetSelectorItem", "target"), NodeID, BindingOwner, bRichText);
}
#endif
