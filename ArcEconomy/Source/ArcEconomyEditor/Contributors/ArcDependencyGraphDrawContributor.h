// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorVisualization/ArcEditorEntityDrawContributor.h"

class UArcItemDefinition;

class FArcDependencyGraphDrawContributor : public IArcEditorEntityDrawContributor
{
public:
	virtual FName GetCategory() const override { return TEXT("Dependency Graph"); }
	virtual TSubclassOf<UMassEntityTraitBase> GetTraitClass() const override { return nullptr; }
	virtual void CollectShapes(
		UArcEditorEntityDrawComponent& DrawComponent,
		const UMassEntityTraitBase* Trait,
		TConstArrayView<FTransform> Transforms,
		const FArcEditorDrawContext& Context) override;

private:
	struct FBuildingInstance
	{
		FVector Location = FVector::ZeroVector;
		FString BuildingName;
		TArray<FPrimaryAssetId> ProducedItems;
		TArray<FPrimaryAssetId> ConsumedItems;
		int32 SettlementIndex = INDEX_NONE;
	};

	struct FSettlementInstance
	{
		FVector Location = FVector::ZeroVector;
		float Radius = 0.f;
	};
};
