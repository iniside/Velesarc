// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcFrontendMVVMModelComponent.h"
#include "Attributes/ArcAttribute.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "ArcFloatAttributeStructModel.generated.h"

class UArcFloatViewModel;
class UArcGameEventSubsystem;

/**
 * Generic MVVM struct-model that mirrors a Mass attribute pair (current + optional
 * min/max) on the owning player's entity into a single UArcFloatViewModel. Not
 * health-specific: point ValueAttribute/MinAttribute/MaxAttribute at any attributes
 * (health, mana, stamina, XP, …) and set ContextName to the VM context to drive.
 */
USTRUCT(BlueprintType)
struct ARCUIMODELS_API FArcFloatAttributeStructModel : public FArcFrontendMVVMStructModel
{
	GENERATED_BODY()

	/** Context name used to register and look up the UArcFloatViewModel. */
	UPROPERTY(EditAnywhere)
	FName ContextName = TEXT("Attribute");

	UPROPERTY(EditAnywhere)
	FGameplayTag RegistrationTag;

	/** Attribute ref to mirror into the VM's Value. */
	UPROPERTY(EditAnywhere)
	FArcAttributeRef ValueAttribute;

	/** Optional attribute ref to mirror into the VM's MinValue. Ignored if invalid. */
	UPROPERTY(EditAnywhere)
	FArcAttributeRef MinAttribute;

	/** Optional attribute ref to mirror into the VM's MaxValue. Ignored if invalid. */
	UPROPERTY(EditAnywhere)
	FArcAttributeRef MaxAttribute;

	UPROPERTY(EditAnywhere)
	FArcAttributeRef RegenPipsAttribute;

	UPROPERTY(EditAnywhere)
	FArcAttributeRef DegenPipsAttribute;

	/** Static label seeded onto the VM during Initialize. Skipped if empty. */
	UPROPERTY(EditAnywhere)
	FText Label;

	virtual void CreateViewModels(UArcFrontendMVVMModelComponent& Component) override;
	virtual void Initialize(UArcFrontendMVVMModelComponent& Component) override;
	virtual void Deinitialize(UArcFrontendMVVMModelComponent& Component) override;

private:
	enum class EArcFloatField : uint8 { Value, Min, Max, RegenPips, DegenPips };

	void ApplyToViewModel(UArcFrontendMVVMModelComponent& Component, EArcFloatField Field, float NewValue);

	TWeakObjectPtr<UArcFloatViewModel> FloatVM;
	TWeakObjectPtr<UArcGameEventSubsystem> BoundEventSubsystem;
	FDelegateHandle ValueHandle;
	FDelegateHandle MinHandle;
	FDelegateHandle MaxHandle;
	FDelegateHandle RegenPipsHandle;
	FDelegateHandle DegenPipsHandle;
	float RegenPips = 0.f;
	float DegenPips = 0.f;
};
