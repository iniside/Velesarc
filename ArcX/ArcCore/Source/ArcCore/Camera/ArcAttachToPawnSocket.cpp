#include "ArcAttachToPawnSocket.h"

#include "Components/SkeletalMeshComponent.h"
#include "Core/CameraEvaluationContext.h"
#include "Core/CameraValueInterpolator.h"
#include "GameFramework/Character.h"

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
			SetNodeEvaluatorFlags(ECameraNodeEvaluatorFlags::None);
			
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