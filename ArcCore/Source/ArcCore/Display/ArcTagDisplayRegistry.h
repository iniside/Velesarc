// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "ArcTagDisplayRegistry.generated.h"

USTRUCT(BlueprintType)
struct ARCCORE_API FArcTagDisplayData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		meta = (AllowedClasses = "/Script/Engine.Texture,/Script/Engine.MaterialInterface,/Script/Engine.SlateTextureAtlasInterface",
				DisallowedClasses = "/Script/MediaAssets.MediaTexture", DisplayThumbnail = "true"))
	TSoftObjectPtr<UObject> Icon;
};

UCLASS(BlueprintType)
class ARCCORE_API UArcTagDisplayRegistry : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FGameplayTag, FArcTagDisplayData> Entries;

	/** Exact match on Tag; if not found, walks parent tags via RequestDirectParent() up to the root.
	 *  Returns nullptr if no entry matches. */
	const FArcTagDisplayData* FindDisplay(FGameplayTag Tag) const;
};
