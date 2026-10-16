// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Svg/ArcSvgImage.h"
#include "ArcNumericStepper.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcNumericStepperValueChangedDynamic, int32, NewValue);

/**
 * A generic -/value/+ integer stepper composite.
 *
 * Decrements/increments a clamped integer Value between Min and Max in StepSize
 * increments, driven by two buttons, and displays the current value in a text block.
 * Buttons disable at their respective bounds; optional SVG icons dim to
 * DisabledIconOpacity when their edge is reached.
 *
 * Any skin reparented to this class MUST provide exactly-named child widgets:
 *   - Button_Decrease  (required BindWidget)
 *   - Button_Increase  (required BindWidget)
 *   - TextBlock_Value  (required BindWidget)
 * The icons Icon_Decrease and Icon_Increase are optional (BindWidgetOptional).
 */
UCLASS(MinimalAPI, meta = (ShortTooltip = "A generic -/value/+ integer stepper composite"))
class UArcNumericStepper : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Sets the value (clamped to [Min, Max]); refreshes visuals and broadcasts OnValueChanged when it changes. */
	UFUNCTION(BlueprintCallable, Category = "Numeric Stepper")
	ARCMARKUPUI_API void SetValue(int32 InValue);

	/** Current stepper value. BlueprintPure so it inlines into a BP argument pin. */
	UFUNCTION(BlueprintPure, Category = "Numeric Stepper")
	ARCMARKUPUI_API int32 GetValue() const { return Value; }

	ARCMARKUPUI_API int32 GetMin() const { return Min; }

	/** Sets the lower bound; re-clamps Value and refreshes button/icon state. */
	ARCMARKUPUI_API void SetMin(int32 InMin);

	ARCMARKUPUI_API int32 GetMax() const { return Max; }

	/** Sets the upper bound; re-clamps Value and refreshes button/icon state. */
	ARCMARKUPUI_API void SetMax(int32 InMax);

	/** Fires whenever Value changes, passing the new value. */
	UPROPERTY(BlueprintAssignable, Category = "Numeric Stepper|Event")
	FArcNumericStepperValueChangedDynamic OnValueChanged;

protected:
	//~ Begin UUserWidget interface
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	//~ End UUserWidget interface

	UFUNCTION()
	void HandleDecreaseClicked();

	UFUNCTION()
	void HandleIncreaseClicked();

	/** Syncs text, button-enabled state, and icon tints to the current Value. */
	void RefreshVisualState();

	/** Decrement button; required in any skin. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Decrease;

	/** Increment button; required in any skin. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Increase;

	/** Displays the current value; required in any skin. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TextBlock_Value;

	/** Optional decrement icon; dims at the lower bound. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UArcSvgImage> Icon_Decrease;

	/** Optional increment icon; dims at the upper bound. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UArcSvgImage> Icon_Increase;

	/** Minimum allowed value. Runtime writes route through SetMin so Value re-clamps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Numeric Stepper", meta = (AllowPrivateAccess = "true"))
	int32 Min = 1;

	/** Maximum allowed value. Runtime writes route through SetMax so Value re-clamps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Numeric Stepper", meta = (AllowPrivateAccess = "true"))
	int32 Max = 99;

	/** Amount added/removed per button press. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Numeric Stepper", meta = (AllowPrivateAccess = "true"))
	int32 StepSize = 1;

	/** Current value; clamped to [Min, Max]. Runtime writes route through SetValue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Numeric Stepper", meta = (AllowPrivateAccess = "true"))
	int32 Value = 1;

	/** Icon tint when the corresponding button is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Numeric Stepper", meta = (AllowPrivateAccess = "true"))
	FLinearColor EnabledIconTint = FLinearColor::White;

	/** Icon tint/opacity when the corresponding button is disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Numeric Stepper", meta = (AllowPrivateAccess = "true"))
	FLinearColor DisabledIconOpacity = FLinearColor(1.f, 1.f, 1.f, 0.35f);
};
