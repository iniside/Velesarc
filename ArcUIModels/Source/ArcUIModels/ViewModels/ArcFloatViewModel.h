// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ArcFloatViewModel.generated.h"

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcFloatViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	float Value = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	float MinValue = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	float MaxValue = 1.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	float NormalizedValue = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText Label;

	UFUNCTION(BlueprintCallable, Category = "Arc UI")
	void SetValue(float InValue);

	UFUNCTION(BlueprintCallable, Category = "Arc UI")
	void SetMinValue(float InMinValue);

	UFUNCTION(BlueprintCallable, Category = "Arc UI")
	void SetMaxValue(float InMaxValue);

	UFUNCTION(BlueprintCallable, Category = "Arc UI")
	void SetLabel(FText InLabel);

	float GetValue() const { return Value; }
	float GetMinValue() const { return MinValue; }
	float GetMaxValue() const { return MaxValue; }
	float GetNormalizedValue() const { return NormalizedValue; }
	const FText& GetLabel() const { return Label; }

private:
	void RecalculateNormalized();
};
