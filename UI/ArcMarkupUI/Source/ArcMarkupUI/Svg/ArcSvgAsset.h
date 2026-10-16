// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ArcSvgAsset.generated.h"

class UAssetImportData;

/**
 * Runtime asset wrapping raw UTF-8 SVG bytes.
 *
 * The bytes are stored as a non-transient UPROPERTY so they persist in (and cook into)
 * the .uasset directly. Slate's SVG path (FSlateVectorImageBrush / FSlateSVGRasterizer)
 * rasterizes from a file on disk, so this asset extracts its bytes to a content-hashed
 * file under ProjectSavedDir()/SVGCache on load. The hash-in-filename is mandatory:
 * Slate's vector graphics cache keys only on {BrushName, PixelSize} with no content or
 * mtime check, so reusing a path after the content changes would serve a stale raster.
 */
UCLASS(BlueprintType)
class ARCMARKUPUI_API UArcSvgAsset : public UObject
{
	GENERATED_BODY()

public:
	/** Raw UTF-8 SVG bytes. Persists and cooks into the .uasset (not Transient). */
	UPROPERTY()
	TArray<uint8> SvgData;

	/** Tint multiplied into the rendered icon (recolors monochrome SVGs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	FLinearColor Tint = FLinearColor::White;

	/** Default draw size the widget uses unless it overrides per-instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	FVector2D DefaultDrawSize = FVector2D(64.f, 64.f);

	/** Preserve the icon's aspect ratio when drawn at a non-matching size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	bool bPreserveAspectRatio = false;

#if WITH_EDITORONLY_DATA
	/** Import metadata (source file path, hashes) for reimport support. */
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Import")
	TObjectPtr<UAssetImportData> AssetImportData;
#endif

	//~ Begin UObject interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	//~ End UObject interface

	/**
	 * Returns the path to the extracted SVG file on disk, extracting it on first call.
	 * The filename embeds an MD5 hash of the bytes, so a content change yields a fresh
	 * path. Returns an empty string if there are no bytes or the write fails.
	 */
	const FString& GetOrExtractSvgFilePath();

	/** Drops the memoized extracted path so the next extraction recomputes the hash/path. */
	void ClearExtractedCache();

private:
	/** Session-memoized absolute path to the extracted SVG file. */
	UPROPERTY(Transient)
	FString CachedExtractedPath;
};
