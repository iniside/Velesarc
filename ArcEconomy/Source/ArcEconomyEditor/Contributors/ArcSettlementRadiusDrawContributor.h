#pragma once

#include "CoreMinimal.h"
#include "EditorVisualization/ArcEditorEntityDrawContributor.h"

class FArcSettlementRadiusDrawContributor : public IArcEditorEntityDrawContributor
{
public:
	virtual FName GetCategory() const override { return TEXT("Settlement Radius"); }
	virtual TSubclassOf<UMassEntityTraitBase> GetTraitClass() const override;
	virtual void CollectShapes(
		UArcEditorEntityDrawComponent& DrawComponent,
		const UMassEntityTraitBase* Trait,
		TConstArrayView<FTransform> Transforms,
		const FArcEditorDrawContext& Context) override;
};
