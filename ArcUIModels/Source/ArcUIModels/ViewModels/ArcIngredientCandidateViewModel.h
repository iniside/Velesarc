// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/ArcItemId.h"
#include "Items/ArcItemSpec.h"
#include "Mass/EntityHandle.h"
#include "MVVMViewModelBase.h"

#include "ArcIngredientCandidateViewModel.generated.h"

struct FArcItemDefinitionData;
class UArcItemViewModel;
class UArcTagDisplayRegistry;

/** An item in an item store on an entity, with a stack count. Used for inventory rows and input preview rows. */
USTRUCT()
struct ARCUIMODELS_API FArcCraftStoreItemRow
{
	GENERATED_BODY()

	UPROPERTY()
	FMassEntityHandle OwnerEntity;

	UPROPERTY()
	TObjectPtr<UScriptStruct> StoreType = nullptr;

	UPROPERTY()
	FArcItemSpec Spec;

	UPROPERTY()
	int32 Stacks = 0;

	bool IsSameItem(const FArcCraftStoreItemRow& Other) const
	{
		return OwnerEntity == Other.OwnerEntity && StoreType == Other.StoreType && Spec.ItemId == Other.Spec.ItemId;
	}
};

/** View-model for one item row: an ingredient slot candidate or a station input view row. */
UCLASS(BlueprintType)
class ARCUIMODELS_API UArcIngredientCandidateViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FArcItemId ItemId;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TSoftObjectPtr<UObject> Icon;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	int32 Stacks = 0;

	/** True for station input view rows (server input or preview); false for inventory and unowned candidates. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bFromStation = false;

	/** True for station input view rows that are still only in the local preview. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bPreview = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TObjectPtr<UArcItemViewModel> Item;

	const FArcItemId& GetItemId() const { return ItemId; }
	const FText& GetDisplayName() const { return DisplayName; }
	const TSoftObjectPtr<UObject>& GetIcon() const { return Icon; }
	int32 GetStacks() const { return Stacks; }
	bool GetbFromStation() const { return bFromStation; }
	bool GetbPreview() const { return bPreview; }
	UArcItemViewModel* GetItem() const { return Item; }
	FMassEntityHandle GetOwnerEntity() const { return OwnerEntity; }
	UScriptStruct* GetStoreType() const { return StoreType; }

	/** Def may be null — name/icon left empty in that case. TagReg may be null. */
	void RefreshFrom(const FArcItemSpec& Spec, int32 InStacks, const FArcItemDefinitionData* Def, const UArcTagDisplayRegistry* TagReg);

	/** InOwnerEntity and InStoreType are set for inventory and preview rows only. */
	void SetOrigin(bool bInFromStation, bool bInPreview, FMassEntityHandle InOwnerEntity, UScriptStruct* InStoreType);

private:
	FMassEntityHandle OwnerEntity;

	UPROPERTY(Transient)
	TObjectPtr<UScriptStruct> StoreType = nullptr;
};
