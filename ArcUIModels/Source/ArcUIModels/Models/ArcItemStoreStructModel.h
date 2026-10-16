// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcFrontendMVVMModelComponent.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemViewFilter.h"
#include "Mass/EntityHandle.h"

#include "ArcItemStoreStructModel.generated.h"

class UArcItemStoreViewModel;
class UArcMassItemEventSubsystem;
class UMassAgentComponent;

USTRUCT(BlueprintType)
struct ARCUIMODELS_API FArcItemStoreStructModel : public FArcFrontendMVVMStructModel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FName ContextName = TEXT("ItemStore");

	UPROPERTY(EditAnywhere)
	FGameplayTag RegistrationTag;

	// Seeded on the bound VM during Initialize. Widgets may override it at
	// runtime via UArcItemStoreViewModel::SetFilter and friends.
	UPROPERTY(EditAnywhere)
	FArcItemViewFilter InitialFilter;

	// Which derived store fragment this model mirrors. Must derive
	// FArcMassItemStoreFragment. Default is the base type — useful until a
	// project defines specialised inventory/equipment/etc. stores.
	UPROPERTY(EditAnywhere, meta=(MetaStruct="/Script/ArcMassItemsRuntime.ArcMassItemStoreFragment"))
	TObjectPtr<const UScriptStruct> StoreFragmentType = nullptr;

	virtual void CreateViewModels(UArcFrontendMVVMModelComponent& Component) override;
	virtual void Initialize(UArcFrontendMVVMModelComponent& Component) override;
	virtual void Deinitialize(UArcFrontendMVVMModelComponent& Component) override;

	// Called from signal subscriptions. Public so the file-scoped namespace helper
	// that captures this pointer inside a lambda can reach it.
	void HandleSignal(FName SignalName, TConstArrayView<FMassEntityHandle> Entities, TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakComponent);

private:
	// Resolves BoundEntity, subscribes to item events, and calls RebuildFromFragment.
	// Called from Initialize and from the OnMassAgentComponentEntityAssociated callback.
	// Returns true if binding succeeded (BoundEntity is now valid).
	bool TryBindToEntity(UArcFrontendMVVMModelComponent& Component);

	void RebuildFromFragment(UArcFrontendMVVMModelComponent& Component);

	TWeakObjectPtr<UArcItemStoreViewModel> StoreVM;

	FMassEntityHandle BoundEntity;
	TWeakObjectPtr<UArcMassItemEventSubsystem> BoundEventSubsystem;
	TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakBoundComponent;

	FDelegateHandle AddedHandle;
	FDelegateHandle RemovedHandle;
	FDelegateHandle SlotChangedHandle;
	FDelegateHandle StacksChangedHandle;
	FDelegateHandle EntityAssociatedHandle;
	FDelegateHandle FilterChangedHandle;
};
