// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Mass/EntityHandle.h"

#include "ArcMassEntityInitializer.generated.h"

struct FMassEntityManager;
class UArcMassEntityConfigAsset;

/**
 * Asset-level post-Commit entity initializer. Declared on UArcMassEntityConfigAsset
 * and invoked by UArcMassSpawnerSubsystem after FEntityBuilder.Commit returns
 * (outside any creation context, outside any processor).
 *
 * Complements UArcMassEntityTraitBase::InitializeEntity (trait-owned init) by
 * providing an asset-level extension point that isn't bound to a specific trait.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class ARCMASS_API UArcMassEntityInitializer : public UObject
{
	GENERATED_BODY()

public:
	virtual void InitializeEntity(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UArcMassEntityConfigAsset& Cfg) const {}
};
