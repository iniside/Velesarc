// Copyright Lukasz Baran. All Rights Reserved.

#include "Camera/ArcRecoilInput2DCameraNode.h"

#include "ArcRecoilApi.h"
#include "ArcRecoilDebt.h"
#include "ArcRecoilEntityResolve.h"
#include "ArcRecoilModule.h"
#include "Core/CameraEvaluationContext.h"
#include "Core/CameraOperation.h"
#include "Fragments/ArcRecoilFragments.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Mass/EntityManager.h"

namespace ArcRecoilCameraNodePrivate
{
	constexpr float UnresolvedWarningDelay = 2.0f;
}

namespace UE::Cameras
{

class FArcRecoilInput2DCameraNodeEvaluator : public FInput2DCameraNodeEvaluator
{
	UE_DECLARE_CAMERA_NODE_EVALUATOR_EX(ARCRECOIL_API, FArcRecoilInput2DCameraNodeEvaluator, FInput2DCameraNodeEvaluator)

protected:
	virtual void OnBuild(const FCameraNodeEvaluatorBuildParams& Params) override;
	virtual FCameraNodeEvaluatorChildrenView OnGetChildren() override;
	virtual void OnInitialize(const FCameraNodeEvaluatorInitializeParams& Params, FCameraNodeEvaluationResult& OutResult) override;
	virtual void OnRun(const FCameraNodeEvaluationParams& Params, FCameraNodeEvaluationResult& OutResult) override;

private:
	FInput2DCameraNodeEvaluator* InputNodeEvaluator = nullptr;

	FVector2f Debt = FVector2f::ZeroVector;
	float TimeSinceKick = 0.0f;
	float UnresolvedTime = 0.0f;

	bool bLoggedStartup = false;
	bool bWarnedNoInputSlot = false;
	bool bWarnedUnresolvedEntity = false;
};

UE_DEFINE_CAMERA_NODE_EVALUATOR(FArcRecoilInput2DCameraNodeEvaluator)

void FArcRecoilInput2DCameraNodeEvaluator::OnBuild(const FCameraNodeEvaluatorBuildParams& Params)
{
	const UArcRecoilInput2DCameraNode* RecoilNode = GetCameraNodeAs<UArcRecoilInput2DCameraNode>();
	if (RecoilNode->InputSlot)
	{
		InputNodeEvaluator = Params.BuildEvaluatorAs<FInput2DCameraNodeEvaluator>(RecoilNode->InputSlot);
	}
}

FCameraNodeEvaluatorChildrenView FArcRecoilInput2DCameraNodeEvaluator::OnGetChildren()
{
	return FCameraNodeEvaluatorChildrenView({ InputNodeEvaluator });
}

void FArcRecoilInput2DCameraNodeEvaluator::OnInitialize(const FCameraNodeEvaluatorInitializeParams& Params, FCameraNodeEvaluationResult& OutResult)
{
	Super::OnInitialize(Params, OutResult);

	Debt = FVector2f::ZeroVector;
	TimeSinceKick = 0.0f;
	UnresolvedTime = 0.0f;

	if (InputNodeEvaluator != nullptr)
	{
		InputValue = InputNodeEvaluator->GetInputValue();
	}
}

void FArcRecoilInput2DCameraNodeEvaluator::OnRun(const FCameraNodeEvaluationParams& Params, FCameraNodeEvaluationResult& OutResult)
{
	const UArcRecoilInput2DCameraNode* RecoilNode = GetCameraNodeAs<UArcRecoilInput2DCameraNode>();

	FVector2d InputBefore = FVector2d::ZeroVector;
	if (InputNodeEvaluator != nullptr)
	{
		InputBefore = InputNodeEvaluator->GetInputValue();

		InputNodeEvaluator->Run(Params, OutResult);

		InputValue = InputNodeEvaluator->GetInputValue();
	}
	else if (!bWarnedNoInputSlot)
	{
		bWarnedNoInputSlot = true;
		UE_LOGF(LogArcRecoil, Warning,"Camera node '%ls' has no InputSlot connected - recoil cannot reach the camera.",
			*GetNameSafe(RecoilNode));
	}

	APlayerController* PlayerController = Params.EvaluationContext.IsValid()
		? Params.EvaluationContext->GetPlayerController()
		: nullptr;
	APawn* Pawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;

	ArcRecoil::FResolvedRecoilEntity Resolved;
	const ArcRecoil::EEntityResolveResult ResolveResult = ArcRecoil::ResolveRecoilEntity(Pawn, Resolved);
	if (ResolveResult != ArcRecoil::EEntityResolveResult::Resolved)
	{
		UnresolvedTime += Params.DeltaTime;
		if (!bWarnedUnresolvedEntity && UnresolvedTime > ArcRecoilCameraNodePrivate::UnresolvedWarningDelay)
		{
			bWarnedUnresolvedEntity = true;
			UE_LOGF(LogArcRecoil, Warning,"Camera node '%ls': no recoil entity after %.1fs (%ls) - camera kick is disabled for this view.",
				*GetNameSafe(RecoilNode), UnresolvedTime, ArcRecoil::DescribeResolveResult(ResolveResult));
		}
		return;
	}

	UnresolvedTime = 0.0f;

	if (!bLoggedStartup)
	{
		bLoggedStartup = true;
		UE_LOGF(LogArcRecoil, Log,"Camera node '%ls' active: InputSlot=%ls Entity=%d DegreesToSlotUnits=%.4f",
			*GetNameSafe(RecoilNode),
			InputNodeEvaluator != nullptr ? TEXT("connected") : TEXT("missing"),
			Resolved.Entity.Index, RecoilNode->DegreesToSlotUnits);
	}

	const FArcRecoilProfileFragment* Profile = Resolved.EntityManager->GetFragmentDataPtr<FArcRecoilProfileFragment>(Resolved.Entity);
	if (Profile == nullptr)
	{
		return;
	}

	const double SlotUnitsPerDegree = FMath::Max(static_cast<double>(RecoilNode->DegreesToSlotUnits), UE_DOUBLE_KINDA_SMALL_NUMBER);

	FVector2f PlayerDeltaDeg = FVector2f::ZeroVector;
	if (InputNodeEvaluator != nullptr)
	{
		const FVector2d InputDelta = InputValue - InputBefore;
		PlayerDeltaDeg = FVector2f(
			static_cast<float>(InputDelta.Y / SlotUnitsPerDegree),
			static_cast<float>(InputDelta.X / SlotUnitsPerDegree));
	}

	const FVector2f DebtBeforeCredit = Debt;
	ArcRecoil::CreditPlayerCompensation(Debt, PlayerDeltaDeg);

	TimeSinceKick += Params.DeltaTime;

	FVector2f StepDeg = FVector2f::ZeroVector;
	if (TimeSinceKick > Profile->RecoveryDelay)
	{
		StepDeg = ArcRecoil::StepDebtRecovery(Debt, Profile->RecoveryRate, Params.DeltaTime);
	}

	const FVector2f PendingKickDeg = ArcRecoil::ConsumePendingCameraKick(*Resolved.EntityManager, Resolved.Entity);
	if (!PendingKickDeg.IsNearlyZero())
	{
		Debt += PendingKickDeg;
		TimeSinceKick = 0.0f;
	}

	const FVector2f TotalDeg = PendingKickDeg - StepDeg;

	FVector2f RefusedDeg = FVector2f::ZeroVector;
	if (InputNodeEvaluator != nullptr && !TotalDeg.IsNearlyZero())
	{
		FCameraOperationParams OperationParams;
		OperationParams.Evaluator = Params.Evaluator;
		OperationParams.EvaluationContext = Params.EvaluationContext;

		FYawPitchCameraOperation Operation;
		Operation.Yaw = FConsumableDouble::Delta(static_cast<double>(TotalDeg.Y) * SlotUnitsPerDegree);
		Operation.Pitch = FConsumableDouble::Delta(static_cast<double>(TotalDeg.X) * SlotUnitsPerDegree);

		InputNodeEvaluator->ExecuteOperation(OperationParams, Operation);

		InputValue = InputNodeEvaluator->GetInputValue();

		// Whatever the slot's clamp refused never reached the view, so it must leave the debt or recovery hands it back.
		if (Operation.Pitch.HasValue())
		{
			RefusedDeg.X = static_cast<float>(Operation.Pitch.Get() / SlotUnitsPerDegree);
		}
		if (Operation.Yaw.HasValue())
		{
			RefusedDeg.Y = static_cast<float>(Operation.Yaw.Get() / SlotUnitsPerDegree);
		}

		Debt -= RefusedDeg;
	}

	if (CVarArcRecoilLogEntity.GetValueOnGameThread() == Resolved.Entity.Index
		&& (!PlayerDeltaDeg.IsNearlyZero() || !TotalDeg.IsNearlyZero() || !Debt.IsNearlyZero()))
	{
		UE_LOGF(LogArcRecoil, Verbose,"CameraNode: entity %d PlayerDelta=(%.4f, %.4f) Debt=(%.4f, %.4f)->(%.4f, %.4f) PendingKick=(%.4f, %.4f) Step=(%.4f, %.4f) Total=(%.4f, %.4f) Refused=%ls(%.4f, %.4f)",
			Resolved.Entity.Index,
			PlayerDeltaDeg.X, PlayerDeltaDeg.Y,
			DebtBeforeCredit.X, DebtBeforeCredit.Y, Debt.X, Debt.Y,
			PendingKickDeg.X, PendingKickDeg.Y,
			StepDeg.X, StepDeg.Y,
			TotalDeg.X, TotalDeg.Y,
			RefusedDeg.IsNearlyZero() ? TEXT("none") : TEXT("clamped"),
			RefusedDeg.X, RefusedDeg.Y);
	}
}

}  // namespace UE::Cameras

UArcRecoilInput2DCameraNode::UArcRecoilInput2DCameraNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AddNodeFlags(ECameraNodeFlags::CustomGetChildren);
}

FCameraNodeChildrenView UArcRecoilInput2DCameraNode::OnGetChildren()
{
	return FCameraNodeChildrenView({ InputSlot });
}

FCameraNodeEvaluatorPtr UArcRecoilInput2DCameraNode::OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const
{
	return Builder.BuildEvaluator<UE::Cameras::FArcRecoilInput2DCameraNodeEvaluator>();
}
