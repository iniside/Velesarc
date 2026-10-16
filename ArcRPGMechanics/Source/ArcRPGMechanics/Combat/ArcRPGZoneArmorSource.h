// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcExtraCapture.h"
#include "ArcRPGZoneArmorSource.generated.h"

USTRUCT(BlueprintType, meta = (Hidden, ToolTip = "Defender item stat of the struck zone's piece (D-78 b, D-89): among the defender's items that sit in a slot, are active and carry the struck zone tag (a child of ArcMech.Hit.Zone in the target tags) in their Item Tags, the highest Stat value for the hit's damage type. Pieces never sum. With no struck zone in the target tags (periodic tick) the value is 0."))
struct ARCRPGMECHANICS_API FArcRPGExtraCaptureSource_ZoneItemStat : public FArcExtraCaptureSource
{
	GENERATED_BODY()

	FArcRPGExtraCaptureSource_ZoneItemStat();

	UPROPERTY(EditAnywhere, Category = "Zone", meta = (ToolTip = "Item Stats key read from the struck piece."))
	FArcAttributeRef Stat;

	UPROPERTY(EditAnywhere, Category = "Zone", meta = (MetaStruct = "/Script/ArcMassItemsRuntime.ArcMassItemStoreFragment", ToolTip = "Item store that holds the defender's slotted items."))
	TObjectPtr<UScriptStruct> StoreType = nullptr;

	virtual bool ResolveValue(const FArcEffectContext& Context, FMassEntityManager& EntityManager, const FArcEvaluateParameters& EvalParams,
		float& OutValue) const override;

	virtual EArcCaptureSource GetCaptureSide() const override;
};

USTRUCT(BlueprintType, meta = (Hidden, DisplayName = "RPG Zone Armor", ToolTip = "Armor of the struck zone's piece for the hit's damage type, read by the RPG Damage Execution on applied hits. Magical types ignore armor (D-17) because armor entries are tagged ArcEffect.Damage.Physical (or a physical child); an untagged armor entry applies to every type."))
struct ARCRPGMECHANICS_API FArcRPGExtraCaptureSource_ZoneArmor : public FArcRPGExtraCaptureSource_ZoneItemStat
{
	GENERATED_BODY()

	FArcRPGExtraCaptureSource_ZoneArmor();
};

USTRUCT(BlueprintType, meta = (Hidden, DisplayName = "RPG Zone Resistance", ToolTip = "Intrinsic resistance of the struck zone's piece for the hit's damage type, read by the RPG Damage Execution on applied hits. Global resistance affixes stay modifiers on the defender's Resistance attribute (D-78 c)."))
struct ARCRPGMECHANICS_API FArcRPGExtraCaptureSource_ZoneResistance : public FArcRPGExtraCaptureSource_ZoneItemStat
{
	GENERATED_BODY()

	FArcRPGExtraCaptureSource_ZoneResistance();
};
