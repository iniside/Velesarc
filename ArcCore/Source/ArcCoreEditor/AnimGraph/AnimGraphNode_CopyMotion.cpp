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

#include "AnimGraph/AnimGraphNode_CopyMotion.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "DetailLayoutBuilder.h"
#include "PropertyHandle.h"
#include "UnrealWidgetFwd.h"
#include "ScopedTransaction.h"
#include "MotionExtractorUtilities.h"
#include "MotionExtractorTypes.h"

#define LOCTEXT_NAMESPACE "AnimationLayering"

FText UAnimGraphNode_CopyMotion::GetControllerDescription() const
{
	return LOCTEXT("CopyMotion", "Copy Motion");
}

FText UAnimGraphNode_CopyMotion::GetTooltipText() const
{
	return LOCTEXT("Copy Motion", "Applies motion from a source, such as a curve, to a bone");
}

FText UAnimGraphNode_CopyMotion::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FLinearColor UAnimGraphNode_CopyMotion::GetNodeTitleColor() const
{
	return FLinearColor(FColor(153.f, 0.f, 0.f));
}

void UAnimGraphNode_CopyMotion::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const
{
	Super::CustomizePinData(Pin, SourcePropertyName, ArrayIndex);
}

void UAnimGraphNode_CopyMotion::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
}

void UAnimGraphNode_CopyMotion::CopyNodeDataToPreviewNode(FAnimNode_Base* InPreviewNode)
{
	FAnimNode_CopyMotion* CopyMotionNode = static_cast<FAnimNode_CopyMotion*>(InPreviewNode);

	// copies Pin values from the internal node to get data which are not compiled yet
	CopyMotionNode->TranslationScale = Node.TranslationScale;
	CopyMotionNode->TranslationRemapCurve = Node.TranslationRemapCurve;
	CopyMotionNode->TranslationOffset = Node.TranslationOffset;

	CopyMotionNode->RotationOffset = Node.RotationOffset;
	CopyMotionNode->RotationPivot = Node.RotationPivot;
	CopyMotionNode->RotationScale = Node.RotationScale;
	CopyMotionNode->RotationRemapCurve = Node.RotationRemapCurve;

	CopyMotionNode->Delay = Node.Delay;
}

void UAnimGraphNode_CopyMotion::CopyPinDefaultsToNodeData(UEdGraphPin* InPin)
{
	if (InPin->GetName() == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_CopyMotion, TranslationScale))
	{
		GetDefaultValue(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_CopyMotion, TranslationScale), Node.TranslationScale);
	}
	else if (InPin->GetName() == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_CopyMotion, TranslationOffset))
	{
		GetDefaultValue(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_CopyMotion, TranslationOffset), Node.TranslationOffset);
	}
	else if (InPin->GetName() == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_CopyMotion, RotationScale))
	{
		Node.RotationScale = FCString::Atof(*InPin->GetDefaultAsString());
	}
	else if (InPin->GetName() == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_CopyMotion, RotationOffset))
	{
		GetDefaultValue(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_CopyMotion, RotationOffset), Node.RotationOffset);
	}
	else if (InPin->GetName() == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_CopyMotion, RotationPivot))
	{
		GetDefaultValue(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_CopyMotion, RotationPivot), Node.RotationPivot);
	}
}

void UAnimGraphNode_CopyMotion::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	bool bShouldRebuildChain = false;

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FAnimNode_CopyMotion, CurvePrefix))
	{
		FScopedTransaction Transaction(LOCTEXT("AnimGraphNode_ChangeCurvePrefix", "Change Curve Prefix"));
		Modify();

		// If the CurvePrefix changes, bake the new curve names on the node based on CurvePreflix w/ the format specified in LayeringMotionExtractorModifier
		Node.TranslationX_CurveName = UMotionExtractorUtilityLibrary::GenerateCurveName(Node.CurvePrefix, EMotionExtractor_MotionType::Translation, EMotionExtractor_Axis::X);
		Node.TranslationY_CurveName = UMotionExtractorUtilityLibrary::GenerateCurveName(Node.CurvePrefix, EMotionExtractor_MotionType::Translation, EMotionExtractor_Axis::Y);
		Node.TranslationZ_CurveName = UMotionExtractorUtilityLibrary::GenerateCurveName(Node.CurvePrefix, EMotionExtractor_MotionType::Translation, EMotionExtractor_Axis::Z);
		Node.RotationRoll_CurveName = UMotionExtractorUtilityLibrary::GenerateCurveName(Node.CurvePrefix, EMotionExtractor_MotionType::Rotation, EMotionExtractor_Axis::X);
		Node.RotationPitch_CurveName = UMotionExtractorUtilityLibrary::GenerateCurveName(Node.CurvePrefix, EMotionExtractor_MotionType::Rotation, EMotionExtractor_Axis::Y);
		Node.RotationYaw_CurveName = UMotionExtractorUtilityLibrary::GenerateCurveName(Node.CurvePrefix, EMotionExtractor_MotionType::Rotation, EMotionExtractor_Axis::Z);

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#undef LOCTEXT_NAMESPACE
