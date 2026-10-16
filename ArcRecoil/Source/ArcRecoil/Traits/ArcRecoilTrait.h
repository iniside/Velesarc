// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "Fragments/ArcRecoilFragments.h"

class UArcRecoilPreset;

#include "ArcRecoilTrait.generated.h"

UCLASS(meta = (DisplayName = "Arc Recoil"))
class ARCRECOIL_API UArcRecoilTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Recoil")
	TObjectPtr<UArcRecoilPreset> Preset;

	UPROPERTY(EditAnywhere, Category = "Recoil")
	FArcRecoilStats DefaultStats;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
