// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcRmlShaders.h"

class FTextureReference;

struct FArcRmlVertex
{
	FVector2f Position;
	uint32 Color = 0;
	FVector2f TexCoord;
};

enum class EArcRmlFilterType : uint8
{
	Opacity,
	Blur,
	DropShadow,
	ColorMatrix
};

struct FArcRmlFilterDesc
{
	EArcRmlFilterType Type = EArcRmlFilterType::Opacity;
	float Opacity = 1.0f;
	float Sigma = 0.0f;
	FVector2f Offset = FVector2f::ZeroVector;
	FVector4f Color = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
	FMatrix44f ColorMatrix = FMatrix44f::Identity;
};

enum class EArcRmlGradientFunction : uint8
{
	Linear,
	Radial,
	Conic,
	RepeatingLinear,
	RepeatingRadial,
	RepeatingConic
};

struct FArcRmlShaderDesc
{
	EArcRmlGradientFunction Function = EArcRmlGradientFunction::Linear;
	FVector2f P = FVector2f::ZeroVector;
	FVector2f V = FVector2f::ZeroVector;
	int32 NumStops = 0;
	FVector4f StopColors[ArcRmlShaders::MaxGradientStops];
	float StopPositions[ArcRmlShaders::MaxGradientStops];
};

enum class EArcRmlResourceOpType : uint8
{
	CreateGeometry,
	ReleaseGeometry,
	CreateTexture,
	CreateAssetTexture,
	ReleaseTexture,
	CreateFilter,
	ReleaseFilter,
	CreateShader,
	ReleaseShader
};

struct FArcRmlResourceOp
{
	EArcRmlResourceOpType Type = EArcRmlResourceOpType::CreateGeometry;
	uint64 Handle = 0;
	int32 PayloadIndex = INDEX_NONE;
};

struct FArcRmlGeometryPayload
{
	TArray<FArcRmlVertex> Vertices;
	TArray<uint32> Indices;
};

struct FArcRmlTexturePayload
{
	FIntPoint Size = FIntPoint::ZeroValue;
	TArray<uint8> Pixels;
};

enum class EArcRmlDrawOp : uint8
{
	RenderGeometry,
	RenderShader,
	SetScissor,
	EnableClipMask,
	RenderToClipMask,
	SetTransform,
	PushLayer,
	PopLayer,
	CompositeLayers,
	SaveLayerAsTexture,
	SaveLayerAsMaskImage
};

enum class EArcRmlBlendMode : uint8
{
	Blend,
	Replace,
	SourceIn,
	SourceOut,
	Xor
};

enum class EArcRmlClipOperation : uint8
{
	Set,
	SetInverse,
	Intersect
};

struct FArcRmlDrawCommand
{
	EArcRmlDrawOp Op = EArcRmlDrawOp::RenderGeometry;
	EArcRmlClipOperation ClipOperation = EArcRmlClipOperation::Set;
	bool bEnable = false;
	EArcRmlBlendMode BlendMode = EArcRmlBlendMode::Blend;
	uint64 Geometry = 0;
	uint64 Texture = 0;
	uint64 Resource = 0;
	FVector2f Translation = FVector2f::ZeroVector;
	FIntRect Rect;
	int32 Index = INDEX_NONE;
	int32 Count = 0;
	int32 SourceLayer = 0;
	int32 DestinationLayer = 0;
};

struct FArcRmlRenderFrame
{
	FIntPoint ViewSize = FIntPoint::ZeroValue;
	TArray<FArcRmlResourceOp> ResourceOps;
	TArray<FArcRmlGeometryPayload> Geometries;
	TArray<FArcRmlTexturePayload> Textures;
	TArray<const FTextureReference*> AssetTextures;
	TArray<FArcRmlFilterDesc> Filters;
	TArray<FArcRmlShaderDesc> Shaders;
	TArray<FArcRmlDrawCommand> Commands;
	TArray<FMatrix44f> Transforms;
	TArray<uint64> FilterHandles;
	bool bSavesLayerTextures = false;
};
