// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_SpawnTargetedProjectile.h"

#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "ArcMass/ArcMassEntityLibrary.h"
#include "ArcMass/Actions/ArcMassAction.h"
#include "ArcMass/Actions/ArcMassActionFragment.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/Projectile/ArcProjectileFragments.h"
#include "ArcMass/Projectile/ArcProjectileSpawning.h"
#include "ArcMass/Projectile/ArcProjectileUtils.h"
#include "Engine/World.h"
#include "Effects/ArcEffectContext.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Mass/Commands.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "MassEntityTemplate.h"
#include "MassSignalSubsystem.h"
#include "Signals/ArcAbilitySignals.h"
#include "StateTreeAsyncExecutionContext.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTargetedProjectileSpawn, Log, All);

namespace ArcSpawnTargetedProjectilePrivate
{
	struct FPreparedTarget
	{
		FMassEntityHandle TargetEntity;
		FArcMassActionFragment Actions;
	};

	struct FDeferredSpawnRequest
	{
		explicit FDeferredSpawnRequest(UArcMassEntityConfigAsset* InProjectileConfig)
			: ProjectileConfig(InProjectileConfig)
		{
		}

		TStrongObjectPtr<UArcMassEntityConfigAsset> ProjectileConfig;
		TArray<FPreparedTarget> Targets;
		FTransform SpawnTransform;
	};
}

EStateTreeRunStatus FArcAbilityTask_SpawnTargetedProjectile::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	if (Transition.ChangeType == EStateTreeStateChangeType::Sustained)
	{
		return EStateTreeRunStatus::Running;
	}

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FArcAbilityTask_SpawnTargetedProjectileInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_SpawnTargetedProjectileInstanceData>(*this);
	InstanceData.ProjectileEntities.Reset();
	++InstanceData.ActivationSerial;
	InstanceData.bIsActive = true;
	InstanceData.bTriggerPending = false;
	InstanceData.bSpawnQueued = false;

	if (InstanceData.SpawnTrigger.IsValid())
	{
		const uint32 ActivationSerial = InstanceData.ActivationSerial;
		Context.BindDelegate(
			InstanceData.SpawnTrigger,
			FSimpleDelegate::CreateLambda(
				[WeakContext = Context.MakeWeakExecutionContext(), ActivationSerial]()
				{
					FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
					FArcAbilityTask_SpawnTargetedProjectileInstanceData* ActiveData =
						StrongContext.GetInstanceDataPtr<FArcAbilityTask_SpawnTargetedProjectileInstanceData>();
					if (!ActiveData
						|| !ActiveData->bIsActive
						|| ActiveData->ActivationSerial != ActivationSerial
						|| ActiveData->bTriggerPending
						|| ActiveData->bSpawnQueued)
					{
						return;
					}

					ActiveData->bTriggerPending = true;
				}));

		return EStateTreeRunStatus::Running;
	}

	return QueueSpawn(Context);
}

EStateTreeRunStatus FArcAbilityTask_SpawnTargetedProjectile::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FArcAbilityTask_SpawnTargetedProjectileInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_SpawnTargetedProjectileInstanceData>(*this);
	if (!InstanceData.bIsActive || !InstanceData.bTriggerPending || InstanceData.bSpawnQueued)
	{
		return EStateTreeRunStatus::Running;
	}

	InstanceData.bTriggerPending = false;
	return QueueSpawn(Context);
}

void FArcAbilityTask_SpawnTargetedProjectile::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	if (Transition.ChangeType == EStateTreeStateChangeType::Sustained)
	{
		return;
	}

	FArcAbilityTask_SpawnTargetedProjectileInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_SpawnTargetedProjectileInstanceData>(*this);
	Context.UnbindDelegate(InstanceData.SpawnTrigger);
	if (InstanceData.bTriggerPending && !InstanceData.bSpawnQueued)
	{
		InstanceData.bTriggerPending = false;
		QueueSpawn(Context);
	}
	InstanceData.bIsActive = false;
	InstanceData.bTriggerPending = false;
	InstanceData.bSpawnQueued = false;
	++InstanceData.ActivationSerial;
}

EStateTreeRunStatus FArcAbilityTask_SpawnTargetedProjectile::QueueSpawn(
	FStateTreeExecutionContext& Context) const
{
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FArcAbilityTask_SpawnTargetedProjectileInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_SpawnTargetedProjectileInstanceData>(*this);
	if (InstanceData.bSpawnQueued)
	{
		return EStateTreeRunStatus::Running;
	}
	InstanceData.bSpawnQueued = true;

	UWorld* World = AbilityContext.GetMassContext().GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !InstanceData.ProjectileConfig)
	{
		UE_LOGF(LogArcTargetedProjectileSpawn, Warning,
			"Spawn rejected: world=%ls netMode=%d config=%ls",
			*GetNameSafe(World), World ? static_cast<int32>(World->GetNetMode()) : -1,
			*GetNameSafe(InstanceData.ProjectileConfig));
		return EStateTreeRunStatus::Failed;
	}

	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	const FMassEntityHandle SourceEntity = AbilityContext.GetEntity();
	const bool bSourceValid = EntityManager.IsEntityValid(SourceEntity);
	const bool bSourceBuilt = bSourceValid && EntityManager.IsEntityBuilt(SourceEntity);
	const FTransformFragment* SourceTransformFragment = bSourceBuilt
		? EntityManager.GetFragmentDataPtr<FTransformFragment>(SourceEntity)
		: nullptr;
	const bool bSourceHasTransform = SourceTransformFragment != nullptr;
	if (!bSourceHasTransform
		|| !InstanceData.TargetDataHandle.HasHitResults())
	{
		UE_LOGF(LogArcTargetedProjectileSpawn, Warning,
			"Spawn rejected: source=[%d:%d] valid=%d built=%d transform=%d hits=%d",
			SourceEntity.Index, SourceEntity.SerialNumber,
			bSourceValid, bSourceBuilt, bSourceHasTransform,
			InstanceData.TargetDataHandle.GetHitResults().Num());
		return EStateTreeRunStatus::Failed;
	}
	UE_LOGF(LogArcTargetedProjectileSpawn, Log,
		"Queue spawn source=[%d:%d] config=%ls hits=%d override=%d location=%ls",
		SourceEntity.Index, SourceEntity.SerialNumber, *GetNameSafe(InstanceData.ProjectileConfig),
		InstanceData.TargetDataHandle.GetHitResults().Num(), InstanceData.bUseOverrideLocation,
		*InstanceData.OverrideLocation.ToString());

	TArray<FMassEntityHandle> TargetEntities;
	for (const FHitResult& Hit : InstanceData.TargetDataHandle.GetHitResults())
	{
		EArcMassResult ResolveResult = EArcMassResult::NotValid;
		const FMassEntityHandle TargetEntity = UArcMassEntityLibrary::ResolveHitToEntity(Hit, ResolveResult);
		const bool bTargetValid = EntityManager.IsEntityValid(TargetEntity);
		const bool bTargetBuilt = bTargetValid && EntityManager.IsEntityBuilt(TargetEntity);
		const bool bTargetHasTransform = bTargetBuilt && EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetEntity) != nullptr;
		UE_LOGF(LogArcTargetedProjectileSpawn, Log,
			"Hit actor=%ls impact=%ls resolved=[%d:%d] result=%d valid=%d built=%d transform=%d",
			*GetNameSafe(Hit.GetActor()), *Hit.ImpactPoint.ToString(), TargetEntity.Index, TargetEntity.SerialNumber,
			static_cast<int32>(ResolveResult), bTargetValid, bTargetBuilt, bTargetHasTransform);
		if (bTargetHasTransform)
		{
			TargetEntities.AddUnique(TargetEntity);
		}
	}

	if (TargetEntities.IsEmpty())
	{
		UE_LOGF(LogArcTargetedProjectileSpawn, Warning,
			"Spawn rejected: no valid Mass targets from %d hits for source=[%d:%d]",
			InstanceData.TargetDataHandle.GetHitResults().Num(), SourceEntity.Index, SourceEntity.SerialNumber);
		return EStateTreeRunStatus::Failed;
	}
	UE_LOGF(LogArcTargetedProjectileSpawn, Log,
		"Queue deferred spawn: source=[%d:%d] uniqueTargets=%d",
		SourceEntity.Index, SourceEntity.SerialNumber, TargetEntities.Num());

	const FTransform SourceTransform = SourceTransformFragment->GetTransform();
	const FVector SpawnLocation = InstanceData.bUseOverrideLocation
		? InstanceData.OverrideLocation
		: SourceTransform.GetLocation();
	const FTransform SpawnTransform(SourceTransform.GetRotation(), SpawnLocation);

	FArcEffectContext EffectContext = ArcAbilities::MakeAbilityEffectContext(EntityManager, SourceEntity, AbilityContext.GetAbilityHandle());
	EffectContext.EntityManager = &EntityManager;

	const FArcMassActionFragment InitialActions = UE::ArcMass::Actions::BuildInitialActionFragment(
		*InstanceData.ProjectileConfig,
		*World);
	TSharedRef<ArcSpawnTargetedProjectilePrivate::FDeferredSpawnRequest> Request =
		MakeShared<ArcSpawnTargetedProjectilePrivate::FDeferredSpawnRequest>(InstanceData.ProjectileConfig.Get());
	Request->SpawnTransform = SpawnTransform;
	Request->Targets.Reserve(TargetEntities.Num());
	for (const FMassEntityHandle TargetEntity : TargetEntities)
	{
		ArcSpawnTargetedProjectilePrivate::FPreparedTarget& PreparedTarget = Request->Targets.AddDefaulted_GetRef();
		PreparedTarget.TargetEntity = TargetEntity;
		PreparedTarget.Actions = InitialActions;
		PreparedTarget.Actions.GetMutableStatelessActions().Remove(UE::ArcMass::Projectile::Events::ProjectileHit);
		TMap<FName, FArcMassStatefulActionList>& StatefulActions = PreparedTarget.Actions.GetMutableStatefulActions();
		StatefulActions.Remove(UE::ArcMass::Projectile::Events::ProjectileHit);
		if (!InstanceData.ImpactActions.IsEmpty())
		{
			EffectContext.TargetEntity = TargetEntity;
			FArcMassStatefulActionList& ImpactActions = StatefulActions.Add(
				UE::ArcMass::Projectile::Events::ProjectileHit);
			ImpactActions.Actions = InstanceData.ImpactActions;
			FArcMassActionPreparationContext PreparationContext{
				EntityManager,
				*World,
				SourceEntity,
				TargetEntity,
				FConstStructView::Make(EffectContext)};
			if (!ImpactActions.Prepare(PreparationContext))
			{
				InstanceData.ProjectileEntities.Reset();
				return EStateTreeRunStatus::Failed;
			}
		}
	}

	const TWeakObjectPtr<UWorld> WeakWorld = World;
	const uint32 ActivationSerial = InstanceData.ActivationSerial;
	const FStateTreeWeakExecutionContext WeakContext = Context.MakeWeakExecutionContext();

	AbilityContext.GetMassContext().Defer().PushCommand<FMassDeferredCreateCommand>(
		[WeakWorld, WeakContext, Request, SourceEntity, ActivationSerial]
		(FMassEntityManager& DeferredEntityManager)
		{
			UWorld* DeferredWorld = WeakWorld.Get();
			UArcMassEntityConfigAsset* ProjectileConfig = Request->ProjectileConfig.Get();
			TArray<FMassEntityHandle> ValidSpawnedEntities;
			bool bCanSpawn = DeferredWorld
				&& DeferredWorld->GetNetMode() != NM_Client
				&& ProjectileConfig;
			if (!bCanSpawn)
			{
				UE_LOGF(LogArcTargetedProjectileSpawn, Warning,
					"Deferred spawn rejected: world=%ls netMode=%d config=%ls source=[%d:%d]",
					*GetNameSafe(DeferredWorld), DeferredWorld ? static_cast<int32>(DeferredWorld->GetNetMode()) : -1,
					*GetNameSafe(ProjectileConfig), SourceEntity.Index, SourceEntity.SerialNumber);
			}

			if (bCanSpawn)
			{
				const FMassEntityTemplate& EntityTemplate = ProjectileConfig->GetOrCreateEntityTemplate(*DeferredWorld);
				if (EntityTemplate.IsValid())
				{
					const FMassEntityTemplateData& TemplateData = EntityTemplate.GetTemplateData();
					bCanSpawn = TemplateData.HasFragment<FTransformFragment>()
						&& TemplateData.HasFragment<FArcProjectileFragment>()
						&& TemplateData.HasFragment<FArcProjectileCollisionFilterFragment>()
						&& TemplateData.HasFragment<FArcTargetedProjectileFragment>()
						&& TemplateData.HasConstSharedFragment<FArcProjectileConfigFragment>()
						&& TemplateData.HasConstSharedFragment<FArcTargetedProjectileConfigFragment>()
						&& TemplateData.HasTag<FArcProjectileTag>()
						&& TemplateData.HasTag<FArcTargetedProjectileTag>();
					if (!bCanSpawn)
					{
					UE_LOGF(LogArcTargetedProjectileSpawn, Warning,
						"Deferred spawn rejected: config=%ls template lacks required targeted projectile fragments or tags",
						*GetNameSafe(ProjectileConfig));
					}
				}
				else
				{
					bCanSpawn = false;
					UE_LOGF(LogArcTargetedProjectileSpawn, Warning,
						"Deferred spawn rejected: invalid template for config=%ls", *GetNameSafe(ProjectileConfig));
				}
			}

			if (bCanSpawn)
			{
				UE_LOGF(LogArcTargetedProjectileSpawn, Log,
					"Deferred spawn transform: source=[%d:%d] location=%ls requestedTargets=%d",
					SourceEntity.Index, SourceEntity.SerialNumber, *Request->SpawnTransform.GetLocation().ToString(), Request->Targets.Num());
				FArcProjectileSpawnData SpawnData;
				for (const ArcSpawnTargetedProjectilePrivate::FPreparedTarget& PreparedTarget : Request->Targets)
				{
					const FMassEntityHandle TargetEntity = PreparedTarget.TargetEntity;
					const bool bTargetValid = DeferredEntityManager.IsEntityValid(TargetEntity);
					const bool bTargetBuilt = bTargetValid && DeferredEntityManager.IsEntityBuilt(TargetEntity);
					const bool bTargetHasTransform = bTargetBuilt && DeferredEntityManager.GetFragmentDataPtr<FTransformFragment>(TargetEntity) != nullptr;
					if (bTargetHasTransform)
					{
						SpawnData.Transforms.Add(Request->SpawnTransform);
						SpawnData.TargetEntities.Add(TargetEntity);
						SpawnData.ActionFragments.Add(PreparedTarget.Actions);
					}
					else
					{
						UE_LOGF(LogArcTargetedProjectileSpawn, Warning,
							"Deferred target rejected: target=[%d:%d] valid=%d built=%d transform=%d",
							TargetEntity.Index, TargetEntity.SerialNumber,
							bTargetValid, bTargetBuilt, bTargetHasTransform);
					}
				}
				UE_LOGF(LogArcTargetedProjectileSpawn, Log,
					"Batch spawn requested: source=[%d:%d] count=%d",
					SourceEntity.Index, SourceEntity.SerialNumber, SpawnData.TargetEntities.Num());

				if (!SpawnData.TargetEntities.IsEmpty())
				{
					TArray<FMassEntityHandle> SpawnedEntities;
					UE::ArcMass::Projectile::BatchSpawnProjectiles(
						DeferredWorld,
						ProjectileConfig,
						SpawnData,
						SpawnedEntities);
					UE_LOGF(LogArcTargetedProjectileSpawn, Log,
						"Batch spawn returned: source=[%d:%d] requested=%d returned=%d",
						SourceEntity.Index, SourceEntity.SerialNumber,
						SpawnData.TargetEntities.Num(), SpawnedEntities.Num());

					for (int32 SpawnIndex = 0; SpawnIndex < SpawnedEntities.Num(); ++SpawnIndex)
					{
						const FMassEntityHandle SpawnedEntity = SpawnedEntities[SpawnIndex];
						const bool bSpawnedValid = DeferredEntityManager.IsEntityValid(SpawnedEntity);
						const bool bSpawnedBuilt = bSpawnedValid && DeferredEntityManager.IsEntityBuilt(SpawnedEntity);
						UE_LOGF(LogArcTargetedProjectileSpawn, Log,
							"Spawned projectile: index=%d entity=[%d:%d] valid=%d built=%d",
							SpawnIndex, SpawnedEntity.Index, SpawnedEntity.SerialNumber,
							bSpawnedValid, bSpawnedBuilt);
						if (bSpawnedBuilt)
						{
							ValidSpawnedEntities.Add(SpawnedEntity);
						}
					}
				}
			}

			bool bSignalCompletion = false;
			{
				FStateTreeStrongExecutionContext CompletionContext = WeakContext.MakeStrongExecutionContext();
				if (!CompletionContext.IsValid())
				{
					UE_LOGF(LogArcTargetedProjectileSpawn, Log,
						"Completion skipped: StateTree context expired for source=[%d:%d]",
						SourceEntity.Index, SourceEntity.SerialNumber);
					return;
				}

				FArcAbilityTask_SpawnTargetedProjectileInstanceData* CompletionData =
					CompletionContext.GetInstanceDataPtr<FArcAbilityTask_SpawnTargetedProjectileInstanceData>();
				if (!CompletionData
					|| !CompletionData->bIsActive
					|| CompletionData->ActivationSerial != ActivationSerial
					|| !CompletionData->bSpawnQueued)
				{
					UE_LOGF(LogArcTargetedProjectileSpawn, Log,
						"Completion skipped: activation expired for source=[%d:%d]",
						SourceEntity.Index, SourceEntity.SerialNumber);
					return;
				}

				CompletionData->ProjectileEntities = MoveTemp(ValidSpawnedEntities);
				if (CompletionData->ProjectileEntities.IsEmpty())
				{
					UE_LOGF(LogArcTargetedProjectileSpawn, Warning,
						"Completion: source=[%d:%d] projectileCount=0 status=Failed",
						SourceEntity.Index, SourceEntity.SerialNumber);
				}
				else
				{
					UE_LOGF(LogArcTargetedProjectileSpawn, Log,
						"Completion: source=[%d:%d] projectileCount=%d status=Succeeded",
						SourceEntity.Index, SourceEntity.SerialNumber, CompletionData->ProjectileEntities.Num());
				}
				CompletionContext.CopyOutputBindings();
				CompletionContext.FinishTask(
					!CompletionData->ProjectileEntities.IsEmpty()
						? EStateTreeFinishTaskType::Succeeded
						: EStateTreeFinishTaskType::Failed);
				bSignalCompletion = true;
			}

			if (bSignalCompletion && DeferredWorld)
			{
				UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(DeferredWorld);
				if (SignalSubsystem)
				{
					SignalSubsystem->SignalEntity(
						UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired,
						SourceEntity);
				}
			}
		});

	return EStateTreeRunStatus::Running;
}
