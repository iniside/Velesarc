// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcCompositeMeshVisualizationTrait.generated.h"

class UStaticMesh;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct ARCMASS_API FArcCompositeMeshEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

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
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Composite Mesh Visualization", Category = "Visualization"))
class ARCMASS_API UArcCompositeMeshVisualizationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

#if WITH_EDITOR
	const TArray<FArcCompositeMeshEntry>& GetMeshEntries() const { return MeshEntries; }
	bool IsValid() const;
	void SetMeshEntryTransform(int32 Index, const FTransform& InTransform);
#endif

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TArray<FArcCompositeMeshEntry> MeshEntries;
};
