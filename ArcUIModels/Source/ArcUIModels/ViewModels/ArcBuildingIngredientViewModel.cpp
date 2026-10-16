// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/ArcBuildingIngredientViewModel.h"

#include "ArcBuildIngredient.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/Fragments/ArcItemFragment_Icon.h"
#include "Items/Fragments/ArcItemFragment_UIDescription.h"

#define LOCTEXT_NAMESPACE "ArcBuildingIngredientViewModel"

void UArcBuildingIngredientViewModel::RefreshFrom(
	const FArcBuildIngredient& Ingredient,
	const FArcMassItemStoreFragment& Inventory)
{
	UE_MVVM_SET_PROPERTY_VALUE(SlotName, Ingredient.SlotName);

	const int32 NewRequired = Ingredient.Amount;
	const int32 NewHave = Ingredient.CountAvailable(Inventory);

	UE_MVVM_SET_PROPERTY_VALUE(Required, NewRequired);
	UE_MVVM_SET_PROPERTY_VALUE(Have, NewHave);
	UE_MVVM_SET_PROPERTY_VALUE(bSatisfied, NewHave >= NewRequired);

	FFormatNamedArguments Args;
	Args.Add(TEXT("Have"), FText::AsNumber(NewHave));
	Args.Add(TEXT("Required"), FText::AsNumber(NewRequired));
	const FText NewHaveRequiredText = FText::Format(LOCTEXT("HaveRequiredFmt", "{Have} / {Required}"), Args);

	UE_MVVM_SET_PROPERTY_VALUE(HaveRequiredText, NewHaveRequiredText);

	FText NewDisplayName = Ingredient.SlotName;
	TSoftObjectPtr<UObject> NewIcon;

	const FPrimaryAssetId ItemAssetId = Ingredient.GetDisplayItemDefinitionId();
	if (ItemAssetId.IsValid())
	{
		const FArcItemDefinitionData* ItemDef = FArcItemDefinitionData::Find(ItemAssetId);
		if (ItemDef != nullptr)
		{
			const FArcItemFragment_UIDescription* UIDesc =
				ItemDef->FindFragment<FArcItemFragment_UIDescription>();
			if (UIDesc != nullptr && !UIDesc->ItemName.IsEmpty())
			{
				NewDisplayName = UIDesc->ItemName;
			}

			const FArcItemFragment_Icon* IconFragment = ItemDef->FindFragment<FArcItemFragment_Icon>();
			if (IconFragment != nullptr && !IconFragment->ItemIcon.IsNull())
			{
				NewIcon = IconFragment->ItemIcon;
			}
		}
	}

	SetDisplayName(NewDisplayName);
	SetIcon(NewIcon);
}

void UArcBuildingIngredientViewModel::SetDisplayName(const FText& NewDisplayName)
{
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, NewDisplayName);
}

void UArcBuildingIngredientViewModel::SetIcon(const TSoftObjectPtr<UObject>& NewIcon)
{
	UE_MVVM_SET_PROPERTY_VALUE(Icon, NewIcon);
}

#undef LOCTEXT_NAMESPACE
