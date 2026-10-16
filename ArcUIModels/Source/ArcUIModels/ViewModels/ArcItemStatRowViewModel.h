// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"

#include "ArcItemStatRowViewModel.generated.h"

struct FArcItemAttributeStat;
class UArcStatDisplayRegistry;

/**
 * Child view model for one item-stat row (Label + Value). Not registered in the global MVVM
 * collection — created and owned by UArcItemViewModel, like UArcIngredientCandidateViewModel.
 */
UCLASS(BlueprintType)
class ARCUIMODELS_API UArcItemStatRowViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText Label;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText Value;

	const FText& GetLabel() const { return Label; }
	const FText& GetValue() const { return Value; }

	/**
	 * Populates Label + Value from a single design-time stat entry.
	 * Label = Reg->FindDisplay(Stat.Stat)->DisplayName, or a humanized PropertyName on miss.
	 * Value = Stat.Value.GetValue() (context-free scalable-float eval) + the registry suffix.
	 * Reg may be null (label falls back to humanized PropertyName, no suffix).
	 */
	void RefreshFrom(const FArcItemAttributeStat& Stat, const UArcStatDisplayRegistry* Reg);
};
