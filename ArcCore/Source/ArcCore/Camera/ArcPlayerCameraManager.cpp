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

#include "ArcCore/Camera/ArcPlayerCameraManager.h"

#include "EnhancedInputComponent.h"
#include "ArcCore/Camera/ArcCameraComponent.h"
#include "ArcCore/Camera/ArcUICameraManagerComponent.h"
#include "Build/CameraObjectBuildContext.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveVector.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameplayCameraComponent.h"
#include "GameFramework/GameplayCameraSystemComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Nodes/Input/Input2DCameraNode.h"

#include "Core/CameraEvaluationContext.h"
#include "Core/CameraParameterReader.h"
#include "Core/CameraSystemEvaluator.h"
#include "Core/CameraValueInterpolator.h"
#include "Math/CameraNodeSpaceMath.h"
#include "Nodes/Input/InputAxisBinding2DCameraNode.h"
#include "Player/ArcCorePlayerController.h"

static FName UICameraComponentName(TEXT("UICamera"));

AArcPlayerCameraManager::AArcPlayerCameraManager(const FObjectInitializer& ObjectInitializer)
	: Super{ObjectInitializer}
{
	DefaultFOV = 90.f;
	ViewPitchMin = -70.f;
	ViewPitchMax = 60.f;

	UICamera = CreateDefaultSubobject<UArcUICameraManagerComponent>(UICameraComponentName);
}

UArcUICameraManagerComponent* AArcPlayerCameraManager::GetUICameraComponent() const
{
	return UICamera;
}

void AArcPlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT
											   , float DeltaTime)
{
	// If the UI Camera is looking at something, let it have priority.
	if (UICamera->NeedsToUpdateViewTarget())
	{
		Super::UpdateViewTarget(OutVT, DeltaTime);
		UICamera->UpdateViewTarget(OutVT, DeltaTime);
		
		return;
	}
		
	Super::UpdateViewTarget(OutVT, DeltaTime);
}

void AArcPlayerCameraManager::DisplayDebug(UCanvas* Canvas
										   , const FDebugDisplayInfo& DebugDisplay
										   , float& YL
										   , float& YPos)
{
	check(Canvas);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;

	DisplayDebugManager.SetFont(GEngine->GetSmallFont());
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("ArcPlayerCameraManager: %s")
		, *GetNameSafe(this)));

	Super::DisplayDebug(Canvas
		, DebugDisplay
		, YL
		, YPos);

	const APawn* Pawn = (PCOwner ? PCOwner->GetPawn() : nullptr);

	if (const UArcCameraComponent* CameraComponent = UArcCameraComponent::FindCameraComponent(Pawn))
	{
		CameraComponent->DrawDebug(Canvas);
	}
}

#if 1
namespace UE::Cameras
{
	class FArcAttachToPawnSocketEvaluator : public UE::Cameras::FCameraNodeEvaluator
	{
		UE_DECLARE_CAMERA_NODE_EVALUATOR(ARCCORE_API, FArcAttachToPawnSocketEvaluator)

	public:
		FVector TargetLocation;
		
	protected:
		FName SocketName = NAME_Name;
		FName ComponentTag = NAME_None;
		
		bool bUsePawnBaseEyeHeight = false;
		
		float LastDampedPushFactor = 0.f;
		
		TUniquePtr<FCameraVector2dValueInterpolator> OffsetInterpolator;
		TUniquePtr<FCameraDoubleValueInterpolator> OffsetInterpolatorZ;
	public:
		virtual void OnInitialize(const FCameraNodeEvaluatorInitializeParams& Params, FCameraNodeEvaluationResult& OutResult) override
		{
			const UArcAttachToPawnSocket* AttachNode = GetCameraNodeAs<UArcAttachToPawnSocket>();
			SocketName = AttachNode->SocketName;
			ComponentTag = AttachNode->ComponentTag;
			bUsePawnBaseEyeHeight = AttachNode->bUsePawnBaseEyeHeight;
			
			OffsetInterpolator = AttachNode->OffsetInterpolator ?
				AttachNode->OffsetInterpolator->BuildVector2dInterpolator() :
				MakeUnique<TPopValueInterpolator<FVector2d>>();

			OffsetInterpolatorZ = AttachNode->OffsetInterpolatorZ ?
				AttachNode->OffsetInterpolatorZ->BuildDoubleInterpolator() :
				MakeUnique<TPopValueInterpolator<double>>();
		}
		
		virtual void OnRun(const FCameraNodeEvaluationParams& Params, FCameraNodeEvaluationResult& OutResult) override
		{
			APlayerController* PC = Params.EvaluationContext->GetPlayerController();
			if (!PC)
			{
				return;
			}

			const APawn* Pawn = PC->GetPawnOrSpectator();
			if (!Pawn)
			{
				return;
			}

			FVector OwnerLocation = Pawn->GetActorLocation();
			
			if (const ACharacter* TargetCharacter = Cast<ACharacter>(PC->GetPawn()))
			{
				if (ComponentTag.IsNone())
				{
					return;
				}
				
				USkeletalMeshComponent* SMC = TargetCharacter->FindComponentByTag<USkeletalMeshComponent>(ComponentTag);
				if (SocketName != NAME_None)
				{
					FTransform CompSpaceTM = SMC->GetSocketTransform(SocketName
						, ERelativeTransformSpace::RTS_Component);

					FTransform RootCompSpaceTM = SMC->GetSocketTransform("root"
						, ERelativeTransformSpace::RTS_Component);

					const FVector RootCompSpaceLoc = CompSpaceTM.GetLocation() - RootCompSpaceTM.GetLocation();
					FTransform WorldSpaceTM = SMC->GetSocketTransform(SocketName
						, ERelativeTransformSpace::RTS_World);
				
					FVector ActorLoc = TargetCharacter->GetActorLocation();
					//OwnerLocation.Y += CompSpaceTM.GetLocation().Y;
					OwnerLocation.Z = WorldSpaceTM.GetLocation().Z;
					if (bUsePawnBaseEyeHeight)
					{
						OwnerLocation.Z += TargetCharacter->BaseEyeHeight;
					}
					
					//PivotSocketTransform.SetLocation(ActorLoc);
				
				}
			}

			FCameraValueInterpolationParams InterpParams;
			InterpParams.bIsCameraCut = Params.bIsFirstFrame;
			InterpParams.DeltaTime = Params.DeltaTime;

			{
				FCameraValueInterpolationResult InterpResult(OutResult.VariableTable);
				FVector2d Target(OwnerLocation.X, OwnerLocation.Y);
				FVector2d Current(TargetLocation.X, TargetLocation.Y);
				
				OffsetInterpolator->Reset(Current, Target);

				Target = OffsetInterpolator->Run(InterpParams, InterpResult);

				TargetLocation.X = Target.X;
				TargetLocation.Y = Target.Y;
			}
			{
				FCameraValueInterpolationResult InterpResult(OutResult.VariableTable);
				OffsetInterpolatorZ->Reset(TargetLocation.Z, OwnerLocation.Z);
				TargetLocation.Z = OffsetInterpolatorZ->Run(InterpParams, InterpResult);
			}
			
			OutResult.CameraPose.SetLocation(TargetLocation);
		}

	};
	UE_DEFINE_CAMERA_NODE_EVALUATOR(FArcAttachToPawnSocketEvaluator)
}
#endif
	

FCameraNodeEvaluatorPtr UArcAttachToPawnSocket::OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const
{
	return Builder.BuildEvaluator<UE::Cameras::FArcAttachToPawnSocketEvaluator>();
}

namespace UE::Cameras
{
	class FArcControlRotationOffsetEvaluator : public UE::Cameras::FInput2DCameraNodeEvaluator
	{
		UE_DECLARE_CAMERA_NODE_EVALUATOR_EX(ARCCORE_API, FArcControlRotationOffsetEvaluator, FInput2DCameraNodeEvaluator)

		FInput2DCameraNodeEvaluator* InputSlotEvaluator = nullptr;

		FVector2D RotationOffset;
		
	public:
		FArcControlRotationOffsetEvaluator()
		{
			SetNodeEvaluatorFlags(
				ECameraNodeEvaluatorFlags::NeedsParameterUpdate);
		}
		virtual void OnBuild(const FCameraNodeEvaluatorBuildParams& Params) override
		{
			Super::OnBuild(Params);
			
			const UArcControlRotationOffset* SlotNode = GetCameraNodeAs<UArcControlRotationOffset>();
			InputSlotEvaluator = Params.BuildEvaluatorAs<FInput2DCameraNodeEvaluator>(SlotNode->InputSlot);
		}

		virtual void OnInitialize(const FCameraNodeEvaluatorInitializeParams& Params, FCameraNodeEvaluationResult& OutResult) override
		{
			Super::OnInitialize(Params, OutResult);
			RotationOffset = FVector2D::ZeroVector;

			const UArcControlRotationOffset* SlotNode = GetCameraNodeAs<UArcControlRotationOffset>();
			
			if (SlotNode && SlotNode->GetVariableID().IsValid() && Params.LastActiveCameraRigInfo.LastResult)
			{
				const FCameraVariableTable& LastActiveRigVariableTable = Params.LastActiveCameraRigInfo.LastResult->VariableTable;
				LastActiveRigVariableTable.TryGetValue<FVector2d>(SlotNode->GetVariableID(), InputValue);
			}
		}
		
		virtual FCameraNodeEvaluatorChildrenView OnGetChildren() override
		{
			return FCameraNodeEvaluatorChildrenView{ InputSlotEvaluator };
		}

		virtual void OnUpdateParameters(const FCameraBlendedParameterUpdateParams& Params, FCameraBlendedParameterUpdateResult& OutResult) override
		{
			//if (InputSlotEvaluator)
			//{
			//	InputSlotEvaluator->UpdateParameters(Params, OutResult);
			//	
			//}
			Super::OnUpdateParameters(Params, OutResult);
		}

		
		virtual void OnRun(const FCameraNodeEvaluationParams& Params, FCameraNodeEvaluationResult& OutResult) override
		{
			if (InputSlotEvaluator)
			{
				InputSlotEvaluator->Run(Params, OutResult);
				InputValue = InputSlotEvaluator->GetInputValue();
			}

			AArcCorePlayerController* PC = Cast<AArcCorePlayerController>(Params.EvaluationContext->GetPlayerController());
			if (PC)
			{
				RotationOffset.X = PC->ControlRotationOffset.Yaw;
				RotationOffset.Y = PC->ControlRotationOffset.Pitch;
			}
			
			FVector2D Final = (RotationOffset + InputValue);// * Params.DeltaTime;
			InputValue += (RotationOffset * Params.DeltaTime);

			const UArcControlRotationOffset* SlotNode = GetCameraNodeAs<UArcControlRotationOffset>();
			if (SlotNode && RotationOffset.IsNearlyZero(0.01))
			{
				//InputSlotEvaluator->SetInputValue(InputValue);
				//OutResult.VariableTable.SetValue<FVector2d>(SlotNode->GetVariableID(), InputValue);
			}

			UE_LOG(LogTemp, Log, TEXT("2 Control Rotation Offset: %s InputValue %s"), *RotationOffset.ToString(), *InputValue.ToString());	
		}
		//virtual void OnSerialize(const FCameraNodeEvaluatorSerializeParams& Params, FArchive& Ar) override
		//{
		//	Super::OnSerialize(Params, Ar);
		//
		//	Ar << RotationOffset;
		//	Ar << DeltaInputValue;
		//}
		
		//virtual void OnExecuteOperation(const FCameraOperationParams& Params, FCameraOperation& Operation) override
		//{
		//	if (FYawPitchCameraOperation* Op = Operation.CastOperation<FYawPitchCameraOperation>())
		//	{
		//		const UCameraRigInput2DSlot* SlotNode = GetCameraNodeAs<UCameraRigInput2DSlot>();
		//
		//		double MinValueX, MaxValueX;
		//		SlotNode->ClampX.GetEffectiveClamping(MinValueX, MaxValueX);
		//
		//		double MinValueY, MaxValueY;
		//		SlotNode->ClampY.GetEffectiveClamping(MinValueY, MaxValueY);
		//
		//		InputValue.X = Op->Yaw.Apply(InputValue.X, MinValueX, MaxValueX);
		//		InputValue.Y = Op->Pitch.Apply(InputValue.Y, MinValueY, MaxValueY);
		//	}
		//}
	};
	UE_DEFINE_CAMERA_NODE_EVALUATOR(FArcControlRotationOffsetEvaluator)
}

void UArcControlRotationOffset::OnBuild(FCameraObjectBuildContext& BuildContext)
{
	Super::OnBuild(BuildContext);

	using namespace UE::Cameras;
	
	FCameraVariableDefinition VariableDefinition;

	VariableDefinition = FBuiltInCameraVariables::Get().GetDefinition(EBuiltInVector2dCameraVariable::YawPitch);
	

	if (VariableDefinition.IsValid())
	{
		VariableDefinition.bIsInput = true;

		FCameraVariableTableAllocationInfo& VariableTableInfo = BuildContext.AllocationInfo.VariableTableInfo;
		VariableTableInfo.VariableDefinitions.Add(VariableDefinition);

		FCameraVariableDefinition SpeedVariableDefinition = VariableDefinition.CreateVariant(TEXT("Speed"));
		VariableTableInfo.VariableDefinitions.Add(SpeedVariableDefinition);

		VariableID = VariableDefinition.VariableID;
	}
}
FCameraNodeEvaluatorPtr UArcControlRotationOffset::OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const
{
	return Builder.BuildEvaluator<UE::Cameras::FArcControlRotationOffsetEvaluator>();
}

namespace UE::Cameras
{

class FArcInputAxisBinding2DCameraNodeEvaluator : public FCameraRigInput2DSlotEvaluator
{
	UE_DECLARE_CAMERA_NODE_EVALUATOR_EX(ARCCORE_API, FArcInputAxisBinding2DCameraNodeEvaluator, FCameraRigInput2DSlotEvaluator)

protected:

	// FCameraNodeEvaluator interface.
	virtual void OnInitialize(const FCameraNodeEvaluatorInitializeParams& Params, FCameraNodeEvaluationResult& OutResult) override;
	virtual void OnRun(const FCameraNodeEvaluationParams& Params, FCameraNodeEvaluationResult& OutResult) override;

private:

	TObjectPtr<UEnhancedInputComponent> InputComponent;

	TCameraParameterReader<FVector2d> MultiplierReader;

	TArray<FEnhancedInputActionValueBinding*> AxisValueBindings;
};

UE_DEFINE_CAMERA_NODE_EVALUATOR(FArcInputAxisBinding2DCameraNodeEvaluator)

void FArcInputAxisBinding2DCameraNodeEvaluator::OnInitialize(const FCameraNodeEvaluatorInitializeParams& Params, FCameraNodeEvaluationResult& OutResult)
{
	UObject* ContextOwner = Params.EvaluationContext->GetOwner();
	if (ContextOwner)
	{
		if (AActor* ContextOwnerActor = Cast<AActor>(ContextOwner))
		{
			InputComponent = Cast<UEnhancedInputComponent>(ContextOwnerActor->InputComponent);
		}
		else if (AActor* OuterActor = ContextOwner->GetTypedOuter<AActor>())
		{
			InputComponent = Cast<UEnhancedInputComponent>(OuterActor->InputComponent);
		}
	}

	const UArcInputAxisBinding2DCameraNode* AxisBindingNode = GetCameraNodeAs<UArcInputAxisBinding2DCameraNode>();

	RevertAxisXReader.Initialize(AxisBindingNode->RevertAxisX);
	RevertAxisYReader.Initialize(AxisBindingNode->RevertAxisY);
	MultiplierReader.Initialize(AxisBindingNode->Multiplier);

	if (InputComponent)
	{
		for (TObjectPtr<UInputAction> AxisAction : AxisBindingNode->AxisActions)
		{
			FEnhancedInputActionValueBinding* AxisValueBinding = &InputComponent->BindActionValue(AxisAction);
			AxisValueBindings.Add(AxisValueBinding);
		}
	}
	else if (Params.Evaluator->GetRole() == ECameraSystemEvaluatorRole::Game)
	{
		//UE_LOG(LogCameraSystem, Error, TEXT("No input component found on context owner '%s' for node '%s' in '%s'."),
		//		*GetNameSafe(ContextOwner), 
		//		*GetNameSafe(AxisBindingNode),
		//		*GetNameSafe(AxisBindingNode ? AxisBindingNode->GetOutermost() : nullptr));
	}

	Super::OnInitialize(Params, OutResult);
}

void FArcInputAxisBinding2DCameraNodeEvaluator::OnRun(const FCameraNodeEvaluationParams& Params, FCameraNodeEvaluationResult& OutResult)
{
	const UArcInputAxisBinding2DCameraNode* AxisBindingNode = GetCameraNodeAs<UArcInputAxisBinding2DCameraNode>();
	
	const UCameraRigInput2DSlot* SlotNode = GetCameraNodeAs<UCameraRigInput2DSlot>();

	FVector2d HighestValue(FVector2d::ZeroVector);
	double HighestSquaredLenth = 0.f;

	for (FEnhancedInputActionValueBinding* AxisValueBinding : AxisValueBindings)
	{
		if (!AxisValueBinding)
		{
			continue;
		}

		const FVector2d Value = AxisValueBinding->GetValue().Get<FVector2D>();
		const double ValueSquaredLength = Value.SquaredLength();
		if (ValueSquaredLength > HighestSquaredLenth)
		{
			HighestValue = Value;
		}
	}

	const FVector2d Multiplier = MultiplierReader.Get(OutResult.VariableTable);
	DeltaInputValue = FVector2d(HighestValue.X * Multiplier.X, HighestValue.Y * Multiplier.Y);
	
	
	const FVector2d Speed = 
		SlotNode->bIsPreBlended ?
			OutResult.VariableTable.GetValue<FVector2d>(SlotNode->GetSpeedVariableID()) :
			SpeedReader.Get(OutResult.VariableTable);

	FVector2d FinalDelta(DeltaInputValue.X * Speed.X * Params.DeltaTime, DeltaInputValue.Y * Speed.Y * Params.DeltaTime);

	
	
	AArcCorePlayerController* PC = Cast<AArcCorePlayerController>(Params.EvaluationContext->GetPlayerController());
	if (PC)
	{
		FinalDelta.X += PC->ControlRotationOffset.Yaw;
		FinalDelta.Y += PC->ControlRotationOffset.Pitch;
		PC->ControlRotationOffset.Yaw = 0;
        PC->ControlRotationOffset.Pitch = 0;
	}
	if (RevertAxisXReader.Get(OutResult.VariableTable))
	{
		FinalDelta.X = -FinalDelta.X;
	}
	if (RevertAxisYReader.Get(OutResult.VariableTable))
	{
		FinalDelta.Y = -FinalDelta.Y;
	}

	//if (bIsAccumulated)
	{
		InputValue += FinalDelta;
	}
	//else
	{
		//	InputValue = FinalDelta;
	}


	
	InputValue.X = SlotNode->NormalizeX.NormalizeValue(InputValue.X);
	InputValue.Y = SlotNode->NormalizeY.NormalizeValue(InputValue.Y);

	InputValue.X = SlotNode->ClampX.ClampValue(InputValue.X);
	InputValue.Y = SlotNode->ClampY.ClampValue(InputValue.Y);
	if (PC)
	{
		if (PC->ClampValue.IsSet())
		{
			if (PC->ClampValue.GetValue() < 0)
			{
				if (InputValue.Y < PC->ClampValue.GetValue())
				{
					InputValue.Y = PC->ClampValue.GetValue();
				}
			}
			else if (PC->ClampValue.GetValue() > 0)
			{
				if (InputValue.Y > PC->ClampValue.GetValue())
				{
					InputValue.Y = PC->ClampValue.GetValue();
				}
			}
		}
		
		PC->InputValue = InputValue;
		
	}
	OutResult.VariableTable.SetValue<FVector2d>(SlotNode->GetVariableID(), InputValue);

	//Super::OnRun(Params, OutResult);
}
}  // namespace UE::Cameras

UArcInputAxisBinding2DCameraNode::UArcInputAxisBinding2DCameraNode(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	Multiplier = FVector2D(1, 1);
}


FCameraNodeEvaluatorPtr UArcInputAxisBinding2DCameraNode::OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const
{
	using namespace UE::Cameras;
	return Builder.BuildEvaluator<FArcInputAxisBinding2DCameraNodeEvaluator>();
}
