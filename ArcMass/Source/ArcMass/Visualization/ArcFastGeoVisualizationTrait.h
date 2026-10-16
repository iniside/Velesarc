// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcMassVisRenderingProps.h"
#include "ArcFastGeoVisualizationTrait.generated.h"

class UStaticMesh;
class UMaterialInterface;

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc FastGeo Visualization", Category = "Visualization"))
class ARCMASS_API UArcFastGeoVisualizationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UStaticMesh* GetMesh() const { return Mesh; }
	const FTransform& GetComponentTransform() const { return ComponentTransform; }
	const TArray<TObjectPtr<UMaterialInterface>>& GetMaterialOverrides() const { return MaterialOverrides; }
	bool IsValid() const { return Mesh != nullptr; }

	bool GetCastShadow() const { return RenderingProps.bCastShadow; }
	bool GetCastShadowAsTwoSided() const { return RenderingProps.bCastShadowAsTwoSided; }
	bool GetCastHiddenShadow() const { return RenderingProps.bCastHiddenShadow; }
	bool GetCastContactShadow() const { return RenderingProps.bCastContactShadow; }
	bool GetReceivesDecals() const { return RenderingProps.bReceivesDecals; }
	bool GetRenderCustomDepth() const { return RenderingProps.bRenderCustomDepth; }
	int64 GetCustomDepthStencilValue() const { return RenderingProps.CustomDepthStencilValue; }
	int32 GetTranslucencySortPriority() const { return RenderingProps.TranslucencySortPriority; }

#if WITH_EDITOR
	void SetComponentTransform(const FTransform& InTransform) { ComponentTransform = InTransform; }
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "FastGeo")
	FName FastGeoGridName;
#endif

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FTransform ComponentTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (ShowOnlyInnerProperties))
	FArcMassVisRenderingProps RenderingProps;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;
};
