// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcFrontendMVVMModelComponent.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "ArcTargetStructModel.generated.h"

struct FArcTargetCandidate;
class UArcTargetingSubsystem;
class UArcTargetViewModel;

USTRUCT(BlueprintType)
struct ARCUIMODELS_API FArcTargetStructModel : public FArcFrontendMVVMStructModel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FName ContextName = TEXT("VM_Target");

	UPROPERTY(EditAnywhere)
	FGameplayTag RegistrationTag;

	UPROPERTY(EditAnywhere, meta = (Categories = "GlobalTargeting"))
	FGameplayTag TargetingTag;

	UPROPERTY(EditAnywhere)
	FVector MarkerOffset = FVector::ZeroVector;

	FArcTargetStructModel()
	{
		bTickForLocalOnly = true;
	}

	virtual void CreateViewModels(UArcFrontendMVVMModelComponent& Component) override;
	virtual void Initialize(UArcFrontendMVVMModelComponent& Component) override;
	virtual void Tick(float DeltaTime, UArcFrontendMVVMModelComponent& Component) override;
	virtual void Deinitialize(UArcFrontendMVVMModelComponent& Component) override;

private:
	bool TryBindToEntity(UArcFrontendMVVMModelComponent& Component);
	void ApplyResult(const TArray<FArcTargetCandidate>* Result);
	bool WriteMarker(UWorld& World);

	FMassEntityHandle TargetEntity;
	FMassEntityHandle BoundEntity;
	TWeakObjectPtr<UArcTargetViewModel> TargetVM;
	TWeakObjectPtr<UArcTargetingSubsystem> BoundTargeting;
	FDelegateHandle ResultHandle;
	FDelegateHandle EntityAssociatedHandle;
};
