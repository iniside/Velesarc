// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcFrontendMVVMModelComponent.h"
#include "Bags/ArcBagId.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"

#include "ArcBagStructModel.generated.h"

class UArcBagContainerViewModel;
class UArcMassBagEventSubsystem;
struct FArcBagInstance;
struct FArcMassBagStoreFragment;
struct FMassEntityManager;

USTRUCT(BlueprintType)
struct ARCUIMODELS_API FArcBagStructModel : public FArcFrontendMVVMStructModel
{
	GENERATED_BODY()

	// Context name the created container VM is registered under in the global collection.
	UPROPERTY(EditAnywhere)
	FName ContextName = TEXT("Bags");

	// Tag the created UArcBagContainerViewModel is registered under in the component's ViewModels map
	// (the tag-based resolvers read that map).
	UPROPERTY(EditAnywhere, meta = (Categories = "ViewModel"))
	FGameplayTag RegistrationTag;

	virtual void Initialize(UArcFrontendMVVMModelComponent& Component) override;
	virtual void Deinitialize(UArcFrontendMVVMModelComponent& Component) override;

private:
	bool TryBindToEntity(UArcFrontendMVVMModelComponent& Component);
	void RebuildAll(UArcFrontendMVVMModelComponent& Component);
	void RefreshBag(UArcFrontendMVVMModelComponent& Component, FArcBagId BagId);
	void RebuildSlots(UArcBagContainerViewModel& Container, FArcBagInstance* Bag, FArcMassBagStoreFragment& Frag, FMassEntityManager& Manager);

	FMassEntityHandle BoundEntity;
	TWeakObjectPtr<UArcMassBagEventSubsystem> BoundBagEvents;
	TWeakObjectPtr<UArcBagContainerViewModel> ContainerVM;

	FDelegateHandle BagListChangedHandle;
	FDelegateHandle BagSlotsChangedHandle;
	FDelegateHandle EntityAssociatedHandle;
	FDelegateHandle ActiveBagChangedHandle;
};
