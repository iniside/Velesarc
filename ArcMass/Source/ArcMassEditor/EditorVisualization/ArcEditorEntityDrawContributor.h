#pragma once

#include "CoreMinimal.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityTraitBase.h"

struct FArcEditorDrawCollector;

enum class EArcDrawCategoryKind : uint8
{
	Detail,
	// Switching this off would leave the entity with no visual at all.
	Representation,
};

struct FArcEditorDrawContext
{
	UWorld* World = nullptr;

	struct FPlacedEntityInfo
	{
		const UMassEntityConfigAsset* Config = nullptr;
		TArray<FTransform> Transforms;
	};

	TArray<FPlacedEntityInfo> AllPlacedEntities;
};

class ARCMASSEDITOR_API IArcEditorEntityDrawContributor
{
public:
	virtual ~IArcEditorEntityDrawContributor() = default;

	virtual FName GetCategory() const = 0;

	virtual TSubclassOf<UMassEntityTraitBase> GetTraitClass() const = 0;

	virtual EArcDrawCategoryKind GetCategoryKind() const = 0;

	virtual void CollectShapes(
		FArcEditorDrawCollector& Collector,
		const UMassEntityTraitBase* Trait,
		TConstArrayView<FTransform> Transforms,
		const FArcEditorDrawContext& Context) = 0;
};
