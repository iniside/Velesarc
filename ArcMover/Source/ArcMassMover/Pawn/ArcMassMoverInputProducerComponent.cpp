/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "Pawn/ArcMassMoverInputProducerComponent.h"

#include "AIController.h"
#include "Mover/ArcChaosWalking.h"
#include "ArcMass/ArcMassFragments.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/NavMoverComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagAssetInterface.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetMathLibrary.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "MassNavigationFragments.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "MoverDataModelTypes.h"
#include "Mover/ArcMoverTypes.h"
#include "Mover/ArcMovementModifiers.h"
#include "Mover/ArcWalkGait.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"
#include "Pawn/ArcMassPlayerInputComponent.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverComponent.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMoverDebug, Log, All);

namespace ArcMassMoverInputProducer
{
	static TAutoConsoleVariable<int32> CVarDebugStop(
		TEXT("arc.Mover.DebugStop"),
		0,
		TEXT("Logs per-frame mover state, last input cmd, predicted trajectory and per-step ProduceInput lines to LogArcMoverDebug.\n")
		TEXT("  0: off\n")
		TEXT("  1: on"),
		ECVF_Cheat);

	static bool IsDebugStopEnabled()
	{
		return CVarDebugStop.GetValueOnAnyThread() != 0;
	}

	template <typename TEnum>
	static FString EnumName(TEnum Value)
	{
		return StaticEnum<TEnum>()->GetNameStringByValue(static_cast<int64>(Value));
	}

	static FString FormatVector(const FVector& Vector)
	{
		return FString::Printf(TEXT("(%.1f,%.1f,%.1f)"), Vector.X, Vector.Y, Vector.Z);
	}

	static void LogProduceInput(const AActor* Owner
								, int32 SimTimeMs
								, const FCharacterDefaultInputs& CharacterInputs
								, const FArcMoverCustomInputs& CustomInputs
								, bool bBlocked)
	{
		UE_LOGF(LogArcMoverDebug, Log, "[DebugStop.PI] F=%llu %ls SimTimeMs=%d Blocked=%d MoveInput=%ls Dir=%ls"
			, GFrameCounter
			, *GetNameSafe(Owner)
			, SimTimeMs
			, bBlocked ? 1 : 0
			, *FormatVector(CharacterInputs.GetMoveInput())
			, *EnumName(CustomInputs.MovementDirection));
	}
#if ENABLE_DRAW_DEBUG
	// 0 = off, 1 = arrows + screen-anchored text, 2 = arrows + in-world 3D text.
	static TAutoConsoleVariable<int32> CVarDebugDraw(
		TEXT("arc.MassMover.DebugDraw"),
		0,
		TEXT("Debug draw for UArcMassMoverInputProducerComponent.\n")
		TEXT("  0: off\n")
		TEXT("  1: arrows + in-world text\n")
		TEXT("  2: arrows only"),
		ECVF_Cheat);
#endif
	static FGameplayTag GetTag_BlockAllInput()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("Mover.BlockAllInput"), false);
		return Tag;
	}

	static FGameplayTag GetTag_GaitWalk()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("Mover.Gait.Walk"), false);
		return Tag;
	}

	static FGameplayTag GetTag_GaitRun()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("Mover.Gait.Run"), false);
		return Tag;
	}

	static FGameplayTag GetTag_GaitSprint()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("Mover.Gait.Sprint"), false);
		return Tag;
	}

	struct FWalkContext
	{
		const IArcWalkGaitProvider* Provider = nullptr;
		const UMoverComponent* Mover = nullptr;
		bool bCrouching = false;
		float LegacyMaxSpeed = 0.f;
	};

	static bool ResolveWalkContext(AActor* Owner, FWalkContext& OutContext)
	{
		UMoverComponent* Mover = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr;
		if (Mover == nullptr)
		{
			return false;
		}

		const IArcWalkGaitProvider* Provider = ArcMover::FindWalkGaitProvider(*Mover);
		if (Provider == nullptr)
		{
			return false;
		}

		const UCharacterMoverComponent* CharacterMover = Cast<UCharacterMoverComponent>(Mover);
		OutContext.Provider = Provider;
		OutContext.Mover = Mover;
		OutContext.bCrouching = CharacterMover && CharacterMover->IsCrouching();
		OutContext.LegacyMaxSpeed = Provider->GetBaseMaxSpeed() * ArcMover::GetActiveSpeedScale(*Mover);
		return true;
	}

	static const FMassEntityManager* ResolveOwnerEntity(UWorld* World, const AActor* Owner, FMassEntityHandle& OutEntity)
	{
		if (World == nullptr || Owner == nullptr)
		{
			return nullptr;
		}

		UMassActorSubsystem* ActorSubsystem = World->GetSubsystem<UMassActorSubsystem>();
		const UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
		if (ActorSubsystem == nullptr || EntitySubsystem == nullptr)
		{
			return nullptr;
		}

		const FMassEntityHandle Entity = ActorSubsystem->GetEntityHandleFromActor(Owner);
		const FMassEntityManager& EntityManager = EntitySubsystem->GetEntityManager();
		if (!EntityManager.IsEntityActive(Entity))
		{
			return nullptr;
		}

		OutEntity = Entity;
		return &EntityManager;
	}

	static void SetFocus(FCharacterDefaultInputs& CharacterInputs
						 , FArcMoverCustomInputs& CustomInputs
						 , const FVector& PawnLocation
						 , const FVector& FocalPoint)
	{
		const FVector Dir = (FocalPoint - PawnLocation).GetSafeNormal();
		CharacterInputs.ControlRotation.Pitch = 0;
		CharacterInputs.ControlRotation.Roll = 0;
		CharacterInputs.ControlRotation.Yaw = Dir.Rotation().Yaw;
		CustomInputs.bHaveFocusTarget = true;
		CustomInputs.RotationMode = EArcMoverAimModeType::Aim;
	}
}

UArcMassMoverInputProducerComponent::UArcMassMoverInputProducerComponent(const FObjectInitializer& ObjectInitializer)
	: Super{ObjectInitializer}
{
}

void UArcMassMoverInputProducerComponent::TickComponent(float DeltaTime
														, enum ELevelTick TickType
														, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* Pawn = GetPawnChecked<APawn>();

	FRotator NewRotation = Pawn->GetControlRotation();
	FRotator AimRot = UKismetMathLibrary::NormalizedDeltaRotator(NewRotation, LastControlRotation);
	ControlRotationRate = AimRot.Yaw / GetWorld()->GetDeltaSeconds();
	LastControlRotation = NewRotation;
}

void UArcMassMoverInputProducerComponent::ProduceInput_Implementation(int32 SimTimeMs
																	  , FMoverInputCmdContext& InputCmdResult)
{
	FCharacterDefaultInputs& CharacterInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	FArcMoverCustomInputs& CustomInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FArcMoverCustomInputs>();
	UArcMassPawnExtensionComponent* HeroComp = GetPawnExtensionComponent();
	APawn* Pawn = GetPawnChecked<APawn>();
	IGameplayTagAssetInterface* GTAI = Cast<IGameplayTagAssetInterface>(Pawn);
	const FGameplayTag BlockAllInputTag = ArcMassMoverInputProducer::GetTag_BlockAllInput();
	const bool bDebugStop = ArcMassMoverInputProducer::IsDebugStopEnabled();
	if (bDebugStop)
	{
		DebugStopProduceInputCount.fetch_add(1, std::memory_order_relaxed);
	}
	if (GTAI && BlockAllInputTag.IsValid() && GTAI->HasMatchingGameplayTag(BlockAllInputTag))
	{
		CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, FVector::ZeroVector);
		CharacterInputs.bIsJumpJustPressed = false;
		CharacterInputs.bIsJumpPressed = false;
		bJumpJustPressedPending = false;
		if (bDebugStop)
		{
			ArcMassMoverInputProducer::LogProduceInput(Pawn, SimTimeMs, CharacterInputs, CustomInputs, true);
		}
		return;
	}

	CustomInputs.bHaveFocusTarget = false;

	ApplyMoveInput(CharacterInputs, HeroComp, Pawn);

	TOptional<EArcMoverGaitType> MassGait;
	ApplyNavMovement(CharacterInputs, GTAI, MassGait);

	CharacterInputs.ControlRotation = Pawn->GetControlRotation();
	CharacterInputs.bIsJumpJustPressed = bJumpJustPressedPending;
	CharacterInputs.bIsJumpPressed = bJumpHeld;
	// Cleared after emission so each press reaches exactly one input command.
	bJumpJustPressedPending = false;

	CustomInputs.RotationMode = EArcMoverAimModeType::Strafe;
	if (AimModeOverride.IsSet())
	{
		CustomInputs.RotationMode = AimModeOverride.GetValue();
	}

	const bool bMassFocus = ApplyMassFocus(CharacterInputs, CustomInputs, Pawn);
	const bool bAIFocus = ApplyAIFocus(CharacterInputs, CustomInputs, Pawn);

	ApplyOrientationIntent(CharacterInputs, CustomInputs, Pawn, bMassFocus && !bAIFocus);

	ApplyGait(CustomInputs, GTAI, MassGait);

	ComputeMovementDirection(CharacterInputs, CustomInputs, Pawn);

#if ENABLE_DRAW_DEBUG
	DrawDebug(CharacterInputs, CustomInputs, Pawn, MovementDirectionAngle);
#endif

	if (bDebugStop)
	{
		ArcMassMoverInputProducer::LogProduceInput(Pawn, SimTimeMs, CharacterInputs, CustomInputs, false);
	}
}

void UArcMassMoverInputProducerComponent::BeginPlay()
{
	Super::BeginPlay();

	LastMovementDirection = EArcMoverDirectionType::F;
	LastRotationOffset = 0.0;
	LastOrientationIntent = FVector::ZeroVector;

	DebugStopPostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &UArcMassMoverInputProducerComponent::HandleDebugStopPostActorTick);
}

void UArcMassMoverInputProducerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FWorldDelegates::OnWorldPostActorTick.Remove(DebugStopPostActorTickHandle);
	DebugStopPostActorTickHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void UArcMassMoverInputProducerComponent::HandleDebugStopPostActorTick(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds)
{
	if (InWorld != GetWorld())
	{
		return;
	}

	if (!ArcMassMoverInputProducer::IsDebugStopEnabled())
	{
		DebugStopProduceInputCount.store(0, std::memory_order_relaxed);
		return;
	}

	LogDebugStopFrame();
}

void UArcMassMoverInputProducerComponent::LogDebugStopFrame()
{
	AActor* Owner = GetOwner();
	UMoverComponent* Mover = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr;
	const int32 ProduceInputCalls = DebugStopProduceInputCount.exchange(0, std::memory_order_relaxed);
	if (Mover == nullptr)
	{
		UE_LOGF(LogArcMoverDebug, Log, "[DebugStop] F=%llu %ls no UMoverComponent", GFrameCounter, *GetNameSafe(Owner));
		return;
	}

	const FVector Velocity = Mover->GetVelocity();
	const FMoverInputCmdContext& LastInputCmd = Mover->GetLastInputCmd();
	const FCharacterDefaultInputs* CharacterInputs = LastInputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FArcMoverCustomInputs* CustomInputs = LastInputCmd.InputCollection.FindDataByType<FArcMoverCustomInputs>();

	FString InputText;
	if (CharacterInputs != nullptr)
	{
		InputText += FString::Printf(TEXT(" MoveInput=%s Orient=%s")
			, *ArcMassMoverInputProducer::FormatVector(CharacterInputs->GetMoveInput())
			, *ArcMassMoverInputProducer::FormatVector(CharacterInputs->OrientationIntent));
	}
	else
	{
		InputText += TEXT(" MoveInput=none");
	}
	if (CustomInputs != nullptr)
	{
		InputText += FString::Printf(TEXT(" Dir=%s Rot=%s Gait=%s RotOff=%.1f")
			, *ArcMassMoverInputProducer::EnumName(CustomInputs->MovementDirection)
			, *ArcMassMoverInputProducer::EnumName(CustomInputs->RotationMode)
			, *ArcMassMoverInputProducer::EnumName(CustomInputs->Gait)
			, CustomInputs->RotationOffset);
	}
	else
	{
		InputText += TEXT(" Custom=none");
	}

	const float SamplingInterval = 1.f / 60.f;
	const float LookAheadSeconds = 1.5f;
	FMoverPredictTrajectoryParams PredictParams;
	PredictParams.NumPredictionSamples = FMath::FloorToInt32(LookAheadSeconds / SamplingInterval) + 2;
	PredictParams.SecondsPerSample = SamplingInterval;
	PredictParams.bUseVisualComponentRoot = true;
	const TArray<FTrajectorySampleInfo> Samples = Mover->GetPredictedTrajectory(PredictParams);

	FString TrajectoryText;
	if (Samples.Num() > 0)
	{
		const FTrajectorySampleInfo& Current = Samples[0];
		const FVector CurrentLocation = Current.Transform.GetLocation();
		TrajectoryText = FString::Printf(TEXT(" Pred[n=%d t0=%.1fms V0=%s]"), Samples.Num(), Current.SimTimeMs, *ArcMassMoverInputProducer::FormatVector(Current.LinearVelocity));
		const float ProbeSeconds[] = {0.1f, 0.25f, 0.5f, 1.0f};
		for (const float Probe : ProbeSeconds)
		{
			const int32 Index = FMath::Clamp(FMath::RoundToInt32(Probe / SamplingInterval), 0, Samples.Num() - 1);
			const FTrajectorySampleInfo& Sample = Samples[Index];
			TrajectoryText += FString::Printf(TEXT(" +%.2f{i=%d t=%.1fms P=%s V=%s}")
				, Probe
				, Index
				, Sample.SimTimeMs
				, *ArcMassMoverInputProducer::FormatVector(Sample.Transform.GetLocation() - CurrentLocation)
				, *ArcMassMoverInputProducer::FormatVector(Sample.LinearVelocity));
		}
	}
	else
	{
		TrajectoryText = TEXT(" Pred[n=0]");
	}

	const FArcChaosSpringFeedState* SpringFeed = Mover->GetSyncState().SyncStateCollection.FindDataByType<FArcChaosSpringFeedState>();
	const FString PropText = SpringFeed != nullptr
		? FString::Printf(TEXT(" Prop=%s"), *ArcMassMoverInputProducer::FormatVector(SpringFeed->LastProposedVelocity))
		: FString(TEXT(" Prop=none"));

	const UClass* BackendClass = Mover->BackendClass.Get();
	UE_LOGF(LogArcMoverDebug, Log, "[DebugStop] F=%llu T=%.4f %ls Role=%ls Mover=%ls Backend=%ls Mode=%ls Vel=%ls Speed2D=%.1f PI=%d%ls%ls%ls"
		, GFrameCounter
		, GetWorld()->GetTimeSeconds()
		, *GetNameSafe(Owner)
		, *ArcMassMoverInputProducer::EnumName(Owner->GetLocalRole())
		, *Mover->GetClass()->GetName()
		, *GetNameSafe(BackendClass)
		, *Mover->GetMovementModeName().ToString()
		, *ArcMassMoverInputProducer::FormatVector(Velocity)
		, Velocity.Size2D()
		, ProduceInputCalls
		, *InputText
		, *TrajectoryText
		, *PropText);
}

void UArcMassMoverInputProducerComponent::ApplyMoveInput(FCharacterDefaultInputs& CharacterInputs
														 , UArcMassPawnExtensionComponent* HeroComp
														 , APawn* Pawn) const
{
	if (!HeroComp)
	{
		return;
	}

	// Player input lives on UArcMassPlayerInputComponent (only present on locally-controlled
	// non-bot pawns). Server / AI pawns never spawn it; in that case we fall through with
	// a zero swizzle so OverrideInput / nav movement can still drive the pawn.
	FVector Swizzled = FVector::ZeroVector;
	if (UArcMassPlayerInputComponent* InputComp = UArcMassPlayerInputComponent::FindPlayerInputComponent(GetOwner()))
	{
		const FVector2D& MoveInputVector = InputComp->GetMoveInputVector();
		Swizzled.X = MoveInputVector.Y;
		Swizzled.Y = MoveInputVector.X;
		Swizzled.Z = 0.0;
		Swizzled = Swizzled.GetClampedToSize(0, 1);
	}

	if (OverrideInput.IsSet())
	{
		Swizzled = OverrideInput.GetValue();
	}

	FRotator ControlRotation = Pawn->GetControlRotation();
	ControlRotation.Pitch = 0.0f;
	ControlRotation.Roll = 0.0f;

	FVector RotatedVector = UKismetMathLibrary::GreaterGreater_VectorRotator(Swizzled, ControlRotation);
	RotatedVector.Normalize();

	CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, RotatedVector);
}

bool UArcMassMoverInputProducerComponent::ApplyNavMovement(FCharacterDefaultInputs& CharacterInputs
														, IGameplayTagAssetInterface* GTAI
														, TOptional<EArcMoverGaitType>& OutMassGait)
{
	bool bRequestedNavMovement = false;
	if (GetNavMoverComponent())
	{
		bRequestedNavMovement = NavMoverComponent->ConsumeNavMovementData(CachedMoveInputIntent, CachedMoveInputVelocity);
		if (bRequestedNavMovement)
		{
			OutMassGait = ResolveMassGait();
		}
		// Favor velocity input
		bool bUsingInputIntentForMove = CachedMoveInputVelocity.IsZero();

		if (bUsingInputIntentForMove)
		{
			const FVector FinalDirectionalIntent = CharacterInputs.ControlRotation.RotateVector(CachedMoveInputIntent);
			CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, FinalDirectionalIntent);
		}
		else
		{
			if (NavMoveInputType == EMoveInputType::Velocity)
			{
				CharacterInputs.SetMoveInput(EMoveInputType::Velocity, CachedMoveInputVelocity);
			}
			else
			{
				float IntentMagnitude = 1.f;
				ArcMassMoverInputProducer::FWalkContext Walk;
				if (ArcMassMoverInputProducer::ResolveWalkContext(GetOwner(), Walk))
				{
					const EArcMoverGaitType FinalGait = ResolveFinalGait(GTAI, OutMassGait);
					const FVector Velocity = Walk.Mover->GetVelocity();
					const float CurrentSpeedXY = FVector2D(Velocity.X, Velocity.Y).Size();
					const float MaxSpeed = ArcWalkGait::ResolveWalkLimits(Walk.Provider->GetGaitSettings(), FinalGait, Walk.bCrouching, true, Walk.LegacyMaxSpeed
						, Walk.Provider->GetDecelerationSmoothingTime(), CurrentSpeedXY).MaxSpeed;
					if (MaxSpeed > UE_KINDA_SMALL_NUMBER)
					{
						IntentMagnitude = FMath::Clamp(CachedMoveInputVelocity.Size() / MaxSpeed, 0.f, 1.f);
					}
				}
				CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, CachedMoveInputVelocity.GetSafeNormal() * IntentMagnitude);
			}
		}

		// Normally cached input is cleared by OnMoveCompleted input event but that won't be called if movement came from nav movement
		if (bRequestedNavMovement)
		{
			CachedMoveInputIntent = FVector::ZeroVector;
			CachedMoveInputVelocity = FVector::ZeroVector;
		}
	}
	return bRequestedNavMovement;
}

TOptional<EArcMoverGaitType> UArcMassMoverInputProducerComponent::ResolveMassGait() const
{
	FMassEntityHandle Entity;
	const FMassEntityManager* EntityManagerPtr = ArcMassMoverInputProducer::ResolveOwnerEntity(GetWorld(), GetOwner(), Entity);
	if (EntityManagerPtr == nullptr)
	{
		return {};
	}
	const FMassEntityManager& EntityManager = *EntityManagerPtr;

	const FMassMoveTargetFragment* MoveTarget = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity);
	if (MoveTarget == nullptr)
	{
		return {};
	}

	ArcMassMoverInputProducer::FWalkContext Walk;
	if (!ArcMassMoverInputProducer::ResolveWalkContext(GetOwner(), Walk))
	{
		return {};
	}

	const float DesiredSpeed = MoveTarget->DesiredSpeed.Get() * ArcMover::GetActiveSpeedScale(*Walk.Mover);
	return ArcWalkGait::GaitForSpeed(Walk.Provider->GetGaitSettings(), DesiredSpeed, Walk.bCrouching, Walk.LegacyMaxSpeed);
}

bool UArcMassMoverInputProducerComponent::ApplyMassFocus(FCharacterDefaultInputs& CharacterInputs
														 , FArcMoverCustomInputs& CustomInputs
														 , APawn* Pawn) const
{
	FMassEntityHandle Entity;
	const FMassEntityManager* EntityManager = ArcMassMoverInputProducer::ResolveOwnerEntity(GetWorld(), GetOwner(), Entity);
	if (EntityManager == nullptr)
	{
		return false;
	}

	const FMassEntityView EntityView(*EntityManager, Entity);
	const FArcMassFocusFragment* Focus = EntityView.GetSparseFragmentDataPtr<FArcMassFocusFragment>();
	if (Focus == nullptr)
	{
		return false;
	}

	FVector FocalPoint = FVector::ZeroVector;
	switch (Focus->Kind)
	{
		case EArcMassFocusKind::Entity:
		{
			if (!EntityManager->IsEntityActive(Focus->TargetEntity))
			{
				return false;
			}
			const FTransformFragment* TargetTransform = EntityManager->GetFragmentDataPtr<FTransformFragment>(Focus->TargetEntity);
			if (TargetTransform == nullptr)
			{
				return false;
			}
			FocalPoint = TargetTransform->GetTransform().GetLocation();
			break;
		}
		case EArcMassFocusKind::Location:
			FocalPoint = Focus->TargetLocation;
			break;
		case EArcMassFocusKind::None:
			return false;
	}

	ArcMassMoverInputProducer::SetFocus(CharacterInputs, CustomInputs, Pawn->GetActorLocation(), FocalPoint);
	return true;
}

bool UArcMassMoverInputProducerComponent::ApplyAIFocus(FCharacterDefaultInputs& CharacterInputs
													   , FArcMoverCustomInputs& CustomInputs
													   , APawn* Pawn) const
{
	if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
	{
		if (AI->GetFocusActor())
		{
			ArcMassMoverInputProducer::SetFocus(CharacterInputs, CustomInputs, Pawn->GetActorLocation(), AI->GetFocalPoint());
			return true;
		}
	}
	return false;
}

void UArcMassMoverInputProducerComponent::ApplyOrientationIntent(FCharacterDefaultInputs& CharacterInputs
																 , const FArcMoverCustomInputs& CustomInputs
																 , APawn* Pawn
																 , bool bMassFocusSource)
{
	FVector Intent = FVector::ZeroVector;
	if (CharacterInputs.GetMoveInput().Size() != 0)
	{
		switch (CustomInputs.RotationMode)
		{
			case EArcMoverAimModeType::OrientToMovement:
				Intent = CharacterInputs.GetMoveInput();
				break;
			case EArcMoverAimModeType::Strafe:
			case EArcMoverAimModeType::Aim:
				FRotator AimRot = CharacterInputs.ControlRotation;
				AimRot.Pitch = 0.0f;
				AimRot.Roll = 0.0f;
				Intent = AimRot.Vector();
				break;
		}
	}
	else
	{
		switch (CustomInputs.RotationMode)
		{
			case EArcMoverAimModeType::OrientToMovement:
			case EArcMoverAimModeType::Strafe:
				Intent = LastOrientationIntent;
				break;
			case EArcMoverAimModeType::Aim:
				if (bMassFocusSource)
				{
					Intent = FRotator(0.0, CharacterInputs.ControlRotation.Yaw, 0.0).Vector();
					break;
				}
				const FRotator AimDelta = UKismetMathLibrary::NormalizedDeltaRotator(Pawn->GetActorRotation(), CharacterInputs.ControlRotation);
				if (FMath::Abs(AimDelta.Yaw) > 60)
				{
					Intent = FRotator(0.0, CharacterInputs.ControlRotation.Yaw, 0.0).Vector();
				}
				else
				{
					Intent = LastOrientationIntent;
				}
				break;
		}
	}
	CharacterInputs.OrientationIntent = Intent;
	if (!Intent.IsNearlyZero())
	{
		LastOrientationIntent = Intent;
	}
}

EArcMoverGaitType UArcMassMoverInputProducerComponent::ResolveFinalGait(IGameplayTagAssetInterface* GTAI
																		, const TOptional<EArcMoverGaitType>& MassGait) const
{
	if (GaitOverride.IsSet())
	{
		return GaitOverride.GetValue();
	}

	if (GTAI)
	{
		const FGameplayTag GaitWalkTag = ArcMassMoverInputProducer::GetTag_GaitWalk();
		const FGameplayTag GaitRunTag = ArcMassMoverInputProducer::GetTag_GaitRun();
		const FGameplayTag GaitSprintTag = ArcMassMoverInputProducer::GetTag_GaitSprint();

		if (GaitWalkTag.IsValid() && GTAI->HasMatchingGameplayTag(GaitWalkTag))
		{
			return EArcMoverGaitType::Walk;
		}
		if (GaitRunTag.IsValid() && GTAI->HasMatchingGameplayTag(GaitRunTag))
		{
			return EArcMoverGaitType::Run;
		}
		if (GaitSprintTag.IsValid() && GTAI->HasMatchingGameplayTag(GaitSprintTag))
		{
			return EArcMoverGaitType::Sprint;
		}
	}

	return MassGait.Get(EArcMoverGaitType::Run);
}

void UArcMassMoverInputProducerComponent::ApplyGait(FArcMoverCustomInputs& CustomInputs
													, IGameplayTagAssetInterface* GTAI
													, const TOptional<EArcMoverGaitType>& MassGait) const
{
	CustomInputs.Gait = ResolveFinalGait(GTAI, MassGait);
	CustomInputs.ControlRotationRate = ControlRotationRate;
	CustomInputs.WantsToCrouch = false;
}

void UArcMassMoverInputProducerComponent::ComputeMovementDirection(const FCharacterDefaultInputs& CharacterInputs
																   , FArcMoverCustomInputs& CustomInputs
																   , APawn* Pawn)
{
	if (CustomInputs.RotationMode == EArcMoverAimModeType::OrientToMovement)
	{
		CustomInputs.MovementDirection = EArcMoverDirectionType::F;
		CustomInputs.RotationOffset = 0.0;
		LastMovementDirection = CustomInputs.MovementDirection;
		LastRotationOffset = CustomInputs.RotationOffset;
		return;
	}

	UMoverComponent* Mover = GetOwner()->FindComponentByClass<UMoverComponent>();
	FVector Velocity = Mover->GetVelocity();

	Velocity.Z = 0.0f;
	Velocity.Normalize();

	// MovementMode
	FVector Forward = Pawn->GetActorForwardVector();

	DirectionOfMovement = CharacterInputs.GetMoveInput();
	float Dot = FVector::DotProduct(Forward, DirectionOfMovement.GetSafeNormal());
	CustomInputs.MaxVelocityMultiplier = FMath::GetMappedRangeValueClamped(FVector2f{1.f, -1.f}, {1.f, 0.5f}, Dot);
	if (DirectionOfMovement.Size() <= 0)
	{
		CustomInputs.MovementDirection = LastMovementDirection;
		CustomInputs.RotationOffset = LastRotationOffset;
		return;
	}

	FRotator MoveRot = UKismetMathLibrary::NormalizedDeltaRotator(DirectionOfMovement.Rotation(), CharacterInputs.OrientationIntent.Rotation());
	MovementDirectionAngle = MoveRot.Yaw;
	if (LastMovementDirection == EArcMoverDirectionType::F)
	{
		FRotator AimRot = UKismetMathLibrary::NormalizedDeltaRotator(Pawn->GetActorRotation(), CharacterInputs.OrientationIntent.Rotation());
		double Angle = 0.0;
		if (LastRotationOffset == 0.0)
		{
			Angle = AimRot.Yaw;
		}
		else
		{
			Angle = LastRotationOffset;
		}

		if (Angle > 0)
		{
			bool bInRange = UKismetMathLibrary::InRange_FloatFloat(MovementDirectionAngle, -180, -170, true, true);
			if (bInRange)
			{
				MovementDirectionAngle = 179.f;
			}
		}
		else
		{
			bool bInRange = UKismetMathLibrary::InRange_FloatFloat(MovementDirectionAngle, 170, 180, true, true);
			if (bInRange)
			{
				MovementDirectionAngle = -179.f;
			}
		}
	}

	FArcMassMoverRotationThresholds Dirs = GetRotationThresholds(LastMovementDirection);

	bool bInRangeF = UKismetMathLibrary::InRange_FloatFloat(MovementDirectionAngle, Dirs.FL, Dirs.FR, true, true);

	bool bInRangeLL = UKismetMathLibrary::InRange_FloatFloat(MovementDirectionAngle, Dirs.BL, Dirs.FL, true, true);

	bool bInRangeRL = UKismetMathLibrary::InRange_FloatFloat(MovementDirectionAngle, Dirs.FR, Dirs.BR, true, true);

	if (bInRangeF)
	{
		CustomInputs.MovementDirection = EArcMoverDirectionType::F;
	}
	else if (bInRangeLL)
	{
		CustomInputs.MovementDirection = EArcMoverDirectionType::LL;
	}
	else if (bInRangeRL)
	{
		CustomInputs.MovementDirection = EArcMoverDirectionType::RL;
	}
	else
	{
		CustomInputs.MovementDirection = EArcMoverDirectionType::B;
	}

	UCurveFloat* SelectedCurve = nullptr;
	switch (CustomInputs.MovementDirection)
	{
		case EArcMoverDirectionType::F:
			SelectedCurve = RotationOffsetF;
			break;
		case EArcMoverDirectionType::B:
			SelectedCurve = RotationOffsetB;
			break;
		case EArcMoverDirectionType::LL:
			SelectedCurve = RotationOffsetLL;
			break;
		case EArcMoverDirectionType::LR:
			SelectedCurve = RotationOffsetLR;
			break;
		case EArcMoverDirectionType::RL:
			SelectedCurve = RotationOffsetRL;
			break;
		case EArcMoverDirectionType::RR:
			SelectedCurve = RotationOffsetRR;
			break;
	}

	if (SelectedCurve)
	{
		CustomInputs.RotationOffset = SelectedCurve->GetFloatValue(MovementDirectionAngle);
	}

	LastMovementDirection = CustomInputs.MovementDirection;
	LastRotationOffset = CustomInputs.RotationOffset;
}

void UArcMassMoverInputProducerComponent::DrawDebug(const FCharacterDefaultInputs& CharacterInputs
													, const FArcMoverCustomInputs& CustomInputs
													, APawn* Pawn
													, float InMovementDirectionAngle) const
{
#if ENABLE_DRAW_DEBUG
	const int32 DebugMode = ArcMassMoverInputProducer::CVarDebugDraw.GetValueOnAnyThread();
	if (DebugMode <= 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FVector Origin = Pawn->GetActorLocation();
	const FVector Anchor = Origin + FVector(0.f, 0.f, 90.f);
	constexpr float ArrowLen = 150.f;
	constexpr float ArrowSize = 25.f;
	constexpr float ArrowThickness = 2.f;
	constexpr float Lifetime = 0.f;
	constexpr uint8 Depth = 0;

	const FRotator ControlRot = CharacterInputs.ControlRotation;
	FRotator FlatControlRot = ControlRot;
	FlatControlRot.Pitch = 0.f;
	FlatControlRot.Roll = 0.f;

	// Yellow: control rotation (camera/aim yaw).
	DrawDebugDirectionalArrow(World, Anchor, Anchor + FlatControlRot.Vector() * ArrowLen
		, ArrowSize, FColor::Yellow, false, Lifetime, Depth, ArrowThickness);

	// Cyan: orientation intent (where the pawn wants to face).
	const FVector OrientationDir = CharacterInputs.OrientationIntent.GetSafeNormal();
	if (!OrientationDir.IsNearlyZero())
	{
		DrawDebugDirectionalArrow(World, Anchor, Anchor + OrientationDir * ArrowLen
			, ArrowSize, FColor::Cyan, false, Lifetime, Depth, ArrowThickness);
	}

	// Green: directional move input (world-space).
	const FVector MoveDir = CharacterInputs.GetMoveInput();
	if (!MoveDir.IsNearlyZero())
	{
		DrawDebugDirectionalArrow(World, Anchor, Anchor + MoveDir.GetSafeNormal() * ArrowLen
			, ArrowSize, FColor::Green, false, Lifetime, Depth, ArrowThickness);
	}

	// White: pawn forward.
	DrawDebugDirectionalArrow(World, Anchor, Anchor + Pawn->GetActorForwardVector() * (ArrowLen * 0.6f)
		, ArrowSize, FColor::White, false, Lifetime, Depth, 1.f);

	// Magenta: current velocity (flat).
	if (UMoverComponent* MoverComp = GetOwner()->FindComponentByClass<UMoverComponent>())
	{
		FVector Vel = MoverComp->GetVelocity();
		Vel.Z = 0.f;
		if (!Vel.IsNearlyZero())
		{
			DrawDebugDirectionalArrow(World, Anchor, Anchor + Vel.GetSafeNormal() * ArrowLen
				, ArrowSize, FColor::Magenta, false, Lifetime, Depth, ArrowThickness);
		}
	}

	if (DebugMode == 1)
	{
		const FString Info = FString::Printf(
			TEXT("ArcMassMoverInputProducer\n")
			TEXT("Gait=%s  RotationMode=%s  HaveFocusTarget=%s\n")
			TEXT("MovementDir=%s  Angle=%.1f  RotationOffset=%.2f\n")
			TEXT("MoveInput=(%.2f, %.2f, %.2f)  Size=%.2f\n")
			TEXT("ControlRot=(P=%.1f Y=%.1f)  Rate=%.1f\n")
			TEXT("OrientationIntent=(%.2f, %.2f)\n")
			TEXT("MaxVelocityMultiplier=%.2f")
			, *UEnum::GetValueAsString(CustomInputs.Gait)
			, *UEnum::GetValueAsString(CustomInputs.RotationMode)
			, CustomInputs.bHaveFocusTarget ? TEXT("true") : TEXT("false")
			, *UEnum::GetValueAsString(CustomInputs.MovementDirection)
			, InMovementDirectionAngle
			, CustomInputs.RotationOffset
			, MoveDir.X, MoveDir.Y, MoveDir.Z, MoveDir.Size()
			, ControlRot.Pitch, ControlRot.Yaw, ControlRotationRate
			, CharacterInputs.OrientationIntent.X, CharacterInputs.OrientationIntent.Y
			, CustomInputs.MaxVelocityMultiplier);

		DrawDebugString(World, Anchor + FVector(0.f, 0.f, 30.f), Info
			, nullptr, FColor::White, 0.f, false, 1.0f);
	}
#endif // ENABLE_DRAW_DEBUG
}

void UArcMassMoverInputProducerComponent::RequestJump()
{
	if (!bJumpHeld)
	{
		bJumpJustPressedPending = true;
	}
	bJumpHeld = true;
}

void UArcMassMoverInputProducerComponent::ReleaseJump()
{
	bJumpHeld = false;
}

UArcMassPawnExtensionComponent* UArcMassMoverInputProducerComponent::GetPawnExtensionComponent() const
{
	if (PawnExtensionComponent)
	{
		return PawnExtensionComponent;
	}

	PawnExtensionComponent = GetOwner()->FindComponentByClass<UArcMassPawnExtensionComponent>();
	return PawnExtensionComponent;
}

UNavMoverComponent* UArcMassMoverInputProducerComponent::GetNavMoverComponent() const
{
	if (NavMoverComponent)
	{
		return NavMoverComponent;
	}

	NavMoverComponent = GetOwner()->FindComponentByClass<UNavMoverComponent>();
	return NavMoverComponent;
}
