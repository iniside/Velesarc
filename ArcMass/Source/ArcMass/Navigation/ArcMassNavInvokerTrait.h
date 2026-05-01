// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "AI/Navigation/NavAgentSelector.h"
#include "AI/Navigation/NavigationInvokerPriority.h"
#include "ArcMassNavInvokerTrait.generated.h"

/**
 * Mass entity trait that adds navigation invoker support to an entity.
 * Populates FMassNavInvokerFragment from the configured properties and optionally
 * tags the entity as static (FMassNavInvokerStaticTag) so the observer only
 * needs to register it once rather than tracking movement.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Nav Invoker"))
class ARCMASS_API UArcMassNavInvokerTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	/** Radius within which nav tiles will be generated (>= 0.1 uu). */
	UPROPERTY(EditAnywhere, Category = "NavInvoker", meta = (ClampMin = "0.1"))
	float GenerationRadius = 3000.f;

	/** Radius beyond which nav tiles will be removed. Clamped to >= GenerationRadius. */
	UPROPERTY(EditAnywhere, Category = "NavInvoker", meta = (ClampMin = "0.1"))
	float RemovalRadius = 5000.f;

	/** Which navigation agents this invoker generates tiles for. */
	UPROPERTY(EditAnywhere, Category = "NavInvoker")
	FNavAgentSelector SupportedAgents;

	/** Priority used when dirtying tiles. */
	UPROPERTY(EditAnywhere, Category = "NavInvoker")
	ENavigationInvokerPriority Priority = ENavigationInvokerPriority::Default;

	/**
	 * If true, the entity is treated as a static invoker.
	 * An observer registers it once; no per-frame movement tracking is performed.
	 */
	UPROPERTY(EditAnywhere, Category = "NavInvoker")
	bool bStatic = false;
};
