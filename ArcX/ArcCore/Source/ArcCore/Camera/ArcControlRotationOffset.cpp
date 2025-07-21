#include "ArcControlRotationOffset.h"

#include "Build/CameraObjectBuildContext.h"
#include "Core/BuiltInCameraVariables.h"
#include "Core/CameraEvaluationContext.h"
#include "Player/ArcCorePlayerController.h"

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
