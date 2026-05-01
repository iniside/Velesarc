// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcSkinnedMeshEntityVisualizationTrait.generated.h"

class USkinnedAsset;
class UTransformProviderData;
class UMaterialInterface;

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Skinned Mesh Entity Visualization", Category = "Visualization"))
class ARCMASS_API UArcSkinnedMeshEntityVisualizationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

#if WITH_EDITOR
	USkinnedAsset* GetSkinnedAsset() const { return SkinnedAsset; }
	UTransformProviderData* GetTransformProvider() const { return TransformProvider; }
	const FTransform& GetComponentTransform() const { return ComponentTransform; }
	const TArray<TObjectPtr<UMaterialInterface>>& GetMaterialOverrides() const { return MaterialOverrides; }
	bool IsValid() const { return SkinnedAsset != nullptr; }
	void SetComponentTransform(const FTransform& InTransform) { ComponentTransform = InTransform; }
#endif

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TObjectPtr<USkinnedAsset> SkinnedAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TObjectPtr<UTransformProviderData> TransformProvider = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FTransform ComponentTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shadows")
	bool bCastShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shadows")
	bool bCastShadowAsTwoSided = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shadows")
	bool bCastHiddenShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shadows")
	bool bCastFarShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shadows")
	bool bCastInsetShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shadows")
	bool bCastContactShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	bool bHiddenInGame = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	bool bReceivesDecals = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	bool bNeverDistanceCull = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	bool bRenderCustomDepth = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bRenderCustomDepth"))
	int64 CustomDepthStencilValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	int32 TranslucencySortPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	bool bAffectDynamicIndirectLighting = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	bool bAffectIndirectLightingWhileHidden = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;
};
