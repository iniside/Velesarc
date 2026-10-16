// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/ArcRmlPreviewSettings.h"

#include "ArcRmlUILog.h"
#include "Engine/Texture2D.h"

UArcRmlPreviewSettings& UArcRmlPreviewSettings::Get()
{
	return *GetMutableDefault<UArcRmlPreviewSettings>();
}

EArcRmlPreviewBackgroundMode UArcRmlPreviewSettings::GetMode() const
{
	return Mode;
}

const FLinearColor& UArcRmlPreviewSettings::GetSolidColor() const
{
	return SolidColor;
}

const FLinearColor& UArcRmlPreviewSettings::GetGradientStartColor() const
{
	return GradientStartColor;
}

const FLinearColor& UArcRmlPreviewSettings::GetGradientEndColor() const
{
	return GradientEndColor;
}

EArcRmlPreviewGradientDirection UArcRmlPreviewSettings::GetGradientDirection() const
{
	return GradientDirection;
}

const TSoftObjectPtr<UTexture2D>& UArcRmlPreviewSettings::GetTexture() const
{
	return Texture;
}

UTexture2D* UArcRmlPreviewSettings::GetLoadedTexture()
{
	ResolveTexture();
	return LoadedTexture;
}

void UArcRmlPreviewSettings::SetMode(EArcRmlPreviewBackgroundMode InMode)
{
	Mode = InMode;
	CommitChange();
}

void UArcRmlPreviewSettings::SetSolidColor(const FLinearColor& InColor)
{
	SolidColor = InColor;
	CommitChange();
}

void UArcRmlPreviewSettings::SetGradientStartColor(const FLinearColor& InColor)
{
	GradientStartColor = InColor;
	CommitChange();
}

void UArcRmlPreviewSettings::SetGradientEndColor(const FLinearColor& InColor)
{
	GradientEndColor = InColor;
	CommitChange();
}

void UArcRmlPreviewSettings::SetGradientDirection(EArcRmlPreviewGradientDirection InDirection)
{
	GradientDirection = InDirection;
	CommitChange();
}

void UArcRmlPreviewSettings::SetTexture(const TSoftObjectPtr<UTexture2D>& InTexture)
{
	Texture = InTexture;
	CommitChange();
}

FSimpleMulticastDelegate& UArcRmlPreviewSettings::OnChanged()
{
	return ChangedDelegate;
}

void UArcRmlPreviewSettings::CommitChange()
{
	SaveConfig();
	ResolveTexture();
	ChangedDelegate.Broadcast();
}

void UArcRmlPreviewSettings::ResolveTexture()
{
	if (Mode != EArcRmlPreviewBackgroundMode::Texture)
	{
		LoadedTexture = nullptr;
		ResolvedTexturePath.Reset();
		bTextureResolved = false;
		return;
	}

	const FSoftObjectPath TexturePath = Texture.ToSoftObjectPath();
	if (bTextureResolved && TexturePath == ResolvedTexturePath)
	{
		return;
	}

	bTextureResolved = true;
	ResolvedTexturePath = TexturePath;
	LoadedTexture = Texture.LoadSynchronous();
	if (!LoadedTexture)
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: preview background texture '%ls' cannot be loaded; the texture background draws nothing",
			*TexturePath.ToString());
	}
}
