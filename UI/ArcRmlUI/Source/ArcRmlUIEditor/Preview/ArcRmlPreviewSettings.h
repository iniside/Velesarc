// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"

#include "ArcRmlPreviewSettings.generated.h"

class UTexture2D;

UENUM()
enum class EArcRmlPreviewBackgroundMode : uint8
{
	SolidColor,
	Gradient,
	Texture
};

UENUM()
enum class EArcRmlPreviewGradientDirection : uint8
{
	Vertical,
	Horizontal
};

UCLASS(config = EditorPerProjectUserSettings)
class UArcRmlPreviewSettings final : public UObject
{
	GENERATED_BODY()

public:
	static UArcRmlPreviewSettings& Get();

	EArcRmlPreviewBackgroundMode GetMode() const;
	const FLinearColor& GetSolidColor() const;
	const FLinearColor& GetGradientStartColor() const;
	const FLinearColor& GetGradientEndColor() const;
	EArcRmlPreviewGradientDirection GetGradientDirection() const;
	const TSoftObjectPtr<UTexture2D>& GetTexture() const;
	UTexture2D* GetLoadedTexture();

	void SetMode(EArcRmlPreviewBackgroundMode InMode);
	void SetSolidColor(const FLinearColor& InColor);
	void SetGradientStartColor(const FLinearColor& InColor);
	void SetGradientEndColor(const FLinearColor& InColor);
	void SetGradientDirection(EArcRmlPreviewGradientDirection InDirection);
	void SetTexture(const TSoftObjectPtr<UTexture2D>& InTexture);

	FSimpleMulticastDelegate& OnChanged();

private:
	void CommitChange();
	void ResolveTexture();

	UPROPERTY(config)
	EArcRmlPreviewBackgroundMode Mode = EArcRmlPreviewBackgroundMode::SolidColor;

	UPROPERTY(config)
	FLinearColor SolidColor = FLinearColor::Black;

	UPROPERTY(config)
	FLinearColor GradientStartColor = FLinearColor(0.1f, 0.1f, 0.1f);

	UPROPERTY(config)
	FLinearColor GradientEndColor = FLinearColor::Black;

	UPROPERTY(config)
	EArcRmlPreviewGradientDirection GradientDirection = EArcRmlPreviewGradientDirection::Vertical;

	UPROPERTY(config)
	TSoftObjectPtr<UTexture2D> Texture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedTexture;

	FSoftObjectPath ResolvedTexturePath;
	bool bTextureResolved = false;
	FSimpleMulticastDelegate ChangedDelegate;
};
