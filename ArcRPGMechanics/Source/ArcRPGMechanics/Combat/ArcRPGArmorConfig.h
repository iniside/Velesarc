// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcRPGArmorConfig.generated.h"

struct FArcArmorCurve;

UCLASS(BlueprintType, meta = (ToolTip = "Armor step shared by every RPG Damage Execution that references it: the armor curve applied to the final armor (struck piece + global Armor). An execution without one applies no armor."))
class ARCRPGMECHANICS_API UArcRPGArmorConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UArcRPGArmorConfig();

	UPROPERTY(EditAnywhere, Category = "Armor", meta = (BaseStruct = "/Script/ArcMassMechanics.ArcArmorCurve"))
	FInstancedStruct ArmorCurve;

	UPROPERTY(EditAnywhere, Category = "Armor", meta = (ClampMin = "0.01", ToolTip = "K input of the Armor Curve."))
	float ArmorK = 10.f;

	const FArcArmorCurve* GetArmorCurve() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
