// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorVisualization/ArcEditorEntityDrawContributor.h"

class UArcRecipeDefinition;

class FArcBuildingProductionDrawContributor : public IArcEditorEntityDrawContributor
{
public:
	virtual FName GetCategory() const override { return TEXT("Production Info"); }
	virtual TSubclassOf<UMassEntityTraitBase> GetTraitClass() const override;
	virtual void CollectShapes(
		UArcEditorEntityDrawComponent& DrawComponent,
		const UMassEntityTraitBase* Trait,
		TConstArrayView<FTransform> Transforms,
		const FArcEditorDrawContext& Context) override;

private:
	FString FormatRecipeInfo(const UArcRecipeDefinition* Recipe) const;
};
