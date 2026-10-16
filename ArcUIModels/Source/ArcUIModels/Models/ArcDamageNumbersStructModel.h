// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcFrontendMVVMModelComponent.h"
#include "GameplayTagContainer.h"
#include "ViewModels/ArcDamageNumbersViewModel.h"
#include "ArcDamageNumbersStructModel.generated.h"

class UArcGameEventSubsystem;
struct FArcGameEvent;

USTRUCT(BlueprintType)
struct ARCUIMODELS_API FArcDamageNumbersStructModel : public FArcFrontendMVVMStructModel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FName ContextName = TEXT("VM_DamageNumbers");

	UPROPERTY(EditAnywhere)
	FGameplayTag RegistrationTag;

	UPROPERTY(EditAnywhere)
	FGameplayTag CritTag;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", Units = "s"))
	float Lifetime = 1.5f;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "1"))
	int32 MaxEntries = 24;

	UPROPERTY(EditAnywhere)
	FVector2D OffsetMin = FVector2D(30.0, -42.0);

	UPROPERTY(EditAnywhere)
	FVector2D OffsetMax = FVector2D(70.0, -19.0);

	UPROPERTY(EditAnywhere)
	float DriftMin = -31.f;

	UPROPERTY(EditAnywhere)
	float DriftMax = 30.f;

	FArcDamageNumbersStructModel()
	{
		bTickForLocalOnly = true;
	}

	virtual void CreateViewModels(UArcFrontendMVVMModelComponent& Component) override;
	virtual void Initialize(UArcFrontendMVVMModelComponent& Component) override;
	virtual void Tick(float DeltaTime, UArcFrontendMVVMModelComponent& Component) override;
	virtual void Deinitialize(UArcFrontendMVVMModelComponent& Component) override;

private:
	void HandleHealthChanged(const FArcGameEvent& Event, UWorld& World);
	void PushToViewModel() const;

	TWeakObjectPtr<UArcDamageNumbersViewModel> DamageNumbersVM;
	TWeakObjectPtr<UArcGameEventSubsystem> BoundEventSubsystem;
	FDelegateHandle HealthChangedHandle;
	TArray<FArcDamageNumberEntry> Entries;
	TArray<double> ExpiryTimes;
	int32 NextId = 0;
};
