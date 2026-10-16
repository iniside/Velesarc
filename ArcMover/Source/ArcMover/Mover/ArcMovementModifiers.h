// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"

#include "ArcMovementModifiers.generated.h"

class UMoverComponent;

namespace ArcMover
{
	ARCMOVER_API float GetActiveSpeedScale(const UMoverComponent& Mover);

	template<typename TSpeedScaleModifier>
	bool AccumulateSpeedPercents(TArray<TSharedPtr<FMovementModifierBase>>::TConstIterator It, TMap<FGuid, float>& InOutPercentBySource)
	{
		for (; It; ++It)
		{
			const FMovementModifierBase* Modifier = It->Get();
			if (!Modifier || Modifier->GetScriptStruct() != TSpeedScaleModifier::StaticStruct())
			{
				continue;
			}

			const TSpeedScaleModifier* SpeedScale = static_cast<const TSpeedScaleModifier*>(Modifier);
			if (!SpeedScale->SourceId.IsValid())
			{
				continue;
			}
			if (!FMath::IsFinite(SpeedScale->SpeedPercent) || SpeedScale->SpeedPercent < 0.0f)
			{
				return false;
			}
			InOutPercentBySource.Add(SpeedScale->SourceId, SpeedScale->SpeedPercent);
		}
		return true;
	}

	ARCMOVER_API float CombineSpeedPercents(const TMap<FGuid, float>& PercentBySource);
}

USTRUCT(BlueprintType)
struct ARCMOVER_API FArcMovementModifier_Root : public FMovementModifierBase
{
	GENERATED_BODY()

	FArcMovementModifier_Root();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCore|Mover")
	FGuid SourceId;

	virtual FMovementModifierBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual bool Matches(const FMovementModifierBase* Other) const override;
};

template<>
struct TStructOpsTypeTraits<FArcMovementModifier_Root> : public TStructOpsTypeTraitsBase2<FArcMovementModifier_Root>
{
	enum
	{
		WithCopy = true
	};
};

USTRUCT(BlueprintType)
struct ARCMOVER_API FArcMovementModifier_SpeedScale : public FMovementModifierBase
{
	GENERATED_BODY()

	FArcMovementModifier_SpeedScale();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCore|Mover")
	FGuid SourceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCore|Mover", meta = (ClampMin = "0"))
	float SpeedPercent = 100.0f;

	virtual void OnActivated(const FMovementModifierActivatedParams& Params) override;
	virtual void OnDeactivated(const FMovementModifierDeactivatedParams& Params) override;
	virtual void OnPreMovement(const FMovementModifierPreMovementParams& Params) override;
	virtual void OnPostMovement(const FMovementModifierPostMovementParams& Params) override;
	virtual FMovementModifierBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual bool Matches(const FMovementModifierBase* Other) const override;
};

template<>
struct TStructOpsTypeTraits<FArcMovementModifier_SpeedScale> : public TStructOpsTypeTraitsBase2<FArcMovementModifier_SpeedScale>
{
	enum
	{
		WithCopy = true
	};
};
