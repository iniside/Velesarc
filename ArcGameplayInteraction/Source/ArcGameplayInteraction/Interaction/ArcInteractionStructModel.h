// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcFrontendMVVMModelComponent.h"
#include "GameplayTagContainer.h"
#include "ArcInteractionStructModel.generated.h"

class UArcInteractionViewModel;

USTRUCT(BlueprintType)
struct ARCGAMEPLAYINTERACTION_API FArcInteractionStructModel : public FArcFrontendMVVMStructModel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FName ContextName = TEXT("VM_Interaction");

	UPROPERTY(EditAnywhere)
	FGameplayTag RegistrationTag;

	UPROPERTY(EditAnywhere)
	FVector AnchorOffset = FVector::ZeroVector;

	FArcInteractionStructModel()
	{
		bTickForLocalOnly = true;
	}

	virtual void CreateViewModels(UArcFrontendMVVMModelComponent& Component) override;
	virtual void Initialize(UArcFrontendMVVMModelComponent& Component) override;
	virtual void Tick(float DeltaTime, UArcFrontendMVVMModelComponent& Component) override;
	virtual void Deinitialize(UArcFrontendMVVMModelComponent& Component) override;

private:
	bool ResolveAnchor(UArcFrontendMVVMModelComponent& Component, FVector& OutAnchor) const;

	TWeakObjectPtr<UArcInteractionViewModel> InteractionVM;
};
