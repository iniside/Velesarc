// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Image.h"
#include "ArcSvgImage.generated.h"

class UArcSvgAsset;
class SScaleBox;

/**
 * UImage subclass that renders a UArcSvgAsset as a runtime-scalable vector icon.
 *
 * The asset extracts its SVG bytes to a content-hashed file on disk, and this widget
 * feeds that path to a stock FSlateVectorImageBrush so Slate's vector rasterizer draws
 * the icon crisply at any DPI / on-screen size. The brush is rebuilt in
 * SynchronizeProperties() (and from the setters) so designer/Blueprint edits show live.
 */
UCLASS()
class ARCMARKUPUI_API UArcSvgImage : public UImage
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "SVG Icon")
	void SetSvgAsset(UArcSvgAsset* InSvgAsset);
	UArcSvgAsset* GetSvgAsset() const;

	UFUNCTION(BlueprintCallable, Category = "SVG Icon")
	void SetIconSize(FVector2D InIconSize);
	FVector2D GetIconSize() const;

	UFUNCTION(BlueprintCallable, Category = "SVG Icon")
	void SetOverrideDrawSize(bool bInOverrideDrawSize);
	bool GetOverrideDrawSize() const;

	//~ Begin UWidget interface
	virtual void SynchronizeProperties() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UWidget interface

private:
	/** Rebuilds the vector brush from the current asset + effective size and assigns it. */
	void RebuildSvgBrush();

	/** SVG asset to render. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SVG Icon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArcSvgAsset> SvgAsset = nullptr;

	/** When true, IconSize overrides the asset's DefaultDrawSize for this instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter = "SetOverrideDrawSize", Getter = "GetOverrideDrawSize", Category = "SVG Icon", meta = (AllowPrivateAccess = "true"))
	bool bOverrideDrawSize = true;

	/** Per-instance draw size used when bOverrideDrawSize is true; raster resolution still follows actual on-screen geometry x DPI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG Icon", meta = (AllowPrivateAccess = "true"))
	FVector2D IconSize = FVector2D(64.f, 64.f);

	/** Wraps the inner SImage so PreserveAspectRatio can drive the stretch mode live. */
	TSharedPtr<class SScaleBox> MyScaleBox;
};
