// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcMassEntityVisualization.h"
#include "PhysicsEngine/BodyInstance.h"
#include "ArcMassEntityVisualizationTrait.generated.h"

class UBodySetup;

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Entity Visualization", Category = "Visualization"))
class ARCMASS_API UArcEntityVisualizationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	FArcVisConfigFragment VisualizationConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Re-extract mesh, collision, and materials from the ActorClass exemplar. */
	UFUNCTION(CallInEditor, Category = "Visualization")
	void ExtractFromActorClass();

	// --- Preview accessors (editor only) ---
	UStaticMesh* GetExtractedMesh() const { return ExtractedMesh; }
	const FTransform& GetExtractedComponentTransform() const { return ExtractedComponentTransform; }
	const TArray<TObjectPtr<UMaterialInterface>>& GetExtractedMaterials() const { return ExtractedMaterials; }
	UBodySetup* GetExtractedBodySetup() const { return ExtractedBodySetup; }
	bool IsExtractionValid() const { return bExtractionValid; }
#endif

protected:
	// --- Extracted from ActorClass at editor time ---

	UPROPERTY(VisibleAnywhere, Category = "Extracted")
	TObjectPtr<UStaticMesh> ExtractedMesh = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Extracted")
	FTransform ExtractedComponentTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, Category = "Extracted")
	bool ExtractedCastShadow = false;

	UPROPERTY(VisibleAnywhere, Category = "Extracted")
	TArray<TObjectPtr<UMaterialInterface>> ExtractedMaterials;

	UPROPERTY(VisibleAnywhere, Category = "Extracted")
	TObjectPtr<UBodySetup> ExtractedBodySetup = nullptr;

	/** Full physics body template extracted from root primitive component. */
	UPROPERTY(VisibleAnywhere, Category = "Extracted", meta=(ShowOnlyInnerProperties, SkipUCSModifiedProperties))
	FBodyInstance ExtractedBodyTemplate;
	
	UPROPERTY(VisibleAnywhere, Category = "Extracted")
	bool bExtractionValid = false;
};
