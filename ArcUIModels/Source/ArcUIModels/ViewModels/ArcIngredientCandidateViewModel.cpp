// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/ArcIngredientCandidateViewModel.h"

#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_UIDescription.h"
#include "Items/Fragments/ArcItemFragment_Icon.h"
#include "ViewModels/ArcItemViewModel.h"

void UArcIngredientCandidateViewModel::RefreshFrom(
	const FArcItemSpec& Spec,
	int32 InStacks,
	const FArcItemDefinitionData* Def,
	const UArcTagDisplayRegistry* TagReg)
{
	UE_MVVM_SET_PROPERTY_VALUE(ItemId, Spec.ItemId);

	UE_MVVM_SET_PROPERTY_VALUE(Stacks, InStacks);

	FText NewDisplayName;
	TSoftObjectPtr<UObject> NewIcon;

	if (Def != nullptr)
	{
		if (const FArcItemFragment_UIDescription* UIDesc = Def->FindFragment<FArcItemFragment_UIDescription>())
		{
			if (!UIDesc->ItemName.IsEmpty())
			{
				NewDisplayName = UIDesc->ItemName;
			}
		}
		if (const FArcItemFragment_Icon* IconFrag = Def->FindFragment<FArcItemFragment_Icon>())
		{
			NewIcon = IconFrag->ItemIcon;
		}
	}

	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, NewDisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Icon, NewIcon);

	if (Item == nullptr)
	{
		UArcItemViewModel* NewItem = NewObject<UArcItemViewModel>(this);
		UE_MVVM_SET_PROPERTY_VALUE(Item, NewItem);
	}
	Item->RefreshFrom(Spec, InStacks, TagReg);
}

void UArcIngredientCandidateViewModel::SetOrigin(
	bool bInFromStation,
	bool bInPreview,
	FMassEntityHandle InOwnerEntity,
	UScriptStruct* InStoreType)
{
	UE_MVVM_SET_PROPERTY_VALUE(bFromStation, bInFromStation);
	UE_MVVM_SET_PROPERTY_VALUE(bPreview, bInPreview);
	OwnerEntity = InOwnerEntity;
	StoreType = InStoreType;
}
