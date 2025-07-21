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

#pragma once

#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_CopyBoneAdvanced.generated.h"

/**
 *	Controller to copy a bone's transform to another one, with individual per-component alphas.
 */
USTRUCT(BlueprintInternalUseOnly)
struct ARCCORE_API FAnimNode_CopyBoneAdvanced : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	/** Source Bone Name to get transform from */
	UPROPERTY(EditAnywhere, Category = Copy)
	FBoneReference SourceBone;

	/** Name of bone to control. This is the main bone chain to modify from. */
	UPROPERTY(EditAnywhere, Category=Copy) 
	FBoneReference TargetBone;

	/** Per-axis translation weight to copy from source to target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Copy, meta = (AllowPreserveRatio, PinHiddenByDefault))
	FVector TranslationWeight;

	/** Rotation angle weight to copy from source to target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Copy, meta = (PinHiddenByDefault))
	float RotationWeight;

	/** Scale weight to copy from source to target (not separated per-axis to avoid non-uniform scales) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Copy, meta = (AllowPreserveRatio, PinHiddenByDefault))
	float ScaleWeight;

	/** Space to convert transforms into prior to copying components */
	UPROPERTY(EditAnywhere, Category = Copy)
	TEnumAsByte<EBoneControlSpace> ControlSpace;

	/** Name of the bone Translation Weight is relative to.
		If left empty, the axes are in source bone space.
		If unchecked, the axes are in component space. */
	UPROPERTY(EditAnywhere, Category=Copy, meta = (EditCondition = "bTranslationInCustomBoneSpace")) 
	FBoneReference TranslationSpaceBone;

	// If disabled, the translation weight axes are in component-space
	UPROPERTY(EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bTranslationInCustomBoneSpace;

	// If disabled, the translation weight axes are in component-space
	UPROPERTY(EditAnywhere, Category = Advanced)
	bool bPropagateToChildren;

	FAnimNode_CopyBoneAdvanced();

	// FAnimNode_Base interface
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

private:
	// FAnimNode_SkeletalControlBase interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	TArray<FCompactPoseBoneIndex> ChildBoneIndices;
};
