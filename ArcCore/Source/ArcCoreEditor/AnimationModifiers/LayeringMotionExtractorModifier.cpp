/**
 * This file is part of ArcX.
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

#include "AnimationModifiers/LayeringMotionExtractorModifier.h"
#include "Animation/Skeleton.h"
#include "MotionExtractorUtilities.h"
#include "Animation/AnimSequence.h"
#include "AnimationBlueprintLibrary.h"
#include "DSP/PassiveFilter.h"
#include "EngineLogs.h"
#include "MotionExtractorTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LayeringMotionExtractorModifier)

namespace LayeringMotionExtractor
{
	struct FNameKeys
	{
		FName Name;
		TArray<FRichCurveKey> Keys;
	};

	FNameKeys ApplyHelper(ULayeringMotionExtractorModifier& Modifier, UAnimSequence* Animation, EMotionExtractor_MotionType InMotionType, EMotionExtractor_Axis InAxis)
	{
		if (Animation == nullptr)
		{
			UE_LOG(LogAnimation, Error, TEXT("LayeringMotionExtractorModifier failed. Reason: Invalid Animation"));
			return FNameKeys();
		}

		USkeleton* Skeleton = Animation->GetSkeleton();
		if (Skeleton == nullptr)
		{
			UE_LOG(LogAnimation, Error, TEXT("LayeringMotionExtractorModifier failed. Reason: Animation with invalid Skeleton. Animation: %s"),
				*GetNameSafe(Animation));
			return FNameKeys();
		}

		const int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(Modifier.BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			UE_LOG(LogAnimation, Error, TEXT("LayeringMotionExtractorModifier failed. Reason: Invalid Bone Index. BoneName: %s Animation: %s Skeleton: %s"),
				*Modifier.BoneName.ToString(), *GetNameSafe(Animation), *GetNameSafe(Skeleton));
			return FNameKeys();
		}

		FMemMark Mark(FMemStack::Get());

		// Don't disable bForceRootLock. We don't care about the root motion for layering purposes.
		//TGuardValue<bool> ForceRootLockGuard(Animation->bForceRootLock, false);

		TArray<FBoneIndexType> RequiredBones;

		// If the user specified a reference bone space, add it to required bones
		int32 BoneSpaceIndex = INDEX_NONE;
		if (Modifier.BoneSpaceName != NAME_None)
		{
			BoneSpaceIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(Modifier.BoneSpaceName);
			if (BoneSpaceIndex == INDEX_NONE)
			{
				UE_LOG(LogAnimation, Error, TEXT("LayeringMotionExtractorModifier failed. Reason: Invalid Reference Bone Index Index. BoneName: %s Animation: %s Skeleton: %s"),
					*Modifier.BoneSpaceName.ToString(), *GetNameSafe(Animation), *GetNameSafe(Skeleton));
				return FNameKeys();
			}

			RequiredBones.Add(BoneSpaceIndex);
		}

		RequiredBones.Add(BoneIndex);
		Skeleton->GetReferenceSkeleton().EnsureParentsExistAndSort(RequiredBones);

		FBoneContainer BoneContainer(RequiredBones, UE::Anim::ECurveFilterMode::DisallowAll, *Skeleton);
		const bool bComponentSpace = true;

		const FCompactPoseBoneIndex CompactPoseBoneIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(BoneIndex));
		const FTransform FirstFrameBoneTransform = UMotionExtractorUtilityLibrary::ExtractBoneTransform(Animation, BoneContainer, CompactPoseBoneIndex, 0.f, bComponentSpace);
		
		// If we have a reference bone space, grab its first frame transform
		FCompactPoseBoneIndex CompactPoseBoneSpaceIndex(INDEX_NONE);
		FTransform FirstFrameBoneSpaceTransform;
		if (BoneSpaceIndex != INDEX_NONE)
		{
			CompactPoseBoneSpaceIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(BoneSpaceIndex));
			FirstFrameBoneSpaceTransform = UMotionExtractorUtilityLibrary::ExtractBoneTransform(Animation, BoneContainer, CompactPoseBoneSpaceIndex, 0.f, bComponentSpace);
		}

		const float AnimLength = Animation->GetPlayLength();
		const float SampleInterval = 1.f / Modifier.SampleRate;

		FTransform LastBoneTransform = FTransform::Identity;
		float Time = 0.f;
		int32 SampleIndex = 0;


		// @todo -- ac: this just gets last transform and max value (could get min/max of each value?)
		float MaxValue = -MAX_FLT;
		if(Modifier.bNormalize)
		{
			while (Time < AnimLength)
			{
				Time = FMath::Clamp(SampleIndex * SampleInterval, 0.f, AnimLength);
				SampleIndex++;

				FTransform BoneTransform = UMotionExtractorUtilityLibrary::ExtractBoneTransform(Animation, BoneContainer, CompactPoseBoneIndex, Time, bComponentSpace);

				if (BoneSpaceIndex != INDEX_NONE)
				{
					FTransform BoneSpaceTransform = UMotionExtractorUtilityLibrary::ExtractBoneTransform(Animation, BoneContainer, CompactPoseBoneSpaceIndex, Time, bComponentSpace);
					const FTransform RefToSrc = BoneTransform.GetRelativeTransform(BoneSpaceTransform);
					const FTransform RefToSrcRefPose = FirstFrameBoneTransform.GetRelativeTransform(FirstFrameBoneSpaceTransform);

					// Calculate the source bone delta in reference bone space
					BoneTransform = RefToSrc.GetRelativeTransform(RefToSrcRefPose);
				}
				else
				{
					// Store the transform as additive if using it relative to the first frame.
					FAnimationRuntime::ConvertTransformToAdditive(BoneTransform, FirstFrameBoneTransform);
				}

				// Ignore first frame if we are extracting something that depends on the previous bone transform
				if (SampleIndex > 1 || (InMotionType != EMotionExtractor_MotionType::TranslationSpeed))
				{
					const float Value = Modifier.GetDesiredValue(BoneTransform, LastBoneTransform, SampleInterval, InMotionType, InAxis);
					MaxValue = FMath::Max(FMath::Abs(Value), MaxValue);
				}

				LastBoneTransform = BoneTransform;
			}
		}

		LastBoneTransform = FTransform::Identity;
		Time = 0.f;
		SampleIndex = 0;

		TArray<FRichCurveKey> CurveKeys;

		while (Time < AnimLength)
		{
			Time = FMath::Clamp(SampleIndex * SampleInterval, 0.f, AnimLength);
			SampleIndex++;

			FTransform BoneTransform = UMotionExtractorUtilityLibrary::ExtractBoneTransform(Animation, BoneContainer, CompactPoseBoneIndex, Time, bComponentSpace);

			if (BoneSpaceIndex != INDEX_NONE)
			{
				FTransform BoneSpaceTransform = UMotionExtractorUtilityLibrary::ExtractBoneTransform(Animation, BoneContainer, CompactPoseBoneSpaceIndex, Time, bComponentSpace);
				const FTransform RefToSrc = BoneTransform.GetRelativeTransform(BoneSpaceTransform);
				const FTransform RefToSrcRefPose = FirstFrameBoneTransform.GetRelativeTransform(FirstFrameBoneSpaceTransform);

				// Calculate the source bone delta in reference bone space
				BoneTransform = RefToSrc.GetRelativeTransform(RefToSrcRefPose);
			}
			else
			{
				// Store the transform as additive if using it relative to the first frame.
				FAnimationRuntime::ConvertTransformToAdditive(BoneTransform, FirstFrameBoneTransform);
			}

			// Ignore first frame if we are extracting something that depends on the previous bone transform
			if (SampleIndex > 1 || (InMotionType != EMotionExtractor_MotionType::TranslationSpeed))
			{
				const float Value = Modifier.GetDesiredValue(BoneTransform, LastBoneTransform, SampleInterval, InMotionType, InAxis);
				const float FinalValue = (Modifier.bNormalize && MaxValue != 0.f) ? FMath::Abs(Value) / MaxValue : Value;

				FRichCurveKey& Key = CurveKeys.AddDefaulted_GetRef();
				Key.Time = Time;
				Key.Value = FinalValue;
			}

			LastBoneTransform = BoneTransform;
		}

		// @todo: This crashes with certain orders, and certain configurations of curves. Figure out why.
		if (Modifier.bApplyFilter)
		{
			Audio::FPassiveFilterParams Params;
			Params.Type = Modifier.bHighPass ? Audio::FPassiveFilterParams::EType::Highpass : Audio::FPassiveFilterParams::EType::Lowpass; // Enum Conversion
			Params.Class = Modifier.bButterworth ? Audio::FPassiveFilterParams::EClass::Butterworth : Audio::FPassiveFilterParams::EClass::Chebyshev; // Enum Conversion
			Params.NormalizedCutoffFrequency = Modifier.CutoffFrequency;
			Params.Order = Modifier.Order;
			Params.bScaleByOffset = false;

			// todo: can we keep as array of floats until final conversion?
			TArray<float> CurveValues;
			float MinCurveValue = FLT_MAX;
			float MaxCurveValue = FLT_MIN;
			CurveValues.Reserve(CurveKeys.Num());
			for (int32 Index = 0; Index < CurveKeys.Num(); ++Index)
			{
				CurveValues.Add(CurveKeys[Index].Value);
				MinCurveValue = FMath::Min(CurveValues[Index], MinCurveValue);
				MaxCurveValue = FMath::Max(CurveValues[Index], MaxCurveValue);
			}

			// We have to offset our samples to center them around the 0 value as the filtering code expects that.
			double ValueOffset = MinCurveValue + ((MaxCurveValue - MinCurveValue) / 2.0);
			for (int32 Index = 0; Index < CurveKeys.Num(); ++Index)
			{
				CurveValues[Index] -= ValueOffset;
			}

			Audio::Filter(CurveValues, Params);

			for (int32 Index = 0; Index < CurveKeys.Num(); ++Index)
			{
				CurveKeys[Index].Value = CurveValues[Index] + ValueOffset;
			}
		}

		return FNameKeys{ UMotionExtractorUtilityLibrary::GenerateCurveName(Modifier.GetCurvePrefix(), InMotionType, InAxis), CurveKeys };
	}

	TArray<EMotionExtractor_Axis> AxisToAxisArray(const EMotionExtractor_Axis InAxis)
	{
		TArray<EMotionExtractor_Axis> OutAxes;
		// @todo: change this to use bit flags
		if (InAxis == EMotionExtractor_Axis::XYZ)
		{
			OutAxes.Add(EMotionExtractor_Axis::X);
			OutAxes.Add(EMotionExtractor_Axis::Y);
			OutAxes.Add(EMotionExtractor_Axis::Z);
		}
		else if (InAxis == EMotionExtractor_Axis::XY)
		{
			OutAxes.Add(EMotionExtractor_Axis::X);
			OutAxes.Add(EMotionExtractor_Axis::Y);
		}
		else if (InAxis == EMotionExtractor_Axis::XZ)
		{
			OutAxes.Add(EMotionExtractor_Axis::X);
			OutAxes.Add(EMotionExtractor_Axis::Z);
		}
		else if (InAxis == EMotionExtractor_Axis::YZ)
		{
			OutAxes.Add(EMotionExtractor_Axis::Y);
			OutAxes.Add(EMotionExtractor_Axis::Z);
		}
		else
		{
			OutAxes.Add(InAxis);
		}

		return OutAxes;
	}
}






ULayeringMotionExtractorModifier::ULayeringMotionExtractorModifier()
	:Super()
{
	// Note: These are the expected default values to use internally.
	BoneName = FName(TEXT("spine_05"));
	MotionTypes = (int32)(EMotionExtractor_MotionType::Translation | EMotionExtractor_MotionType::Rotation);
	Axis = EMotionExtractor_Axis::XYZ;
	bAbsoluteValue = false;
	bNormalize = false;
	bUseCustomCurveName = false;
	CustomCurveName = NAME_None;
	SampleRate = 30;
	bRemoveCurveOnRevert = true;
}

void ULayeringMotionExtractorModifier::OnApply_Implementation(UAnimSequence* Animation)
{
	TArray<EMotionExtractor_Axis> AxesToDo = LayeringMotionExtractor::AxisToAxisArray(Axis);

	TArray<LayeringMotionExtractor::FNameKeys> Results;
	TArray<EMotionExtractor_MotionType> MotionTypesArray;
	for (EMotionExtractor_MotionType MotionTypeToDo : TEnumRange<EMotionExtractor_MotionType>())
	{
		if (EnumHasAnyFlags(MotionTypes, MotionTypeToDo))
		{
			for (EMotionExtractor_Axis AxisToDo : AxesToDo)
			{
				Results.Add(LayeringMotionExtractor::ApplyHelper(*this, Animation, MotionTypeToDo, AxisToDo));
			}
		}
	}

	for (LayeringMotionExtractor::FNameKeys& Result : Results)
	{
		IAnimationDataController& Controller = Animation->GetController();
		const FAnimationCurveIdentifier CurveId = UAnimationCurveIdentifierExtensions::GetCurveIdentifier(Animation->GetSkeleton(), Result.Name, ERawCurveTrackTypes::RCT_Float);
		if (Result.Keys.Num() && Controller.AddCurve(CurveId))
		{
			Controller.SetCurveKeys(CurveId, Result.Keys);
		}
	}
}

void ULayeringMotionExtractorModifier::OnRevert_Implementation(UAnimSequence* Animation)
{
	if (bRemoveCurveOnRevert)
	{
		TArray<EMotionExtractor_Axis> AxesToDo = LayeringMotionExtractor::AxisToAxisArray(Axis);

		for (EMotionExtractor_MotionType MotionTypeToDo : TEnumRange<EMotionExtractor_MotionType>())
		{
			if (EnumHasAnyFlags(MotionTypes, MotionTypeToDo))
			{
				for (EMotionExtractor_Axis AxisToDo : AxesToDo)
				{
					const FName FinalCurveName = UMotionExtractorUtilityLibrary::GenerateCurveName(GetCurvePrefix(), MotionTypeToDo, AxisToDo);
					const bool bRemoveNameFromSkeleton = false;
					UAnimationBlueprintLibrary::RemoveCurve(Animation, FinalCurveName, bRemoveNameFromSkeleton);
				}
			}
		}
	}
}

FName ULayeringMotionExtractorModifier::GetCurvePrefix() const
{
	if (bUseCustomCurveName && !CustomCurveName.IsEqual(NAME_None))
	{
		return CustomCurveName;
	}

	return BoneName;
}

float ULayeringMotionExtractorModifier::GetDesiredValue(const FTransform& BoneTransform, const FTransform& LastBoneTransform, float DeltaTime, EMotionExtractor_MotionType InMotionType, EMotionExtractor_Axis InAxis) const
{
	float Value = UMotionExtractorUtilityLibrary::GetDesiredValue(BoneTransform, LastBoneTransform, DeltaTime, InMotionType, InAxis);

	if (bAbsoluteValue)
	{
		Value = FMath::Abs(Value);
	}

	if (MathOperation != EMotionExtractor_MathOperation::None)
	{
		switch (MathOperation)
		{
		case EMotionExtractor_MathOperation::Addition:		Value = Value + Modifier; break;
		case EMotionExtractor_MathOperation::Subtraction:	Value = Value - Modifier; break;
		case EMotionExtractor_MathOperation::Division:		Value = Value / Modifier; break;
		case EMotionExtractor_MathOperation::Multiplication: Value = Value * Modifier; break;
		default: check(false); break;
		}
	}

	return Value;
}
