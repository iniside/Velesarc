// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainMaterialExpressions.h"

#include "ArcTerrainChannelSet.h"
#include "ArcTerrainLayerBlendMath.h" // pure-scalar blend formulas mirrored by the Compile() node-calls
#include "ArcTerrainModule.h" // LogArcTerrain, ArcTerrainDiag::IsLayerBlendVerbose
#include "Editor/IEditorMaterialGraphProvider.h" // GEditorMaterialGraphProvider — pin regen on Layers edit
#include "Engine/Texture.h"
#include "Engine/Texture2DArray.h"
#include "ImageUtils.h"
#include "MaterialCompiler.h"
#include "PixelFormat.h"
#include "TextureResource.h"
#include "Materials/MaterialAttributeDefinitionMap.h" // GetProperty / CompileDefaultExpression for the MaterialAttributes path
#include "Materials/MaterialAggregate.h"
#include "Materials/MaterialToMIR.h"
#include "Materials/MIR/MIREmitter.h"

#define LOCTEXT_NAMESPACE "ArcTerrainMaterialExpressions"

namespace ArcTerrainMaterialExpressionsPrivate
{
	// Version-stamp virtual shader path (mapped by the ArcTerrain runtime module to the plugin's
	// Shaders dir). Declared via GetIncludeFilePaths so its content hash feeds the material shader-map
	// id (ExpressionIncludesHash) — see ArcTerrainNodeVersion.ush. Never actually #included.
	const TCHAR* const GNodeVersionIncludePath = TEXT("/Plugin/ArcTerrain/ArcTerrainNodeVersion.ush");

	// Slice-local UV -> texel-exact slice UV, resolution-agnostic (Compile() cannot know the raster). A
	// slice carries a 1-texel APRON on every side: N texels with the (N - 3 + 1) inner texel centers
	// spanning UV 0..1, so with InvRes = 1/N:
	//   UV' = UV*(1 - 3*InvRes) + 1.5*InvRes
	// The vertex factory writes the SECTOR-local UV into UV1 and the weight-group-local UV into UV0, both
	// normalised to [0,1] and both taken against the leaf CENTROID's slice; each raster has its own
	// resolution, so each remap reads its own InvRes scalar parameter. THE REMAP LIVES HERE, not in the
	// vertex factory — doing it in both would shrink every tile by a texel.
	int32 CompileSliceUV(FMaterialCompiler* Compiler, uint32 UVIndex, const TCHAR* InvResParameterName,
		float DefaultInvRes)
	{
		const int32 UVCode = Compiler->TextureCoordinate(UVIndex, false, false);
		const int32 InvResIndex = Compiler->ScalarParameter(FName(InvResParameterName), DefaultInvRes);
		const int32 ScaleIndex =
			Compiler->Sub(Compiler->Constant(1.0f), Compiler->Mul(Compiler->Constant(3.0f), InvResIndex));
		const int32 BiasIndex = Compiler->Mul(Compiler->Constant(1.5f), InvResIndex);
		return Compiler->Add(Compiler->Mul(UVCode, ScaleIndex), BiasIndex);
	}

	// UV2 carries the two atlas slice indices the vertex factory resolved per leaf out of the residency
	// rings: x = height slice, y = weight slice. They cannot be material parameters — both change per
	// leaf INSIDE one draw call.
	int32 CompileSliceIndex(FMaterialCompiler* Compiler, bool bWeightSlice)
	{
		const int32 SliceCoords = Compiler->TextureCoordinate(2, false, false);
		return Compiler->ComponentMask(SliceCoords, !bWeightSlice, bWeightSlice, false, false);
	}

	// Compile-time defaults matching QuadsPerCell 256 with SectorShift 2 / WeightSliceShift 0; the real
	// values arrive as scalar parameters from the terrain's material instance.
	constexpr float GDefaultHeightInvRes = 1.0f / 1027.0f;
	constexpr float GDefaultWeightInvRes = 1.0f / 259.0f;

	// Cached transient Texture2DArrays the nodes compile and cook against before the terrain's material
	// instance binds the real atlas arrays. Same shape as the engine's own GetDefaultTexture2DArray
	// compile placeholder. Two of them: the sampler type of a texture parameter must match its default,
	// and the weight arrays sample as Masks while the height array samples as LinearColor.
	UTexture2DArray* GetPlaceholderArray(bool bMasks)
	{
		static TStrongObjectPtr<UTexture2DArray> MaskPlaceholder;
		static TStrongObjectPtr<UTexture2DArray> LinearPlaceholder;
		TStrongObjectPtr<UTexture2DArray>& Placeholder = bMasks ? MaskPlaceholder : LinearPlaceholder;
		if (!Placeholder.IsValid())
		{
			UTexture2DArray* Created = FImageUtils::CreateCheckerboardTexture2DArray(
				FColor(0, 0, 0, 255), FColor(0, 0, 0, 255), 4, 1);
			if (Created != nullptr)
			{
#if WITH_EDITORONLY_DATA
				Created->SRGB = false;
				Created->CompressionSettings = bMasks ? TC_Masks : TC_HDR;
				Created->MipGenSettings = TMGS_NoMipmaps;
				Created->UpdateResource();
#endif
			}
			Placeholder.Reset(Created);
		}
		return Placeholder.Get();
	}

	/** DIAGNOSTICS ONLY: what the compile-time placeholder array actually is, at the point a node compiles
	 *  a TextureParameter against it. */
	void LogPlaceholderArray(const TCHAR* Context, UTexture2DArray* Placeholder)
	{
		if (!ArcTerrainDiag::IsLayerBlendVerbose())
		{
			return;
		}
		if (Placeholder == nullptr)
		{
			UE_LOGF(LogArcTerrain, Warning,"[Placeholder] %ls: <null>  <<<< the node compiles no texture parameter at all", Context);
			return;
		}
		const FTextureResource* const Resource = Placeholder->GetResource();
		UE_LOGF(LogArcTerrain, Warning,
			"[Placeholder] %ls: object='%ls' size=%dx%d slices=%d mips=%d format=%ls srgb=%d compression=%d "
			"resource=%ls  <<<< this array is the solid-BLACK compile default: any pixel still sampling it "
			"reads weight 0 on every channel",
			Context, *Placeholder->GetName(), Placeholder->GetSizeX(), Placeholder->GetSizeY(),
			Placeholder->GetArraySize(), Placeholder->GetNumMips(),
			GetPixelFormatString(Placeholder->GetPixelFormat()),
			Placeholder->SRGB ? 1 : 0, static_cast<int32>(Placeholder->CompressionSettings.GetValue()),
			Resource != nullptr ? TEXT("valid") : TEXT("NULL"));
	}

#if WITH_EDITOR
	// One named weight channel out of the tile atlas. The channel's (array, component) address is not a
	// compile-time constant: it arrives at runtime through two parameters named after the channel, and
	// the slice comes from the vertex factory's per-leaf weight slice, so a draw covering many cells
	// still samples each cell's own weights.
	int32 CompileChannelWeight(
		FMaterialCompiler& Compiler, const UArcTerrainChannelSet& ChannelSet, FName Channel)
	{
		if (ChannelSet.FindChannelIndex(Channel) == INDEX_NONE)
		{
			return Compiler.Errorf(TEXT("ArcTerrainLayerWeight: channel '%s' not found in ChannelSet."),
				*Channel.ToString());
		}

		UTexture2DArray* const Placeholder = GetPlaceholderArray(true);
		if (Placeholder == nullptr)
		{
			return Compiler.Errorf(TEXT("ArcTerrainLayerWeight: missing placeholder texture array."));
		}
		LogPlaceholderArray(TEXT("CompileChannelWeight"), Placeholder);

		const int32 ArrayCount = FMath::DivideAndRoundUp(ChannelSet.Channels.Num(), 4);
		const FString MaskParamName = FString::Printf(TEXT("ArcTerrainWeightMask_%s"), *Channel.ToString());
		const FString ArrayParamName = FString::Printf(TEXT("ArcTerrainWeightArray_%s"), *Channel.ToString());

		if (ArcTerrainDiag::IsLayerBlendVerbose())
		{
			UE_LOGF(LogArcTerrain, Warning,"ArcTerrainLayerWeight: Channel='%ls' Arrays=%d Mask='%ls' ArraySelect='%ls'",
				*Channel.ToString(), ArrayCount, *MaskParamName, *ArrayParamName);
		}

		TArray<int32> TextureCodes;
		TextureCodes.Reserve(ArrayCount);
		for (int32 ArrayIndex = 0; ArrayIndex < ArrayCount; ++ArrayIndex)
		{
			TextureCodes.Add(Compiler.TextureParameter(
				FName(*FString::Printf(TEXT("ArcTerrainWeights%d"), ArrayIndex)),
				Placeholder, SAMPLERTYPE_Masks));
		}
		// Defaults miss every array and every component: a channel the runtime never binds must
		// resolve to weight 0.
		const int32 MaskCode = Compiler.VectorParameter(
			FName(*MaskParamName), FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		const int32 ArraySelectCode = Compiler.ScalarParameter(FName(*ArrayParamName), -1.0f);

		const int32 UVCode = CompileSliceUV(&Compiler, 0, TEXT("ArcTerrainWeightInvRes"), GDefaultWeightInvRes);
		const int32 UVSliceCode = Compiler.AppendVector(UVCode, CompileSliceIndex(&Compiler, true));
		// Pure UV + CONSTANT slice for the derivative operands, so DDX/DDY never cross a slice boundary
		// and the sampler gets stable derivatives at cell UV seams.
		const int32 UVConstSliceCode = Compiler.AppendVector(UVCode, Compiler.Constant(0.0f));
		const int32 DDXCode = Compiler.DDX(UVConstSliceCode);
		const int32 DDYCode = Compiler.DDY(UVConstSliceCode);
		const int32 ThresholdCode = Compiler.Constant(0.5f);

		int32 WeightCode = Compiler.Constant(0.0f);
		for (int32 ArrayIndex = 0; ArrayIndex < ArrayCount; ++ArrayIndex)
		{
			const int32 SampleCode = Compiler.TextureSample(
				TextureCodes[ArrayIndex], UVSliceCode, SAMPLERTYPE_Masks, DDXCode, DDYCode,
				TMVM_Derivative, SSM_Clamp_WorldGroupSettings);
			WeightCode = Compiler.If(
				ArraySelectCode, Compiler.Constant(static_cast<float>(ArrayIndex)),
				WeightCode,
				Compiler.Dot(SampleCode, MaskCode),
				WeightCode,
				ThresholdCode);
		}
		return WeightCode;
	}

	// Layer input for one UMaterialExpressionArcTerrainLayerBlend layer: the connected LayerInput
	// expression, or the constant fallback FVector when unconnected (mirrors engine LandscapeLayerBlend
	// GetLayerCode). float3, so the blend output type follows the layer inputs.
	int32 CompileLayerBlendInput(FMaterialCompiler& Compiler, FArcTerrainLayerBlendInput& Layer)
	{
		if (Layer.LayerInput.Expression != nullptr)
		{
			return Layer.LayerInput.Compile(&Compiler);
		}
		return Compiler.Constant3(
			static_cast<float>(Layer.ConstLayerInput.X),
			static_cast<float>(Layer.ConstLayerInput.Y),
			static_cast<float>(Layer.ConstLayerInput.Z));
	}

	MIR::FValueRef BuildSliceUV(MIR::FEmitter& Em, uint32 UVIndex, FName InvResParameterName,
		float DefaultInvRes)
	{
		const MIR::FValueRef UV = MaterialToMIR::EmitTexCoord(Em, UVIndex);
		const MIR::FValueRef InvRes = Em.NamedPrimitiveUniform(
			InvResParameterName, Em.ConstantFloat(DefaultInvRes));
		const MIR::FValueRef Scale = Em.Subtract(
			Em.ConstantFloat(1.0f), Em.Multiply(Em.ConstantFloat(3.0f), InvRes));
		const MIR::FValueRef Bias = Em.Multiply(Em.ConstantFloat(1.5f), InvRes);
		return Em.Add(Em.Multiply(UV, Scale), Bias);
	}

	MIR::FValueRef BuildSliceIndex(MIR::FEmitter& Em, bool bWeightSlice)
	{
		const MIR::FValueRef SliceCoords = MaterialToMIR::EmitTexCoord(Em, 2);
		return Em.Swizzle(SliceCoords,
			bWeightSlice ? MIR::EVectorComponent::Y : MIR::EVectorComponent::X);
	}

	MIR::FValueRef BuildChannelWeight(
		MIR::FEmitter& Em, const UArcTerrainChannelSet& ChannelSet, FName Channel)
	{
		if (ChannelSet.FindChannelIndex(Channel) == INDEX_NONE)
		{
			return Em.Errorf(TEXT("ArcTerrainLayerWeight: channel '%s' not found in ChannelSet."),
				*Channel.ToString());
		}

		UTexture2DArray* const Placeholder = GetPlaceholderArray(true);
		if (Placeholder == nullptr)
		{
			return Em.Error(TEXT("ArcTerrainLayerWeight: missing placeholder texture array."));
		}
		LogPlaceholderArray(TEXT("BuildChannelWeight"), Placeholder);

		const int32 ArrayCount = FMath::DivideAndRoundUp(ChannelSet.Channels.Num(), 4);
		const FString MaskParamName = FString::Printf(TEXT("ArcTerrainWeightMask_%s"), *Channel.ToString());
		const FString ArrayParamName = FString::Printf(TEXT("ArcTerrainWeightArray_%s"), *Channel.ToString());

		TArray<MIR::FValueRef> Textures;
		Textures.Reserve(ArrayCount);
		for (int32 ArrayIndex = 0; ArrayIndex < ArrayCount; ++ArrayIndex)
		{
			Textures.Add(Em.TextureUniform(
				FName(*FString::Printf(TEXT("ArcTerrainWeights%d"), ArrayIndex)),
				Placeholder, MIR::FType::MakeTexture2DArray(), SAMPLERTYPE_Masks));
		}
		const MIR::FValueRef Mask = Em.NamedPrimitiveUniform(
			FName(*MaskParamName), Em.ConstantFloat4(FVector4f(0.0f, 0.0f, 0.0f, 0.0f)));
		const MIR::FValueRef ArraySelector = Em.NamedPrimitiveUniform(
			FName(*ArrayParamName), Em.ConstantFloat(-1.0f));

		const MIR::FValueRef UV = BuildSliceUV(
			Em, 0, FName(TEXT("ArcTerrainWeightInvRes")), GDefaultWeightInvRes);
		const MIR::FValueRef UVSlice = Em.Vector3(UV, BuildSliceIndex(Em, true));
		const MIR::FValueRef UVConstantSlice = Em.Vector3(UV, Em.ConstantFloat(0.0f));
		const MIR::FValueRef Ddx = Em.PartialDerivative(UVConstantSlice, MIR::EDerivativeAxis::X);
		const MIR::FValueRef Ddy = Em.PartialDerivative(UVConstantSlice, MIR::EDerivativeAxis::Y);
		const MIR::FValueRef Threshold = Em.ConstantFloat(0.5f);

		MIR::FTextureSampleAttributes SampleAttributes;
		SampleAttributes.SamplerSourceMode = SSM_Clamp_WorldGroupSettings;
		SampleAttributes.SamplerType = SAMPLERTYPE_Masks;

		MIR::FValueRef Weight = Em.ConstantFloat(0.0f);
		for (int32 ArrayIndex = 0; ArrayIndex < ArrayCount; ++ArrayIndex)
		{
			const MIR::FValueRef Sample = Em.TextureSampleGrad(
				Textures[ArrayIndex], UVSlice, Ddx, Ddy, false, SampleAttributes);
			const MIR::FValueRef MatchesArray = Em.LessThanOrEquals(
				Em.Abs(Em.Subtract(ArraySelector, Em.ConstantFloat(static_cast<float>(ArrayIndex)))), Threshold);
			Weight = Em.Select(MatchesArray, Em.Dot(Sample, Mask), Weight);
		}
		return Weight;
	}

	MIR::FValueRef BuildBlendMany(
		MIR::FEmitter& Em,
		MIR::FType CommonType,
		TConstArrayView<MIR::FValueRef> LayerWeights,
		TConstArrayView<MIR::FValueRef> LayerInputs,
		const TBitArray<>& IsWeightBlended,
		MIR::FValueRef InvWeightSum,
		MIR::FValueRef MaxWeight,
		MIR::FValueRef DefaultValue = {})
	{
		check(LayerWeights.Num() == LayerInputs.Num());
		check(LayerWeights.Num() == IsWeightBlended.Num());

		if (CommonType.IsAnyFloat())
		{
			MIR::FValueRef Output = DefaultValue.IsValid()
				? DefaultValue
				: Em.Cast(Em.ConstantZero(MIR::EScalarKind::Float), CommonType);
			for (int32 LayerIndex = 0; LayerIndex < LayerWeights.Num(); ++LayerIndex)
			{
				if (IsWeightBlended[LayerIndex])
				{
					const MIR::FValueRef LayerInput = Em.Cast(LayerInputs[LayerIndex], CommonType);
					const MIR::FValueRef Contribution = DefaultValue.IsValid()
						? Em.Subtract(LayerInput, DefaultValue)
						: LayerInput;
					Output = Em.Add(Output, Em.Multiply(
						Contribution, Em.Multiply(InvWeightSum, LayerWeights[LayerIndex])));
				}
			}
			for (int32 LayerIndex = 0; LayerIndex < LayerWeights.Num(); ++LayerIndex)
			{
				if (!IsWeightBlended[LayerIndex])
				{
					Output = Em.Lerp(Output, Em.Cast(LayerInputs[LayerIndex], CommonType),
						LayerWeights[LayerIndex]);
				}
			}
			return Output;
		}

		if (CommonType.IsInteger())
		{
			MIR::FValueRef Output = DefaultValue.IsValid()
				? DefaultValue
				: Em.ConstantInt(static_cast<int32>(EMaterialShadingModel::MSM_DefaultLit));
			const MIR::FValueRef AlteredMaxWeight = Em.Select(
				Em.GreaterThanOrEquals(MaxWeight, Em.ConstantFloat(0.5f)),
				MaxWeight, Em.ConstantFloat(TNumericLimits<float>::Max()));
			for (int32 LayerIndex = 0; LayerIndex < LayerWeights.Num(); ++LayerIndex)
			{
				if (!LayerInputs[LayerIndex]->Type.IsInteger())
				{
					continue;
				}
				const MIR::FValueRef AlteredLayerWeight = Em.Select(
					Em.GreaterThanOrEquals(LayerWeights[LayerIndex], Em.ConstantFloat(0.5f)),
					LayerWeights[LayerIndex], Em.ConstantFloat(TNumericLimits<float>::Lowest()));
				Output = Em.Select(
					Em.GreaterThanOrEquals(AlteredLayerWeight, AlteredMaxWeight),
					Em.Cast(LayerInputs[LayerIndex], CommonType), Output);
			}
			return Output;
		}

		if (const UMaterialAggregate* const Aggregate = CommonType.AsAggregate())
		{
			const MIR::FValueRef DefaultAggregate = DefaultValue.IsValid()
				? DefaultValue
				: Em.Aggregate(Aggregate);
			TArray<MIR::FValueRef> AttributeValues;
			AttributeValues.Reserve(Aggregate->Attributes.Num());
			for (int32 AttributeIndex = 0; AttributeIndex < Aggregate->Attributes.Num(); ++AttributeIndex)
			{
				const MIR::FValueRef AttributeDefault =
					Aggregate == MaterialAttributesAggregate::Get()
						&& MaterialAttributesAggregate::AttributeIndexToMaterialProperty(AttributeIndex) == MP_ShadingModel
					? Em.ConstantInt(static_cast<int32>(EMaterialShadingModel::MSM_DefaultLit))
					: Em.Subscript(DefaultAggregate, AttributeIndex);
				TArray<MIR::FValueRef> AttributeInputs;
				AttributeInputs.Reserve(LayerInputs.Num());
				for (const MIR::FValueRef LayerInput : LayerInputs)
				{
					AttributeInputs.Add(LayerInput->Type.AsAggregate()
						? Em.Subscript(LayerInput, AttributeIndex)
						: LayerInput);
				}
				AttributeValues.Add(BuildBlendMany(
					Em,
					MIR::FType::FromMaterialValueType(Aggregate->Attributes[AttributeIndex].ToMaterialValueType()),
					LayerWeights,
					AttributeInputs,
					IsWeightBlended,
					InvWeightSum,
					MaxWeight,
					AttributeDefault));
			}
			return Em.Aggregate(Aggregate, MIR::FValueRef{}, AttributeValues);
		}

		if (CommonType.IsSubstrateData())
		{
			return DefaultValue.IsValid() ? DefaultValue : Em.SubstrateDefaultSlab();
		}

		return Em.Errorf(TEXT("Cannot blend values of type '%s'."), *CommonType.GetSpelling());
	}

#endif // WITH_EDITOR
}

// **************************************************
// UMaterialExpressionArcTerrainHeight
// **************************************************

UMaterialExpressionArcTerrainHeight::UMaterialExpressionArcTerrainHeight(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	MenuCategories.Add(LOCTEXT("ArcTerrainCategory", "ArcTerrain"));
#endif
}

UObject* UMaterialExpressionArcTerrainHeight::GetReferencedTexture() const
{
	return ArcTerrainMaterialExpressionsPrivate::GetPlaceholderArray(false);
}

void UMaterialExpressionArcTerrainHeight::GetIncludeFilePaths(TSet<FString>& OutIncludeFilePaths) const
{
	OutIncludeFilePaths.Add(ArcTerrainMaterialExpressionsPrivate::GNodeVersionIncludePath);
}

#if WITH_EDITOR
int32 UMaterialExpressionArcTerrainHeight::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	UTexture2DArray* const Placeholder = ArcTerrainMaterialExpressionsPrivate::GetPlaceholderArray(false);
	if (Placeholder == nullptr)
	{
		return Compiler->Errorf(TEXT("ArcTerrainHeight: missing placeholder texture array."));
	}
	ArcTerrainMaterialExpressionsPrivate::LogPlaceholderArray(TEXT("ArcTerrainHeight::Compile"), Placeholder);
	PlaceholderTexture = Placeholder;

	// "ArcTerrainHeightAtlas" is the height atlas ARRAY; the slice is the one the vertex factory resolved
	// for this leaf's centroid and delivered in UV2.
	const int32 TextureCodeIndex = Compiler->TextureParameter(
		FName(TEXT("ArcTerrainHeightAtlas")), Placeholder, SAMPLERTYPE_LinearColor);
	const int32 UVCode = ArcTerrainMaterialExpressionsPrivate::CompileSliceUV(
		Compiler, 1, TEXT("ArcTerrainHeightInvRes"), ArcTerrainMaterialExpressionsPrivate::GDefaultHeightInvRes);
	const int32 UVSliceCode = Compiler->AppendVector(
		UVCode, ArcTerrainMaterialExpressionsPrivate::CompileSliceIndex(Compiler, false));
	// Height drives world-position offset, so this node also compiles in the VERTEX shader, where there
	// are no derivatives: DDX/DDY are illegal there, and only an explicit mip is a legal sample.
	const int32 SampleIndex = Compiler->TextureSample(
		TextureCodeIndex, UVSliceCode, SAMPLERTYPE_LinearColor,
		Compiler->Constant(0.0f), INDEX_NONE,
		TMVM_MipLevel, SSM_FromTextureAsset);
	// Height lives in the R channel and is terrain-local cm, both live and baked.
	return Compiler->ComponentMask(SampleIndex, true, false, false, false);
}

void UMaterialExpressionArcTerrainHeight::Build(MIR::FEmitter& Em)
{
	UTexture2DArray* const Placeholder = ArcTerrainMaterialExpressionsPrivate::GetPlaceholderArray(false);
	if (Placeholder == nullptr)
	{
		Em.Output(0, Em.Error(TEXT("ArcTerrainHeight: missing placeholder texture array.")));
		return;
	}
	ArcTerrainMaterialExpressionsPrivate::LogPlaceholderArray(TEXT("ArcTerrainHeight::Build"), Placeholder);
	PlaceholderTexture = Placeholder;

	const MIR::FValueRef Texture = Em.TextureUniform(
		FName(TEXT("ArcTerrainHeightAtlas")), Placeholder, MIR::FType::MakeTexture2DArray(), SAMPLERTYPE_LinearColor);
	const MIR::FValueRef UV = ArcTerrainMaterialExpressionsPrivate::BuildSliceUV(
		Em, 1, FName(TEXT("ArcTerrainHeightInvRes")), ArcTerrainMaterialExpressionsPrivate::GDefaultHeightInvRes);
	const MIR::FValueRef UVSlice = Em.Vector3(
		UV, ArcTerrainMaterialExpressionsPrivate::BuildSliceIndex(Em, false));
	MIR::FTextureSampleAttributes SampleAttributes;
	SampleAttributes.SamplerSourceMode = SSM_FromTextureAsset;
	SampleAttributes.SamplerType = SAMPLERTYPE_LinearColor;
	const MIR::FValueRef Sample = Em.TextureSampleLevel(
		Texture, UVSlice, Em.ConstantFloat(0.0f), false, SampleAttributes);
	Em.Output(0, Em.Subscript(Sample, 0));
}

EMaterialValueType UMaterialExpressionArcTerrainHeight::GetOutputValueType(int32 OutputIndex)
{
	return MCT_Float1;
}

void UMaterialExpressionArcTerrainHeight::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Arc Terrain Height"));
}
#endif // WITH_EDITOR

// **************************************************
// UMaterialExpressionArcTerrainLayerWeight
// **************************************************

UMaterialExpressionArcTerrainLayerWeight::UMaterialExpressionArcTerrainLayerWeight(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	MenuCategories.Add(LOCTEXT("ArcTerrainCategory", "ArcTerrain"));
#endif
}

UObject* UMaterialExpressionArcTerrainLayerWeight::GetReferencedTexture() const
{
	return ArcTerrainMaterialExpressionsPrivate::GetPlaceholderArray(true);
}

void UMaterialExpressionArcTerrainLayerWeight::GetIncludeFilePaths(TSet<FString>& OutIncludeFilePaths) const
{
	OutIncludeFilePaths.Add(ArcTerrainMaterialExpressionsPrivate::GNodeVersionIncludePath);
}

TArray<FName> UMaterialExpressionArcTerrainLayerWeight::GetChannelOptions() const
{
	if (const UArcTerrainChannelSet* ResolvedSet = ChannelSet)
	{
		return ResolvedSet->Channels;
	}
	return TArray<FName>();
}

#if WITH_EDITOR
int32 UMaterialExpressionArcTerrainLayerWeight::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (ChannelSet == nullptr)
	{
		return Compiler->Errorf(TEXT("ArcTerrainLayerWeight: no ChannelSet assigned."));
	}
	PlaceholderTexture = ArcTerrainMaterialExpressionsPrivate::GetPlaceholderArray(true);

	return ArcTerrainMaterialExpressionsPrivate::CompileChannelWeight(*Compiler, *ChannelSet, Channel);
}

void UMaterialExpressionArcTerrainLayerWeight::Build(MIR::FEmitter& Em)
{
	if (ChannelSet == nullptr)
	{
		Em.Output(0, Em.Error(TEXT("ArcTerrainLayerWeight: no ChannelSet assigned.")));
		return;
	}
	PlaceholderTexture = ArcTerrainMaterialExpressionsPrivate::GetPlaceholderArray(true);
	Em.Output(0, ArcTerrainMaterialExpressionsPrivate::BuildChannelWeight(Em, *ChannelSet, Channel));
}

EMaterialValueType UMaterialExpressionArcTerrainLayerWeight::GetOutputValueType(int32 OutputIndex)
{
	return MCT_Float1;
}

void UMaterialExpressionArcTerrainLayerWeight::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Arc Terrain Layer Weight"));
}
#endif // WITH_EDITOR

// **************************************************
// UMaterialExpressionArcTerrainLayerBlend
// **************************************************

UMaterialExpressionArcTerrainLayerBlend::UMaterialExpressionArcTerrainLayerBlend(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	MenuCategories.Add(LOCTEXT("ArcTerrainCategory", "ArcTerrain"));
#endif
}

UObject* UMaterialExpressionArcTerrainLayerBlend::GetReferencedTexture() const
{
	return ArcTerrainMaterialExpressionsPrivate::GetPlaceholderArray(true);
}

UMaterialExpression::ReferencedTextureArray UMaterialExpressionArcTerrainLayerBlend::GetReferencedTextures() const
{
	return { ArcTerrainMaterialExpressionsPrivate::GetPlaceholderArray(true) };
}

void UMaterialExpressionArcTerrainLayerBlend::GetIncludeFilePaths(TSet<FString>& OutIncludeFilePaths) const
{
	OutIncludeFilePaths.Add(ArcTerrainMaterialExpressionsPrivate::GNodeVersionIncludePath);
}

#if WITH_EDITOR
void UMaterialExpressionArcTerrainLayerBlend::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// A HeightInput pin only exists for HeightBlend layers (see GetInput); drop any stale height
	// expression on layers that are no longer height-blended so it can't feed a hidden pin.
	for (int32 LayerIdx = 0; LayerIdx < Layers.Num(); ++LayerIdx)
	{
		if (Layers[LayerIdx].BlendType != EArcTerrainLayerBlendType::HeightBlend)
		{
			Layers[LayerIdx].HeightInput.Expression = nullptr;
		}
	}

	// Adding/removing a layer (or toggling its BlendType) changes the input-pin set, but the graph
	// node's pins are only built once at construction — force a rebuild so the new LayerInput/HeightInput
	// pins appear. Mirrors UMaterialExpressionLandscapeLayerBlend::PostEditChangeProperty.
	if (PropertyChangedEvent.MemberProperty != nullptr
		&& PropertyChangedEvent.MemberProperty->GetFName()
			== GET_MEMBER_NAME_CHECKED(UMaterialExpressionArcTerrainLayerBlend, Layers))
	{
		if (GEditorMaterialGraphProvider != nullptr && GraphNode != nullptr)
		{
			GEditorMaterialGraphProvider->RecreateAndLinkMaterialGraphNode(GraphNode);
		}
	}
}

int32 UMaterialExpressionArcTerrainLayerBlend::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (ChannelSet == nullptr)
	{
		return Compiler->Errorf(TEXT("ArcTerrainLayerBlend: no ChannelSet assigned."));
	}
	PlaceholderTexture = ArcTerrainMaterialExpressionsPrivate::GetPlaceholderArray(true);

	const bool bLogLayerBlend = ArcTerrainDiag::IsLayerBlendVerbose();
	if (bLogLayerBlend)
	{
		const EMaterialProperty LogProperty =
			FMaterialAttributeDefinitionMap::GetProperty(Compiler->GetMaterialAttribute());
		FString ChannelsList;
		for (int32 ChannelIdx = 0; ChannelIdx < ChannelSet->Channels.Num(); ++ChannelIdx)
		{
			if (ChannelIdx > 0)
			{
				ChannelsList += TEXT(", ");
			}
			ChannelsList += ChannelSet->Channels[ChannelIdx].ToString();
		}
		UE_LOGF(LogArcTerrain, Warning,"ArcTerrainLayerBlend: Material='%ls' ChannelSet='%ls' Layers=%d Property=%ls Channels=[%ls]",
			Material != nullptr ? *Material->GetName() : TEXT("<none>"),
			*ChannelSet->GetPathName(), Layers.Num(),
			*StaticEnum<EMaterialProperty>()->GetNameStringByValue(static_cast<int64>(LogProperty)),
			*ChannelsList);
	}

	// Pass 1: resolve each layer's final weight, accumulating the WeightBlend/HeightBlend weight sum.
	// AlphaBlend layers are excluded from the sum (folded in after normalization). WeightCodes carries a
	// valid code for EVERY layer (alpha included) because MaxWeightCode — used by the ShadingModel pick —
	// weighs all layers; ArcTerrain's CompileChannelWeight never returns INDEX_NONE.
	int32 WeightSumCode = Compiler->Constant(0.0f);
	int32 MaxWeightCode = INDEX_NONE;
	TArray<int32> WeightCodes;
	WeightCodes.Reserve(Layers.Num());

	for (int32 LayerIdx = 0; LayerIdx < Layers.Num(); ++LayerIdx)
	{
		FArcTerrainLayerBlendInput& Layer = Layers[LayerIdx];

		if (bLogLayerBlend)
		{
			const int32 LogChannelIndex = ChannelSet->FindChannelIndex(Layer.Channel);
			const FString LogMapping = LogChannelIndex == INDEX_NONE
				? FString(TEXT("Array=<unresolved> Component=<unresolved>"))
				: FString::Printf(TEXT("Array=%d Component=%d"), LogChannelIndex / 4, LogChannelIndex % 4);
			UE_LOGF(LogArcTerrain, Warning,
				"ArcTerrainLayerBlend: Material='%ls' LayerIdx=%d Channel='%ls' ChannelIndex=%d %ls "
				"BlendType=%ls LayerInput=%ls HeightInput=%ls",
				Material != nullptr ? *Material->GetName() : TEXT("<none>"),
				LayerIdx, *Layer.Channel.ToString(), LogChannelIndex, *LogMapping,
				*StaticEnum<EArcTerrainLayerBlendType>()->GetNameStringByValue(static_cast<int64>(Layer.BlendType)),
				Layer.LayerInput.Expression != nullptr ? TEXT("connected") : TEXT("null"),
				Layer.HeightInput.Expression != nullptr ? TEXT("connected") : TEXT("null"));
		}

		const int32 RawWeightCode = ArcTerrainMaterialExpressionsPrivate::CompileChannelWeight(
			*Compiler, *ChannelSet, Layer.Channel);

		int32 WeightCode = RawWeightCode;
		switch (Layer.BlendType)
		{
		case EArcTerrainLayerBlendType::WeightBlend:
			WeightSumCode = Compiler->Add(WeightSumCode, WeightCode);
			break;
		case EArcTerrainLayerBlendType::HeightBlend:
		{
			int32 HeightCode = INDEX_NONE;
			if (Layer.HeightInput.Expression != nullptr)
			{
				HeightCode = Layer.HeightInput.Compile(Compiler);
			}
			// Fallback to ConstHeightInput if unconnected or compilation failed.
			if (HeightCode == INDEX_NONE)
			{
				HeightCode = Compiler->Constant(Layer.ConstHeightInput);
			}
			// Mirrors ArcTerrainLayerBlend::ArcTerrainHeightBlendWeight (ArcTerrainLayerBlendMath.h):
			//   clamp(lerp(-1, 1, RawWeight) + Height, 1e-4, 1)
			WeightCode = Compiler->Clamp(
				Compiler->Add(
					Compiler->Lerp(Compiler->Constant(-1.0f), Compiler->Constant(1.0f), RawWeightCode),
					HeightCode),
				Compiler->Constant(1e-4f), Compiler->Constant(1.0f));
			WeightSumCode = Compiler->Add(WeightSumCode, WeightCode);
			break;
		}
		case EArcTerrainLayerBlendType::AlphaBlend:
			// Excluded from the weight sum; folded in via Lerp after normalization.
			break;
		default:
			checkNoEntry();
			break;
		}
		WeightCodes.Add(WeightCode);

		// Track the running max weight (incremental Compiler->If: A>B -> A, A<B -> B, tie -> INDEX_NONE)
		// exactly as LandscapeLayerBlend :246-248 — consumed only by the MP_ShadingModel dominant pick.
		const int32 PreviousMaxWeightCode = MaxWeightCode;
		MaxWeightCode = (MaxWeightCode == INDEX_NONE)
			? WeightCode
			: Compiler->If(WeightCode, MaxWeightCode, WeightCode, INDEX_NONE, MaxWeightCode, INDEX_NONE);
		if (bLogLayerBlend)
		{
			UE_LOGF(LogArcTerrain, Warning,
				"ArcTerrainLayerBlend: LayerIdx=%d RawWeightCode=%d WeightCode=%d WeightSumCode=%d "
				"MaxWeightCode %d -> %d%ls",
				LayerIdx, RawWeightCode, WeightCode, WeightSumCode, PreviousMaxWeightCode, MaxWeightCode,
				MaxWeightCode == INDEX_NONE ? TEXT("  <<<< INDEX_NONE: Compile() will early-out and return the material default")
											: TEXT(""));
		}
	}

	// Normalize by the weight-sum group: FinalWeight = Weight / max(WeightSum, 1) so accumulated weights
	// stay in [0, 1] (matches LandscapeLayerBlend's non-legacy behavior). InvWeightSum operates on
	// compiler node indices, so it is NOT factored into the pure-math header. (Unused in the ShadingModel
	// branch, which is a discrete pick, but computed here to mirror the engine node ordering.)
	const int32 InvWeightSumCode = Compiler->Div(
		Compiler->Constant(1.0f), Compiler->Max(WeightSumCode, Compiler->Constant(1.0f)));

	// Which EMaterialProperty is this Compile invocation producing? For MaterialAttributes inputs the
	// translator calls Compile once per property; MP_ShadingModel is a discrete dominant-layer pick, every
	// other property is a delta-from-default weighted sum. GetProperty(GetMaterialAttribute()) mirrors
	// LandscapeLayerBlend :266 (ue6-main: GetMaterialAttribute() returns the property FGuid).
	const EMaterialProperty Property =
		FMaterialAttributeDefinitionMap::GetProperty(Compiler->GetMaterialAttribute());
	const bool bIsShadingModel = (Property == MP_ShadingModel);

	// DefaultCode is the compiled material-default of the current MA property; when set, weight/height
	// layers accumulate (LayerCode − DefaultCode) so the sum is Default + Sum(delta * InvSum * Weight) —
	// a proper weighted average toward the default, not toward black. MaterialAttributes-only: for a
	// plain MCT_Float output IsResultMaterialAttributes() is false, DefaultCode stays INDEX_NONE, and the
	// emitted HLSL is bit-for-bit the Step-2 float path (ContributionCode == LayerCode, OutputCode == 0).
	int32 OutputCode = INDEX_NONE;
	int32 DefaultCode = INDEX_NONE;
	if (bIsShadingModel)
	{
		OutputCode = Compiler->ShadingModel(EMaterialShadingModel::MSM_DefaultLit);
	}
	else if (IsResultMaterialAttributes(OutputIndex, {}) && Property != MP_MAX)
	{
		OutputCode = FMaterialAttributeDefinitionMap::CompileDefaultExpression(
			Compiler, Compiler->GetMaterialAttribute());
		DefaultCode = OutputCode;
	}
	else
	{
		OutputCode = Compiler->Constant(0.0f);
	}

	if (bLogLayerBlend)
	{
		UE_LOGF(LogArcTerrain, Warning,
			"ArcTerrainLayerBlend: Property=%ls IsResultMaterialAttributes=%ls DefaultCode=%d "
			"OutputCode(seed)=%d MaxWeightCode=%d InvWeightSumCode=%d -> %ls",
			*StaticEnum<EMaterialProperty>()->GetNameStringByValue(static_cast<int64>(Property)),
			IsResultMaterialAttributes(OutputIndex, {}) ? TEXT("yes") : TEXT("no"),
			DefaultCode, OutputCode, MaxWeightCode, InvWeightSumCode,
			MaxWeightCode == INDEX_NONE
				? TEXT("EARLY OUT — returns the seed, the layers are NEVER accumulated")
				: TEXT("accumulating"));
	}

	// Early-out with the best default when there isn't a single layer to blend (mirrors engine :288-291).
	if (MaxWeightCode == INDEX_NONE)
	{
		return OutputCode;
	}

	if (bIsShadingModel)
	{
		// MP_ShadingModel: not a weighted sum. The dominant layer wins — its weight must be >= 0.5 AND
		// strictly greater than every other layer's weight (modern non-legacy rule; the tie goes to the
		// later layer). Ported from LandscapeLayerBlend :304-340, legacy-CVar branch dropped to match the
		// float path (which already uses the modern max(sum,1) normalization).

		// If no layer clears 0.5, push the comparison bar to +inf so every layer fails the "> max" test.
		const int32 AlteredMaxWeightCode = Compiler->If(
			MaxWeightCode, Compiler->Constant(0.5f),
			MaxWeightCode, INDEX_NONE, Compiler->Constant(TNumericLimits<float>::Max()), INDEX_NONE);

		for (int32 LayerIdx = 0; LayerIdx < Layers.Num(); ++LayerIdx)
		{
			FArcTerrainLayerBlendInput& Layer = Layers[LayerIdx];
			const int32 LayerCode =
				ArcTerrainMaterialExpressionsPrivate::CompileLayerBlendInput(*Compiler, Layer);
			if (Compiler->GetType(LayerCode) != MCT_ShadingModel)
			{
				continue;
			}
			// Demote a sub-0.5 layer's weight to -inf so it can never be picked, then select this layer's
			// shading model only when its (possibly demoted) weight >= the altered max.
			const int32 AlteredLayerWeightCode = Compiler->If(
				WeightCodes[LayerIdx], Compiler->Constant(0.5f),
				WeightCodes[LayerIdx], INDEX_NONE, Compiler->Constant(TNumericLimits<float>::Lowest()), INDEX_NONE);
			OutputCode = Compiler->If(
				AlteredLayerWeightCode, AlteredMaxWeightCode,
				LayerCode, INDEX_NONE, OutputCode, INDEX_NONE);
		}

		return OutputCode;
	}

	// Non-ShadingModel: weight/height-blended layers first (delta-from-default when MA), alpha layers last.
	// Pass 2: accumulate the normalized weight/height-blended layers.
	for (int32 LayerIdx = 0; LayerIdx < Layers.Num(); ++LayerIdx)
	{
		FArcTerrainLayerBlendInput& Layer = Layers[LayerIdx];
		if (Layer.BlendType == EArcTerrainLayerBlendType::AlphaBlend)
		{
			continue;
		}
		const int32 LayerCode =
			ArcTerrainMaterialExpressionsPrivate::CompileLayerBlendInput(*Compiler, Layer);
		// MaterialAttributes: accumulate the delta from the material default; plain float: LayerCode as-is.
		const int32 ContributionCode =
			(DefaultCode != INDEX_NONE) ? Compiler->Sub(LayerCode, DefaultCode) : LayerCode;
		OutputCode = Compiler->Add(
			OutputCode, Compiler->Mul(ContributionCode, Compiler->Mul(InvWeightSumCode, WeightCodes[LayerIdx])));
	}

	// Pass 3: fold in AlphaBlend layers last via Lerp — mirrors
	// ArcTerrainLayerBlend::ArcTerrainAlphaBlend (ArcTerrainLayerBlendMath.h): lerp(Under, Over, Weight).
	for (int32 LayerIdx = 0; LayerIdx < Layers.Num(); ++LayerIdx)
	{
		FArcTerrainLayerBlendInput& Layer = Layers[LayerIdx];
		if (Layer.BlendType != EArcTerrainLayerBlendType::AlphaBlend)
		{
			continue;
		}
		const int32 LayerCode =
			ArcTerrainMaterialExpressionsPrivate::CompileLayerBlendInput(*Compiler, Layer);
		OutputCode = Compiler->Lerp(OutputCode, LayerCode, WeightCodes[LayerIdx]);
	}

	return OutputCode;
}

void UMaterialExpressionArcTerrainLayerBlend::Build(MIR::FEmitter& Em)
{
	if (ChannelSet == nullptr)
	{
		Em.Output(0, Em.Error(TEXT("ArcTerrainLayerBlend: no ChannelSet assigned.")));
		return;
	}
	PlaceholderTexture = ArcTerrainMaterialExpressionsPrivate::GetPlaceholderArray(true);

	MIR::FValueRef WeightSum = Em.ConstantFloat(0.0f);
	MIR::FValueRef MaxWeight;
	TArray<MIR::FValueRef> LayerWeights;
	TArray<MIR::FValueRef> LayerInputs;
	TBitArray<> IsWeightBlended;
	LayerWeights.Reserve(Layers.Num());
	LayerInputs.Reserve(Layers.Num());
	IsWeightBlended.Reserve(Layers.Num());

	MIR::FType CommonType = MIR::FType::MakeVoid();
	const UMaterialAggregate* CommonAggregate = nullptr;
	for (int32 LayerIndex = 0; LayerIndex < Layers.Num(); ++LayerIndex)
	{
		FArcTerrainLayerBlendInput& Layer = Layers[LayerIndex];
		const MIR::FValueRef LayerInput = Em.InputDefaultFloat3(
			&Layer.LayerInput, FVector3f(Layer.ConstLayerInput));
		UE_MIR_CHECK(Em, LayerInput);
		LayerInputs.Add(LayerInput);

		if (const UMaterialAggregate* const LayerAggregate = LayerInput->Type.AsAggregate())
		{
			if (CommonAggregate != nullptr && CommonAggregate != LayerAggregate)
			{
				Em.Output(0, Em.Errorf(TEXT("Incompatible aggregate input type for layer [%i] (%s)."),
					LayerIndex, *Layer.Channel.ToString()));
				return;
			}
			CommonAggregate = LayerAggregate;
			CommonType = LayerInput->Type;
		}
		else if (CommonAggregate == nullptr)
		{
			if (CommonType.IsVoid())
			{
				CommonType = LayerInput->Type;
			}
			else
			{
				const MIR::FType LocalCommonType = Em.TryGetCommonType(LayerInput->Type, CommonType);
				if (LocalCommonType.IsPoison())
				{
					Em.Output(0, Em.Errorf(
						TEXT("Incompatible input types for layer [%i] (%s): %s vs. %s."),
						LayerIndex, *Layer.Channel.ToString(),
						*LayerInput->Type.GetSpelling(), *CommonType.GetSpelling()));
					return;
				}
				CommonType = LocalCommonType;
			}
		}

		const MIR::FValueRef RawWeight = ArcTerrainMaterialExpressionsPrivate::BuildChannelWeight(
			Em, *ChannelSet, Layer.Channel);
		MIR::FValueRef FinalWeight = RawWeight;
		switch (Layer.BlendType)
		{
		case EArcTerrainLayerBlendType::WeightBlend:
			WeightSum = Em.Add(WeightSum, FinalWeight);
			IsWeightBlended.Add(true);
			break;
		case EArcTerrainLayerBlendType::HeightBlend:
		{
			const MIR::FValueRef Height = Em.InputDefaultFloat(&Layer.HeightInput, Layer.ConstHeightInput);
			FinalWeight = Em.Clamp(
				Em.Add(Em.Lerp(Em.ConstantFloat(-1.0f), Em.ConstantFloat(1.0f), RawWeight), Height),
				Em.ConstantFloat(1e-4f), Em.ConstantFloat(1.0f));
			WeightSum = Em.Add(WeightSum, FinalWeight);
			IsWeightBlended.Add(true);
			break;
		}
		case EArcTerrainLayerBlendType::AlphaBlend:
			IsWeightBlended.Add(false);
			break;
		default:
			checkNoEntry();
			break;
		}

		LayerWeights.Add(FinalWeight);
		MaxWeight = MaxWeight.IsValid() ? Em.Max(MaxWeight, FinalWeight) : FinalWeight;
	}

	UE_MIR_CHECK(Em);
	if (LayerWeights.IsEmpty())
	{
		Em.Output(0, Em.ConstantFloat(0.0f));
		return;
	}

	const MIR::FValueRef InvWeightSum = Em.Divide(
		Em.ConstantFloat(1.0f), Em.Max(WeightSum, Em.ConstantFloat(1.0f)));
	Em.Output(0, ArcTerrainMaterialExpressionsPrivate::BuildBlendMany(
		Em, CommonType, LayerWeights, LayerInputs, IsWeightBlended, InvWeightSum, MaxWeight));
}

bool UMaterialExpressionArcTerrainLayerBlend::IsResultMaterialAttributes(
	int32 OutputIndex, TMaterialRecursionGuard<UMaterialExpression> RecursionGuard)
{
	// Mirrors LandscapeLayerBlend :158-174: the node is a MaterialAttributes result iff any connected
	// LayerInput resolves to MaterialAttributes. Recursion-guarded against cyclic graphs.
	if (RecursionGuard.Contains(this))
	{
		return false;
	}
	RecursionGuard.Set(this);

	for (int32 LayerIdx = 0; LayerIdx < Layers.Num(); ++LayerIdx)
	{
		FExpressionInput& LayerInput = Layers[LayerIdx].LayerInput;
		if (LayerInput.Expression != nullptr
			&& LayerInput.Expression->IsResultMaterialAttributes(LayerInput.OutputIndex, RecursionGuard))
		{
			return true;
		}
	}
	return false;
}

EMaterialValueType UMaterialExpressionArcTerrainLayerBlend::GetInputValueType(int32 InputIndex)
{
	int32 Idx = 0;
	for (int32 LayerIdx = 0; LayerIdx < Layers.Num(); ++LayerIdx)
	{
		if (InputIndex == Idx++)
		{
			// LayerInput accepts floats or MaterialAttributes (mirrors LandscapeLayerBlend :144).
			return static_cast<EMaterialValueType>(MCT_Float | MCT_MaterialAttributes);
		}
		if (Layers[LayerIdx].BlendType == EArcTerrainLayerBlendType::HeightBlend)
		{
			if (InputIndex == Idx++)
			{
				return MCT_Float1; // the height input must be float1
			}
		}
	}
	return MCT_Unknown;
}

TArrayView<FExpressionInput*> UMaterialExpressionArcTerrainLayerBlend::GetInputsView()
{
	CachedInputs.Empty();
	CachedInputs.Reserve(Layers.Num() * 2);
	for (int32 LayerIdx = 0; LayerIdx < Layers.Num(); ++LayerIdx)
	{
		CachedInputs.Add(&Layers[LayerIdx].LayerInput);
		if (Layers[LayerIdx].BlendType == EArcTerrainLayerBlendType::HeightBlend)
		{
			CachedInputs.Add(&Layers[LayerIdx].HeightInput);
		}
	}
	return CachedInputs;
}

FExpressionInput* UMaterialExpressionArcTerrainLayerBlend::GetInput(int32 InputIndex)
{
	int32 Idx = 0;
	for (int32 LayerIdx = 0; LayerIdx < Layers.Num(); ++LayerIdx)
	{
		if (InputIndex == Idx++)
		{
			return &Layers[LayerIdx].LayerInput;
		}
		if (Layers[LayerIdx].BlendType == EArcTerrainLayerBlendType::HeightBlend)
		{
			if (InputIndex == Idx++)
			{
				return &Layers[LayerIdx].HeightInput;
			}
		}
	}
	return nullptr;
}

FName UMaterialExpressionArcTerrainLayerBlend::GetInputName(int32 InputIndex) const
{
	int32 Idx = 0;
	for (int32 LayerIdx = 0; LayerIdx < Layers.Num(); ++LayerIdx)
	{
		if (InputIndex == Idx++)
		{
			return *FString::Printf(TEXT("Layer %s"), *Layers[LayerIdx].Channel.ToString());
		}
		if (Layers[LayerIdx].BlendType == EArcTerrainLayerBlendType::HeightBlend)
		{
			if (InputIndex == Idx++)
			{
				return *FString::Printf(TEXT("Height %s"), *Layers[LayerIdx].Channel.ToString());
			}
		}
	}
	return NAME_None;
}

void UMaterialExpressionArcTerrainLayerBlend::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Arc Terrain Layer Blend"));
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
