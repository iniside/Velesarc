// ArcEditorEntityDrawComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Debug/DebugDrawComponent.h"
#include "DebugRenderSceneProxy.h"
#include "ArcEditorEntityDrawComponent.generated.h"

struct FArcEditorCircle
{
	FVector Center = FVector::ZeroVector;
	float Radius = 0.f;
	int32 Segments = 32;
	FVector YAxis = FVector::YAxisVector;
	FVector ZAxis = FVector::ZAxisVector;
	FColor Color = FColor::White;
	float Thickness = 1.f;
};

struct FArcEditorArrow
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	FColor Color = FColor::White;
	float ArrowSize = 40.f;
	float Thickness = 2.f;
};

struct FArcEditorDrawDelegateHelper : public FDebugDrawDelegateHelper
{
	typedef FDebugDrawDelegateHelper Super;

public:
	void SetupFromProxy(const FDebugRenderSceneProxy* InSceneProxy)
	{
		Super::InitDelegateHelper(InSceneProxy);
	}

	void Reset()
	{
		ResetTexts();
	}

protected:
	virtual void DrawDebugLabels(UCanvas* Canvas, APlayerController* PC) override
	{
		Super::DrawDebugLabels(Canvas, PC);
	}
};

UCLASS()
class ARCMASSEDITOR_API UArcEditorEntityDrawComponent : public UDebugDrawComponent
{
	GENERATED_BODY()

public:
	UArcEditorEntityDrawComponent();

	void ClearShapes();

	void AddLine(const FVector& Start, const FVector& End, const FColor& Color, float Thickness = 1.f);
	void AddSphere(const FVector& Center, float Radius, const FColor& Color);
	void AddCircle(const FVector& Center, float Radius, const FVector& YAxis, const FVector& ZAxis, const FColor& Color, float Thickness = 2.f, int32 Segments = 48);
	void AddText(const FVector& Location, const FString& Text, const FColor& Color);
	void AddArrow(const FVector& Start, const FVector& End, const FColor& Color, float ArrowSize = 40.f, float Thickness = 3.f);

	void FinishCollecting();

protected:
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

#if UE_ENABLE_DEBUG_DRAWING
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
	virtual FDebugDrawDelegateHelper& GetDebugDrawDelegateHelper() override { return DebugDrawDelegateHelper; }
	FArcEditorDrawDelegateHelper DebugDrawDelegateHelper;
#endif

private:
	TArray<FDebugRenderSceneProxy::FDebugLine> StoredLines;
	TArray<FDebugRenderSceneProxy::FSphere> StoredSpheres;
	TArray<FDebugRenderSceneProxy::FText3d> StoredTexts;
	TArray<FArcEditorArrow> StoredArrows;
	TArray<FArcEditorCircle> StoredCircles;
};
