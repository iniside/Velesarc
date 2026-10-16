// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "ArcTeamAttitude.generated.h"

/** How one side regards another. Neutral means "not a valid target" and nothing else: it does not
 *  flee, warn or change stance. */
UENUM(BlueprintType)
enum class EArcTeamAttitude : uint8
{
	Hostile,
	Neutral,
	Friendly
};

/** One authored relation. Symmetric today - resolution matches it in either direction - while the
 *  resolution call stays directional so a one-way grudge lands inside that function. */
USTRUCT(BlueprintType)
struct ARCAI_API FArcTeamRelation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Team Relation")
	int32 TeamA = 0;

	UPROPERTY(EditAnywhere, Category = "Team Relation")
	int32 TeamB = 0;

	// Neutral, so a malformed ini row is inert rather than hostile. Every other failure in
	// this design resolves toward Neutral; a row that lost its attitude must too.
	UPROPERTY(EditAnywhere, Category = "Team Relation")
	EArcTeamAttitude Attitude = EArcTeamAttitude::Neutral;
};

/** The authored team relation table. Sparse by necessity: team ids are unbounded bare integers with
 *  no identity asset, and an unlisted pair is Neutral, so hostility is a deliberate act. */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "ArcAI Team Attitudes"))
class ARCAI_API UArcTeamAttitudeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName("Arc Game"); }
	virtual FName GetSectionName() const override { return FName("Team Attitudes"); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Scanned in authored order; the first row matching a pair in either direction wins. A row
	 *  naming one team twice, or naming a teamless side, is unreachable rather than invalid. */
	UPROPERTY(config, EditAnywhere, Category = "Teams")
	TArray<FArcTeamRelation> Relations;
};

namespace ArcTeamAttitude
{
	/** What Observer's side makes of Target's side. Total for every input: no caller needs to
	 *  pre-check, and no branch assumes a validator ran. */
	ARCAI_API EArcTeamAttitude Resolve(int32 ObserverTeamId, int32 TargetTeamId);
}
