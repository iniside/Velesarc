// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ContentWidget.h"
#include "Layout/Margin.h"
#include "Styling/SlateBrush.h"
#include "UBoxShadow.generated.h"

class SImage;
class SOverlay;
class UTexture2D;

/**
 * A UContentWidget that renders a CSS-like drop shadow behind its single child.
 *
 * The widget builds an SOverlay with two layers: a shadow layer (drawn first /
 * behind) and the content layer (the inherited single child, drawn on top).
 *
 * The shadow is a 9-slice FSlateBrush (FAppStyle "BoxShadow" by default, soft
 * alpha baked in) tinted by ShadowColor, translated by Offset (render
 * transform) and grown beyond the content box by Spread (negative slot
 * padding).  An optional ShadowTexture override builds a Box brush from a
 * supplied texture using ShadowTextureMargin for its 9-slice corners.
 *
 * If the resolved shadow brush is null/invalid (e.g. stripped Slate content),
 * the shadow layer is skipped entirely and only the content is rendered.
 *
 * * Single Child
 * * CSS-like drop shadow (offset / spread / color / optional texture)
 */
UCLASS(MinimalAPI, meta = (ShortTooltip = "Renders a CSS-like drop shadow behind its child"))
class UBoxShadow : public UContentWidget
{
	GENERATED_BODY()

public:
	ARCMARKUPUI_API UBoxShadow();

	// --- Getters ---

	ARCMARKUPUI_API FVector2D GetOffset() const;
	ARCMARKUPUI_API float GetSpread() const;
	ARCMARKUPUI_API FLinearColor GetShadowColor() const;
	ARCMARKUPUI_API UTexture2D* GetShadowTexture() const;
	ARCMARKUPUI_API FMargin GetShadowTextureMargin() const;

	// --- Setters ---

	UFUNCTION(BlueprintCallable, Category = "Box Shadow")
	ARCMARKUPUI_API void SetOffset(FVector2D InOffset);

	UFUNCTION(BlueprintCallable, Category = "Box Shadow")
	ARCMARKUPUI_API void SetSpread(float InSpread);

	UFUNCTION(BlueprintCallable, Category = "Box Shadow")
	ARCMARKUPUI_API void SetShadowColor(FLinearColor InShadowColor);

	UFUNCTION(BlueprintCallable, Category = "Box Shadow")
	ARCMARKUPUI_API void SetShadowTexture(UTexture2D* InShadowTexture);

	UFUNCTION(BlueprintCallable, Category = "Box Shadow")
	ARCMARKUPUI_API void SetShadowTextureMargin(FMargin InShadowTextureMargin);

	//~ Begin UWidget interface
	ARCMARKUPUI_API virtual void SynchronizeProperties() override;
	ARCMARKUPUI_API virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UWidget interface

#if WITH_EDITOR
	ARCMARKUPUI_API virtual const FText GetPaletteCategory() override;
#endif

protected:
	//~ Begin UPanelWidget interface
	ARCMARKUPUI_API virtual UClass* GetSlotClass() const override;
	ARCMARKUPUI_API virtual void OnSlotAdded(UPanelSlot* InSlot) override;
	ARCMARKUPUI_API virtual void OnSlotRemoved(UPanelSlot* InSlot) override;
	//~ End UPanelWidget interface

	//~ Begin UWidget interface
	ARCMARKUPUI_API virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget interface

private:
	/** Pixel offset of the shadow relative to the content box. X = right, Y = down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Box Shadow", meta = (AllowPrivateAccess = "true"))
	FVector2D Offset = FVector2D(0.f, 2.f);

	/** How far (in Slate Units) the shadow extends beyond the content box on every side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Box Shadow", meta = (AllowPrivateAccess = "true"))
	float Spread = 4.f;

	/** Tint applied to the shadow brush. Alpha controls overall shadow strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Box Shadow", meta = (AllowPrivateAccess = "true"))
	FLinearColor ShadowColor = FLinearColor(0.f, 0.f, 0.f, 0.35f);

	/** Optional texture used for the shadow. When set, overrides the default FAppStyle BoxShadow brush. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Box Shadow", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> ShadowTexture = nullptr;

	/** 9-slice corner margin (normalized 0..1) used when ShadowTexture is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Box Shadow", meta = (AllowPrivateAccess = "true"))
	FMargin ShadowTextureMargin = FMargin(0.5f);

	/** Rebuilds ShadowBrush from the default style or ShadowTexture override. Returns true if a valid brush is available. */
	bool RebuildShadowBrush();

	/** Creates and inserts the shadow layer into MyOverlay if a valid brush exists and the layer is missing (e.g. the brush only became available after RebuildWidget). */
	void EnsureShadowLayer();

	/** Pushes Offset (render transform), Spread (padding) and ShadowColor (tint) onto the live shadow widget/slot. */
	void UpdateShadowLayout();

	/** Root overlay holding the shadow + content layers. */
	TSharedPtr<SOverlay> MyOverlay;

	/** The shadow image (bottom layer); null when no valid shadow brush exists. */
	TSharedPtr<SImage> MyShadowImage;

	/** The shadow's overlay slot; used to push spread as negative padding. */
	SOverlay::FOverlaySlot* MyShadowSlot = nullptr;

	/** Backing brush for the shadow SImage (SImage::SetImage takes a raw pointer, so this must outlive the widget). */
	TSharedPtr<FSlateBrush> ShadowBrush;

	/** The texture the current ShadowBrush was built from, to detect override changes. */
	TWeakObjectPtr<UTexture2D> CachedShadowTexture;
};
