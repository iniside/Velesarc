// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Items/Fragments/ArcItemFragment.h"

#include "ArcItemFragment_Icon.generated.h"

USTRUCT(BlueprintType,
	meta = (DisplayName = "Icon", Category = "UI",
	        ToolTip = "Icon-only UI fragment. Use when UI needs the icon without pulling in the full UIDescription text payload."))
struct ARCMASSITEMSRUNTIME_API FArcItemFragment_Icon : public FArcItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere
		, BlueprintReadWrite
		, Category = UI
		, meta = (AllowPrivateAccess = "true", DisplayThumbnail = "true", DisplayName = "Image", AllowedClasses =
			"/Script/Engine.Texture,/Script/Engine.MaterialInterface,/Script/Engine.SlateTextureAtlasInterface",
			DisallowedClasses = "/Script/MediaAssets.MediaTexture"))
	TSoftObjectPtr<UObject> ItemIcon;

	virtual ~FArcItemFragment_Icon() override = default;
};
