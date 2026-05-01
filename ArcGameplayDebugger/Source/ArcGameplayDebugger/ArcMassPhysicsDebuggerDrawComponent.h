// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcDebugDrawComponent.h"
#include "ArcMassPhysicsDebuggerDrawComponent.generated.h"

struct FArcMassPhysicsDebugDrawData
{
	bool bDrawHitLine = false;
	FVector ViewLocation = FVector::ZeroVector;
	FVector HitPoint = FVector::ZeroVector;
	FColor HitSphereColor = FColor::Green;

	bool bDrawBounds = false;
	FVector EntityLocation = FVector::ZeroVector;

	struct FShapeSphere { FVector Center; float Radius; FColor Color; };
	struct FShapeBox { FVector Center; FVector Extent; FQuat Rotation; FColor Color; };
	struct FShapeCapsule { FVector Center; float HalfHeight; float Radius; FVector X; FVector Y; FVector Z; FColor Color; };
	struct FShapeLine { FVector Start; FVector End; FColor Color; };

	TArray<FShapeSphere> CollisionSpheres;
	TArray<FShapeBox> CollisionBoxes;
	TArray<FShapeCapsule> CollisionCapsules;
	TArray<FShapeLine> ConvexLines;
};

UCLASS(ClassGroup = Debug)
class UArcMassPhysicsDebuggerDrawComponent : public UArcDebugDrawComponent
{
	GENERATED_BODY()
public:
	void UpdatePhysicsDebug(const FArcMassPhysicsDebugDrawData& InData);
protected:
	virtual void CollectShapes() override;
private:
	FArcMassPhysicsDebugDrawData Data;
};
