// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "ArcMassEquipmentTrait.generated.h"

class UArcMassEquipmentSlotPreset;
class UArcMassEquipmentSlotDefaultItems;

// Adds the FArcMassEquipmentFragment sparse fragment to an entity, plus a const shared
// fragment carrying the slot preset and (optional) default items asset. The init observer
// applies any default items into the matching equipment slots after the sparse fragment
// has been attached.
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Equipment", Category = "Items"))
class ARCMASSITEMSRUNTIME_API UArcMassEquipmentTrait : public UArcMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Equipment")
	TObjectPtr<UArcMassEquipmentSlotPreset> SlotPreset = nullptr;

	UPROPERTY(EditAnywhere, Category = "Equipment")
	TObjectPtr<UArcMassEquipmentSlotDefaultItems> DefaultItems = nullptr;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual void BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const override;
	virtual void InitializeEntity(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UWorld& World) const override;
};
