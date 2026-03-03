// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "ArcMassPersistence.generated.h"

USTRUCT()
struct ARCMASS_API FArcMassPersistenceFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGuid PersistenceGuid;

	FIntVector StorageCell = FIntVector(MAX_int32, MAX_int32, MAX_int32);
	FIntVector CurrentCell = FIntVector(MAX_int32, MAX_int32, MAX_int32);
};

USTRUCT()
struct ARCMASS_API FArcMassPersistenceTag : public FMassTag
{
	GENERATED_BODY()
};

/** Marks an entity as a persistence source (e.g., player). */
USTRUCT()
struct ARCMASS_API FArcMassPersistenceSourceTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCMASS_API FArcMassPersistenceConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowAbstract = "false"))
	TArray<TObjectPtr<UScriptStruct>> AllowedFragments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowAbstract = "false"))
	TArray<TObjectPtr<UScriptStruct>> DisallowedFragments;
};
