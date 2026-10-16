// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "GameplayTagContainer.h"
#include "ArcAbilityViewModel.generated.h"

/** Internal transport POD — produced by the Step-4 resolver, consumed by UArcAbilityViewModel. */
struct FArcAbilitySnapshot
{
	bool bHasAbility = false;
	bool bIsActive = false;
	bool bCanActivate = false;
	float CooldownRemaining = 0.f;
	float CooldownDuration = 0.f;
	float CooldownPercent = 0.f;
	FGameplayTag InputTag;
	FGameplayTagContainer AbilityTags;
};

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcAbilityViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bHasAbility = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bIsActive = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bCanActivate = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	float CooldownRemaining = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	float CooldownDuration = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	float CooldownPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FGameplayTag InputTag;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FGameplayTagContainer AbilityTags;

	UFUNCTION(BlueprintCallable, Category = "Arc UI")
	void SetHasAbility(bool InHasAbility);

	UFUNCTION(BlueprintCallable, Category = "Arc UI")
	void SetIsActive(bool InIsActive);

	UFUNCTION(BlueprintCallable, Category = "Arc UI")
	void SetCanActivate(bool InCanActivate);

	UFUNCTION(BlueprintCallable, Category = "Arc UI")
	void SetCooldownRemaining(float InCooldownRemaining);

	UFUNCTION(BlueprintCallable, Category = "Arc UI")
	void SetCooldownDuration(float InCooldownDuration);

	UFUNCTION(BlueprintCallable, Category = "Arc UI")
	void SetCooldownPercent(float InCooldownPercent);

	UFUNCTION(BlueprintCallable, Category = "Arc UI")
	void SetInputTag(FGameplayTag InInputTag);

	UFUNCTION(BlueprintCallable, Category = "Arc UI")
	void SetAbilityTags(FGameplayTagContainer InAbilityTags);

	bool GetbHasAbility() const { return bHasAbility; }
	bool GetbIsActive() const { return bIsActive; }
	bool GetbCanActivate() const { return bCanActivate; }
	float GetCooldownRemaining() const { return CooldownRemaining; }
	float GetCooldownDuration() const { return CooldownDuration; }
	float GetCooldownPercent() const { return CooldownPercent; }
	const FGameplayTag& GetInputTag() const { return InputTag; }
	const FGameplayTagContainer& GetAbilityTags() const { return AbilityTags; }

	void ApplySnapshot(const FArcAbilitySnapshot& Snapshot);
};
