// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameFeatureAction.h"
#include "GameFeaturesSubsystem.h"

#include "ArcMassGameFeatureAction_WorldActionBase.generated.h"

class UGameInstance;
struct FWorldContext;

/**
 * Base class for ArcMassGameplayFramework GameFeatureActions that need to operate per-world.
 *
 * Mirrors the pattern from ArcCore's UGameFeatureAction_WorldActionBase but lives inside
 * ArcMassGameplayFramework to avoid an ArcCore dependency.
 */
UCLASS(Abstract)
class ARCMASSGAMEPLAYFRAMEWORK_API UArcMassGameFeatureAction_WorldActionBase : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	//~ Begin UGameFeatureAction interface
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;

	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	//~ End UGameFeatureAction interface

private:
	void HandleGameInstanceStart(UGameInstance* GameInstance
								 , FGameFeatureStateChangeContext ChangeContext);

	/** Override with the action-specific logic */
	virtual void AddToWorld(const FWorldContext& WorldContext
							, const FGameFeatureStateChangeContext& ChangeContext) PURE_VIRTUAL(
		UArcMassGameFeatureAction_WorldActionBase::AddToWorld
		,);

private:
	TMap<FGameFeatureStateChangeContext, FDelegateHandle> GameInstanceStartHandles;
};
