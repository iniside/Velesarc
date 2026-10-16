// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcWeatherSubsystem.h"
#include "Mass/Processor.h"
#include "MassEntityTraitBase.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "ArcIceWeatherProcessor.generated.h"

class UArcEffectDefinition;

USTRUCT()
struct ARCWEATHER_API FArcIceEffectsSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UArcEffectDefinition> DamageEffect = nullptr;

	UPROPERTY()
	TObjectPtr<UArcEffectDefinition> HealEffect = nullptr;
};

UCLASS(meta = (DisplayName = "Arc Ice Weather Processor"))
class ARCWEATHER_API UArcIceWeatherProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcIceWeatherProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Ice", Category = "Weather"))
class ARCWEATHER_API UArcIceTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FArcIceFragment IceConfig;

	UPROPERTY(EditAnywhere, meta = (ToolTip = "Applied when the ice melts, with the melt amount as ArcMech.SBC.TrueDamage."))
	TObjectPtr<UArcEffectDefinition> DamageEffect = nullptr;

	UPROPERTY(EditAnywhere, meta = (ToolTip = "Applied when the ice freezes, with the freeze amount as ArcMech.SBC.TrueHeal."))
	TObjectPtr<UArcEffectDefinition> HealEffect = nullptr;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
