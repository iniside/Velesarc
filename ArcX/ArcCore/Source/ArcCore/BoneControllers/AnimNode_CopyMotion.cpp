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

#include "BoneControllers/AnimNode_CopyMotion.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveFloat.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "Animation/AnimSubsystem_Tag.h"
#include "Engine/SkeletalMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_CopyMotion)

DECLARE_CYCLE_STAT(TEXT("CopyMotion Eval"), STAT_CopyMotion_Eval, STATGROUP_Anim);

namespace UE::Anim::CopyMotion
{
#if ENABLE_ANIM_DEBUG
	static TAutoConsoleVariable<int32> CVarDebug(TEXT("a.AnimNode.CopyMotion.Debug"), 0, TEXT("Turn on visualization debugging for Copy Motion. 1: Debug draw coordinate systems, 2: Only show copied motion, 3: Draw motion history, 4: Enable all"));
#endif
	static TAutoConsoleVariable<int32> CVarEnable(TEXT("a.AnimNode.CopyMotion.Enable"), 1, TEXT("Toggle the Copy Motion node."));

	bool IsDebugEnabled()
	{
#if ENABLE_ANIM_DEBUG
		return CVarDebug.GetValueOnAnyThread() > 0;
#else
		return false;
#endif
	}

	bool ShouldShowCopiedMotion()
	{
#if ENABLE_ANIM_DEBUG
		return	CVarDebug.GetValueOnAnyThread() == 2 ||
				CVarDebug.GetValueOnAnyThread() == 4;
#else
		return false;
#endif
	}

	bool ShouldDrawMotionHistory()
	{
#if ENABLE_ANIM_DEBUG
		return	CVarDebug.GetValueOnAnyThread() == 3 ||
				CVarDebug.GetValueOnAnyThread() == 4;
#else
		return false;
#endif
	}

	FTransform GetDeltaTransform(const FTransform& SourceToTransform, const FTransform& SourceFromTransform, const FTransform& BoneSpaceToTransform, const FTransform& BoneSpaceFromPoseTransform)
	{
		const FTransform DeltaFrom = SourceFromTransform.GetRelativeTransform(BoneSpaceFromPoseTransform);
		const FTransform DeltaFromInBoneSpace = DeltaFrom * BoneSpaceToTransform;
		const FTransform DeltaOut = SourceToTransform.GetRelativeTransform(DeltaFromInBoneSpace);

		return DeltaOut;
	}

	FQuat ReOrientDeltaQuat(const FQuat DeltaQuat, const FQuat NewOrientation)
	{
		FVector DeltaAxis;
		float DeltaAngle;
		DeltaQuat.ToAxisAndAngle(DeltaAxis, DeltaAngle);

		FVector ReOrientedAxis = NewOrientation.RotateVector(DeltaAxis);
		//Calculate the new delta rotation
		FQuat ReOrientedQuat(ReOrientedAxis, DeltaAngle);
		ReOrientedQuat.Normalize();

		return ReOrientedQuat;
	}

	const bool GetPoseHistoryCollectorNodeFromTag(const FName Tag, const UAnimInstance* AnimInstance, const FAnimNode_PoseSearchHistoryCollector_Base*& OutNode)
	{
		if (IAnimClassInterface* AnimBlueprintClass = IAnimClassInterface::GetFromClass(AnimInstance->Blueprint_GetMainAnimInstance()->GetClass()))
		{
			if (const FAnimSubsystem_Tag* TagSubsystem = AnimBlueprintClass->FindSubsystem<FAnimSubsystem_Tag>())
			{
				OutNode = TagSubsystem->FindNodeByTag<const FAnimNode_PoseSearchHistoryCollector_Base>(Tag, AnimInstance->Blueprint_GetMainAnimInstance());
				if (OutNode)
				{
					return true;
				}
				else
				{
					OutNode = TagSubsystem->FindNodeByTag<const FAnimNode_PoseSearchHistoryCollector_Base>(Tag, AnimInstance->Blueprint_GetMainAnimInstance());
					if (OutNode)
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	FVector GetAxisVector(const EAxis::Type InAxis)
	{
		switch (InAxis)
		{
		case EAxis::X: return FVector::ForwardVector;
		case EAxis::Y: return FVector::RightVector;
		default:
		case EAxis::Z: return FVector::UpVector;
		};
	}

	float GetSignedAngleAroundAxis(const FQuat& Rotation, const FVector& TwistAxis)
	{
		FQuat Swing, Twist;

		Rotation.ToSwingTwist(TwistAxis, Swing, Twist);
		if (FVector::DotProduct(Twist.GetRotationAxis(), TwistAxis) > 0)
		{
			return -Twist.GetAngle();
		}
		else
		{
			return Twist.GetAngle();
		}
	}
};

FAnimNode_CopyMotion::FAnimNode_CopyMotion()
{
}

void FAnimNode_CopyMotion::GatherDebugData(FNodeDebugData& DebugData)
{
	Super::GatherDebugData(DebugData);

	if (bUseBasePose)
	{
		BasePose.GatherDebugData(DebugData);
		BasePoseReference.GatherDebugData(DebugData);
	}
}

void FAnimNode_CopyMotion::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	Super::Initialize_AnyThread(Context);

	if (bUseBasePose)
	{
		BasePose.Initialize(Context);
		BasePoseReference.Initialize(Context);
	}
}

void FAnimNode_CopyMotion::UpdateInternal(const FAnimationUpdateContext& Context)
{
	Super::UpdateInternal(Context);

	// If we just became relevant and haven't been initialized yet, then reinitialize foot placement.
	if (!bIsFirstUpdate && UpdateCounter.HasEverBeenUpdated() && !UpdateCounter.WasSynchronizedCounter(Context.AnimInstanceProxy->GetUpdateCounter()))
	{
		Reset();
	}
	UpdateCounter.SynchronizeWith(Context.AnimInstanceProxy->GetUpdateCounter());
}

void FAnimNode_CopyMotion::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	SCOPE_CYCLE_COUNTER(STAT_CopyMotion_Eval);
	check(OutBoneTransforms.Num() == 0);
	Super::EvaluateSkeletalControl_AnyThread(Output, OutBoneTransforms);

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	FTransformNoScale TransformOffset;
	EBoneControlSpace TransformOffsetSpace = EBoneControlSpace::BCS_ComponentSpace;

	if (bUseBasePose)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_CopyMotion_CopyTransforms);

		FComponentSpacePoseContext BaseData(Output);
		{
			BasePose.EvaluateComponentSpace(BaseData);
		}

		FComponentSpacePoseContext ReferencePoseData(Output);
		{
			BasePoseReference.EvaluateComponentSpace(ReferencePoseData);
		}

		const FCompactPoseBoneIndex CompactPoseSourceBoneIdx = SourceBone.GetCompactPoseIndex(BoneContainer);

		FTransform SourceToTransform;
		bool bUsePoseHistory = false;
		const FAnimNode_PoseSearchHistoryCollector_Base* PoseHistoryNode = nullptr;
		//@todo: Error out on missing, but expected pose history node. Can this be caught at compile time and cached?
		if (bUseBasePose && (Delay > 0.0f))
		{
			bUsePoseHistory = UE::Anim::CopyMotion::GetPoseHistoryCollectorNodeFromTag(PoseHistoryTag, CastChecked<const UAnimInstance>(Output.GetAnimInstanceObject()), PoseHistoryNode);
		}

		const FSkeletonPoseBoneIndex SkeletonSourceBoneIdx = BoneContainer.GetSkeletonPoseIndexFromCompactPoseIndex(CompactPoseSourceBoneIdx);
		if (!bUsePoseHistory ||
			!PoseHistoryNode->GetPoseHistory().GetTransformAtTime(-Delay, SourceToTransform,
				BoneContainer.GetSkeletalMeshAsset()->GetSkeleton(), SkeletonSourceBoneIdx.GetInt()))
		{
			// If no pose history is found, use current frame instead
			//@todo: Error out on missing bones or if exceeding the time horizon.
			SourceToTransform = BaseData.Pose.GetComponentSpaceTransform(CompactPoseSourceBoneIdx);
		}

		const FTransform SourceFromTransform = ReferencePoseData.Pose.GetComponentSpaceTransform(CompactPoseSourceBoneIdx);

		const bool bNeedAdditivePose = CopySpace.IsValidToEvaluate(BoneContainer);
		FTransform AdditiveTransform;
		if (bNeedAdditivePose)
		{
			AdditiveTransform = SourceToTransform;
			FAnimationRuntime::ConvertTransformToAdditive(AdditiveTransform, SourceFromTransform);
		}

		if (CopySpace.IsValidToEvaluate(BoneContainer))
		{
			const FCompactPoseBoneIndex CompactPoseReferenceBoneIdx = CopySpace.GetCompactPoseIndex(BoneContainer);
			const FSkeletonPoseBoneIndex SkeletonReferenceBoneIdx = BoneContainer.GetSkeletonPoseIndexFromCompactPoseIndex(CompactPoseReferenceBoneIdx);

			FTransform BoneSpaceToTransform;
			if (!bUsePoseHistory || 
				!PoseHistoryNode->GetPoseHistory().GetTransformAtTime(-Delay, BoneSpaceToTransform,
					BoneContainer.GetSkeletalMeshAsset()->GetSkeleton(), SkeletonReferenceBoneIdx.GetInt()))
			{
				// If no pose history is found, use current frame instead
				BoneSpaceToTransform = BaseData.Pose.GetComponentSpaceTransform(CompactPoseReferenceBoneIdx);
			}

			const FTransform BoneSpaceFromPoseTransform = ReferencePoseData.Pose.GetComponentSpaceTransform(CompactPoseReferenceBoneIdx);

			TransformOffset = UE::Anim::CopyMotion::GetDeltaTransform(
				SourceToTransform, 
				SourceFromTransform, 
				BoneSpaceToTransform,
				BoneSpaceFromPoseTransform);

			// Prevent rotation from flipping directions
			TransformOffset.Rotation.Normalize();
			TransformOffset.Rotation = TransformOffset.Rotation.W < 0.0 ? -TransformOffset.Rotation : TransformOffset.Rotation;

			// Resulting translation move delta is in bone space
			TransformOffsetSpace = EBoneControlSpace::BCS_BoneSpace;
		}
		else
		{
			// No reference bone. Bake a component space additive instead.
			TransformOffset = AdditiveTransform;
		}

		if (UE::Anim::CopyMotion::ShouldShowCopiedMotion())
		{
			// Reset pose to input reference pose, to only show copied motion, and no inherited motion.
			Output = ReferencePoseData;
		}
	}

	if (bUseBasePose == false)
	{
		//@todo: Curves may not be the best way to store this information vs a bone or a transform attribute.
		// Grab curve values and scale by the node's modifiers
		TransformOffset.Location.X = Output.Curve.Get(TranslationX_CurveName);
		TransformOffset.Location.Y = Output.Curve.Get(TranslationY_CurveName);
		TransformOffset.Location.Z = Output.Curve.Get(TranslationZ_CurveName);

		RotationOffset.Pitch = Output.Curve.Get(RotationPitch_CurveName);
		RotationOffset.Yaw = Output.Curve.Get(RotationYaw_CurveName);
		RotationOffset.Roll = Output.Curve.Get(RotationRoll_CurveName);
	}

	if (HasTargetCurve())
	{
		// Output the motion to the specified curve as a scalar value.
		const float CurrentTargetCurveValue = Output.Curve.Get(TargetCurveName);
		const float TargetCurveMotionDelta = GetTargetCurveValue(TransformOffset, TargetCurveComponent);
		Output.Curve.Set(TargetCurveName, CurrentTargetCurveValue + TargetCurveMotionDelta * TargetCurveScale);
	}

	if (!HasTargetBone(BoneContainer))
	{
		// No target bone defined. No need to apply this transform.
		return;
	}

	// Scale translation
	if (TranslationRemapCurve)
	{
		const TArray<FRichCurveEditInfoConst> Curves = TranslationRemapCurve->GetCurves();
		TransformOffset.Location.X = Curves[0].CurveToEdit->Eval(TransformOffset.Location.X);
		TransformOffset.Location.Y = Curves[1].CurveToEdit->Eval(TransformOffset.Location.Y);
		TransformOffset.Location.Z = Curves[2].CurveToEdit->Eval(TransformOffset.Location.Z);
	}
	TransformOffset.Location *= TranslationScale;

	// Scale rotation
	{
		float Angle;
		FVector Axis;
		TransformOffset.Rotation.ToAxisAndAngle(Axis, Angle);
		if (RotationRemapCurve)
		{
			Angle = RotationRemapCurve->GetFloatValue(Angle);
		}
		TransformOffset.Rotation = FQuat(Axis, Angle * RotationScale);
	}

	const FCompactPoseBoneIndex CompactPoseBoneToModify = BoneToModify.GetCompactPoseIndex(BoneContainer);
	FTransform NewBoneTM = Output.Pose.GetComponentSpaceTransform(CompactPoseBoneToModify);
	FTransform ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();

#if ENABLE_ANIM_DEBUG
	const FTransform OriginalBoneWS = NewBoneTM * ComponentTransform;
	FTransform ApplySpaceTransform = FTransform::Identity;
#endif

	TransformOffset.Location = TranslationOffset.RotateVector(TransformOffset.Location);
	TransformOffset.Rotation = UE::Anim::CopyMotion::ReOrientDeltaQuat(TransformOffset.Rotation, FQuat(RotationOffset));
	if (ApplySpace.IsValidToEvaluate(BoneContainer))
	{
		// If our translation is in a specified bone's space, convert to component space
		const FCompactPoseBoneIndex CompactPoseBoneSpaceIdx = ApplySpace.GetCompactPoseIndex(BoneContainer);
		const FTransform BoneSpaceTransform = Output.Pose.GetComponentSpaceTransform(CompactPoseBoneSpaceIdx);
#if ENABLE_ANIM_DEBUG
		ApplySpaceTransform = BoneSpaceTransform;
#endif
		
		// Convert the offset relative to the Apply Space bone to component space
		TransformOffset.Location = BoneSpaceTransform.GetRotation().RotateVector(TransformOffset.Location);
		// Convert back to local space
		TransformOffset.Location = NewBoneTM.GetRotation().UnrotateVector(TransformOffset.Location);

		// Convert the offset relative to the Apply Space bone to component space
		TransformOffset.Rotation = UE::Anim::CopyMotion::ReOrientDeltaQuat(TransformOffset.Rotation, BoneSpaceTransform.GetRotation());
		// Convert back to local space
		TransformOffset.Rotation = UE::Anim::CopyMotion::ReOrientDeltaQuat(TransformOffset.Rotation, NewBoneTM.GetRotation().Inverse());
	}

	if ((TransformOffset.Rotation.Rotator().IsNearlyZero() == false)
		|| (TransformOffset.Location.IsNearlyZero() == false))
	{
		// Convert to Bone Space.
		FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentTransform, Output.Pose, NewBoneTM, CompactPoseBoneToModify, TransformOffsetSpace);
		NewBoneTM.AddToTranslation(TransformOffset.Location);
		NewBoneTM.SetRotation(TransformOffset.Rotation * NewBoneTM.GetRotation());

		if (RotationPivot.IsNearlyZero() == false)
		{
			// Rotate our pivot offset by the rotation offset.
			// Apply the pivot delta to the target bone
			const FVector PivotDelta = TransformOffset.Rotation.RotateVector(RotationPivot) - RotationPivot;
			NewBoneTM.AddToTranslation(PivotDelta);
		}

		// Convert back to Component Space.
		FAnimationRuntime::ConvertBoneSpaceTransformToCS(ComponentTransform, Output.Pose, NewBoneTM, CompactPoseBoneToModify, TransformOffsetSpace);
	}

	if (UE::Anim::CopyMotion::IsDebugEnabled())
	{
#if ENABLE_ANIM_DEBUG
		const FQuat& DebugApplySpaceOrientation = (ApplySpace.IsValidToEvaluate(BoneContainer)) ? OriginalBoneWS.GetRotation() : ApplySpaceTransform.GetRotation() * ComponentTransform.GetRotation();
		const FTransform TargetBoneTransformWS = NewBoneTM * ComponentTransform;
		// Draw coordinate system at original bone location
		Output.AnimInstanceProxy->AnimDrawDebugCoordinateSystem(OriginalBoneWS.GetLocation(), DebugApplySpaceOrientation.Rotator(), 15.0f, false, -1.0f, 2.0f, SDPG_Foreground);
		// Draw line between original and new bone locations. Original bone serves as pivot
		Output.AnimInstanceProxy->AnimDrawDebugLine(OriginalBoneWS.GetLocation(), TargetBoneTransformWS.GetLocation(), FColor::Yellow, false, -1.0f, 1.0f, SDPG_Foreground);

		float Angle;
		FVector Axis;
		TransformOffset.Rotation.ToAxisAndAngle(Axis, Angle);
		// Draw hinge axis and hinge coordinate system
		Output.AnimInstanceProxy->AnimDrawDebugLine(TargetBoneTransformWS.GetLocation(), TargetBoneTransformWS.GetLocation() + DebugApplySpaceOrientation.RotateVector(Axis) * 10.0f, FColor::Yellow, false, -1.0f, 1.0f, SDPG_Foreground);
		Output.AnimInstanceProxy->AnimDrawDebugCoordinateSystem(TargetBoneTransformWS.GetLocation(), DebugApplySpaceOrientation.Rotator(), 10.0f, false, -1.0f, 1.0f, SDPG_Foreground);

		const bool bShouldDrawMotionHistory = UE::Anim::CopyMotion::ShouldDrawMotionHistory();
		const FVector LocDebug = TargetBoneTransformWS.GetLocation();
		const FVector RotDebug = LocDebug + DebugApplySpaceOrientation.RotateVector(Axis) * 2.0f;

		if (bShouldDrawMotionHistory)
		{
			// Only draw if we haven't just enabled debug draw.
			if ((bIsDrawHistoryEnabled == bShouldDrawMotionHistory)
				&& !bIsFirstUpdate)
			{
				// Draw a quad in the direction of our hinge axis
				Output.AnimInstanceProxy->AnimDrawDebugLine(LocDebug, RotDebug, FColor::Cyan, false, 2.0f, 0.05f, SDPG_World);
				Output.AnimInstanceProxy->AnimDrawDebugLine(LastRotDebug, LastLocDebug, FColor::Cyan, false, 2.0f, 0.05f, SDPG_World);
				Output.AnimInstanceProxy->AnimDrawDebugLine(LastLocDebug, RotDebug, FColor::Cyan, false, 2.0f, 0.05f, SDPG_World);
				Output.AnimInstanceProxy->AnimDrawDebugLine(LastRotDebug, RotDebug, FColor::Cyan, false, 2.0f, 0.05f, SDPG_World);

				// Draw a line between move deltas.
				Output.AnimInstanceProxy->AnimDrawDebugLine(LocDebug, LastLocDebug, FColor::Yellow, false, 2.0f, 0.2f, SDPG_World);
			}
		}
		LastRotDebug = RotDebug;
		LastLocDebug = LocDebug;

		bIsDrawHistoryEnabled = bShouldDrawMotionHistory;
#endif
	}

	OutBoneTransforms.Add( FBoneTransform(BoneToModify.GetCompactPoseIndex(BoneContainer), NewBoneTM) );
	bIsFirstUpdate = false;
}

void FAnimNode_CopyMotion::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Super::CacheBones_AnyThread(Context);

	if (bUseBasePose)
	{
		BasePose.CacheBones(Context);
		BasePoseReference.CacheBones(Context);
	}
}

void FAnimNode_CopyMotion::UpdateComponentPose_AnyThread(const FAnimationUpdateContext& Context)
{
	Super::UpdateComponentPose_AnyThread(Context);

	if (bUseBasePose)
	{
		BasePose.Update(Context);
		BasePoseReference.Update(Context);
	}
}

bool FAnimNode_CopyMotion::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	if ( UE::Anim::CopyMotion::CVarEnable.GetValueOnAnyThread() == 0)
	{
		return false;
	}

	if (!HasTargetBone(RequiredBones) && !HasTargetCurve())
	{
		return false;
	}

	if ((ApplySpace.BoneIndex != INDEX_NONE) && (ApplySpace.IsValidToEvaluate(RequiredBones) == false))
	{
		return false;
	}

	if (bUseBasePose)
	{
		if (SourceBone.IsValidToEvaluate(RequiredBones) == false)
		{
			return false;
		}

		if ((CopySpace.BoneIndex != INDEX_NONE) && (CopySpace.IsValidToEvaluate(RequiredBones) == false))
		{
			return false;
		}
	}

	return true;
}

void FAnimNode_CopyMotion::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	Super::InitializeBoneReferences(RequiredBones);

	BoneToModify.Initialize(RequiredBones);
	SourceBone.Initialize(RequiredBones);
	CopySpace.Initialize(RequiredBones);
	ApplySpace.Initialize(RequiredBones);
}

float FAnimNode_CopyMotion::GetTargetCurveValue(const FTransformNoScale& InTransformDelta, ECopyMotion_Component MotionComponent)
{
	if (MotionComponent == ECopyMotion_Component::RotationAngle)
	{
		const FVector TwistAxis = UE::Anim::CopyMotion::GetAxisVector(TargetCurveRotationAxis);
		const float SignedAngle = UE::Anim::CopyMotion::GetSignedAngleAroundAxis(InTransformDelta.Rotation, TwistAxis);
		return FMath::RadiansToDegrees(SignedAngle);
	}
	else
	{
		return InTransformDelta.Location[static_cast<int32>(MotionComponent)];
	}
}

bool FAnimNode_CopyMotion::HasTargetBone(const FBoneContainer& BoneContainer) const
{
	return BoneToModify.IsValidToEvaluate(BoneContainer);
}

bool FAnimNode_CopyMotion::HasTargetCurve() const
{
	return TargetCurveName != NAME_None;
}

void FAnimNode_CopyMotion::Reset()
{
	bIsFirstUpdate = true;
}
