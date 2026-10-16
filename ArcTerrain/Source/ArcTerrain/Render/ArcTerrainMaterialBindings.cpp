// Copyright Lukasz Baran. All Rights Reserved.

#include "Render/ArcTerrainMaterialBindings.h"

#include "ArcTerrainChannelSet.h"
#include "ArcTerrainModule.h"
#include "Engine/Texture.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameters.h"

namespace ArcTerrainMaterialBindingsPrivate_74c1
{
	const FName HeightAtlasName(TEXT("ArcTerrainHeightAtlas"));
	const FName HeightInvResName(TEXT("ArcTerrainHeightInvRes"));
	const FName WeightInvResName(TEXT("ArcTerrainWeightInvRes"));
	const FName CellSizeName(TEXT("ArcTerrainCellSize"));
	const FString WeightsPrefix(TEXT("ArcTerrainWeights"));
	const FString WeightMaskPrefix(TEXT("ArcTerrainWeightMask_"));
	const FString WeightArrayPrefix(TEXT("ArcTerrainWeightArray_"));

	bool TryParseArrayIndex(const FString& ParameterName, int32& OutArrayIndex)
	{
		if (!ParameterName.StartsWith(WeightsPrefix, ESearchCase::CaseSensitive))
		{
			return false;
		}

		const FString Suffix = ParameterName.RightChop(WeightsPrefix.Len());
		if (Suffix.IsEmpty())
		{
			return false;
		}
		if (Suffix.Len() > 1 && Suffix[0] == TEXT('0'))
		{
			return false;
		}

		int64 ParsedIndex = 0;
		for (int32 CharacterIndex = 0; CharacterIndex < Suffix.Len(); ++CharacterIndex)
		{
			const TCHAR Character = Suffix[CharacterIndex];
			if (!FChar::IsDigit(Character))
			{
				return false;
			}
			ParsedIndex = ParsedIndex * 10 + static_cast<int64>(Character - TEXT('0'));
			if (ParsedIndex > MAX_int32)
			{
				return false;
			}
		}

		OutArrayIndex = static_cast<int32>(ParsedIndex);
		return true;
	}

	bool TryResolveChannelIndex(const FString& ParameterName, const FString& Prefix,
		const UArcTerrainChannelSet* ChannelSet, int32& OutChannelIndex)
	{
		if (!ParameterName.StartsWith(Prefix, ESearchCase::CaseSensitive))
		{
			return false;
		}

		const FString ChannelName = ParameterName.RightChop(Prefix.Len());
		if (ChannelName.IsEmpty() || ChannelSet == nullptr)
		{
			return false;
		}

		OutChannelIndex = ChannelSet->FindChannelIndex(FName(*ChannelName));
		return ChannelSet->Channels.IsValidIndex(OutChannelIndex);
	}

	const TCHAR* TextureTypeName(EMaterialTextureParameterType Type)
	{
		switch (Type)
		{
		case EMaterialTextureParameterType::Standard2D:
			return TEXT("standard2d");
		case EMaterialTextureParameterType::Cube:
			return TEXT("cube");
		case EMaterialTextureParameterType::Array2D:
			return TEXT("array2d");
		case EMaterialTextureParameterType::ArrayCube:
			return TEXT("arraycube");
		case EMaterialTextureParameterType::Volume:
			return TEXT("volume");
		case EMaterialTextureParameterType::Virtual:
			return TEXT("virtual");
		case EMaterialTextureParameterType::SparseVolume:
			return TEXT("sparsevolume");
		default:
			return TEXT("unknown");
		}
	}

	void LogBindingFailure(const UMaterialInstanceDynamic& MID, const FMaterialParameterInfo& ParameterInfo,
		const TCHAR* Reason)
	{
		UE_LOGF(LogArcTerrain, Warning,"[MaterialBind] MID='%ls' param='%ls' assoc=%d layerindex=%d failed: %ls",
			*MID.GetName(), *ParameterInfo.Name.ToString(),
			static_cast<int32>(ParameterInfo.Association), ParameterInfo.Index, Reason);
	}
}

ArcTerrainMaterialBindings::EResult ArcTerrainMaterialBindings::Bind(
	UMaterialInstanceDynamic& MID, EShaderPlatform ShaderPlatform,
	UTexture* HeightAtlas, TConstArrayView<UTexture*> WeightAtlases,
	const UArcTerrainChannelSet* ChannelSet, float HeightInvRes, float WeightInvRes,
	float CellSizeCm, bool bDumpMaterialParameters, bool bDumpWeightConsistency)
{
	using namespace ArcTerrainMaterialBindingsPrivate_74c1;

	FMaterialResource* const Resource = MID.GetMaterialResource(ShaderPlatform);
	if (Resource == nullptr)
	{
		UE_LOGF(LogArcTerrain, Warning,"[MaterialBind] MID='%ls' shaderplatform=%d has no active material resource; binding will retry.",
			*MID.GetName(), static_cast<int32>(ShaderPlatform));
		return EResult::ResourceUnavailable;
	}
	if (Resource->GetGameThreadShaderMap() == nullptr)
	{
		UE_LOGF(LogArcTerrain, Warning,"[MaterialBind] MID='%ls' shaderplatform=%d has no active game-thread shader map; binding will retry.",
			*MID.GetName(), static_cast<int32>(ShaderPlatform));
		return EResult::ResourceUnavailable;
	}

	const TArrayView<const FMaterialNumericParameterInfo> NumericExpressions =
		Resource->GetUniformNumericParameterExpressions();
	int32 NumTextureExpressions = 0;
	for (uint32 TypeIndex = 0; TypeIndex < NumMaterialTextureParameterTypes; ++TypeIndex)
	{
		const EMaterialTextureParameterType TextureType =
			static_cast<EMaterialTextureParameterType>(TypeIndex);
		NumTextureExpressions += Resource->GetUniformTextureExpressions(TextureType).Num();
	}

	int32 NumScalarExpressions = 0;
	for (const FMaterialNumericParameterInfo& Expression : NumericExpressions)
	{
		if (Expression.ParameterType == EMaterialParameterType::Scalar)
		{
			++NumScalarExpressions;
		}
	}

	if (bDumpMaterialParameters)
	{
		UMaterialInterface* const ParentMaterial = MID.Parent;
		const UMaterial* const BaseMaterial = MID.GetMaterial();
		UE_LOGF(LogArcTerrain, Warning,"[MatParams] MID='%ls' parent='%ls' basematerial='%ls': compiled textureparams=%d scalarparams=%d",
			*MID.GetName(), ParentMaterial != nullptr ? *ParentMaterial->GetName() : TEXT("<null>"),
			BaseMaterial != nullptr ? *BaseMaterial->GetName() : TEXT("<null>"),
			NumTextureExpressions, NumScalarExpressions);
	}

	const FUniformExpressionSet& UniformExpressions = Resource->GetUniformExpressions();
	const int32 Array2DCount = Resource->GetUniformTextureExpressions(
		EMaterialTextureParameterType::Array2D).Num();
	const int32 Standard2DCount = Resource->GetUniformTextureExpressions(
		EMaterialTextureParameterType::Standard2D).Num();
	if (bDumpMaterialParameters)
	{
		UE_LOGF(LogArcTerrain, Warning,"[UniformTex] MID='%ls' array2dcount=%d standard2dcount=%d",
			*MID.GetName(), Array2DCount, Standard2DCount);
	}

	bool bSucceeded = true;
	TSet<FMaterialParameterInfo> BoundTextureParameters;
	TSet<FMaterialParameterInfo> BoundNumericParameters;
	TSet<FName> CompiledTextureNames;
	TSet<FName> CompiledScalarNames;
	TSet<int32> CompiledWeightArrayIndices;
	int32 TextureDiagnosticIndex = 0;

	for (uint32 TypeIndex = 0; TypeIndex < NumMaterialTextureParameterTypes; ++TypeIndex)
	{
		const EMaterialTextureParameterType TextureType =
			static_cast<EMaterialTextureParameterType>(TypeIndex);
		const TArrayView<const FMaterialTextureParameterInfo> TextureExpressions =
			Resource->GetUniformTextureExpressions(TextureType);
		for (int32 ExpressionIndex = 0; ExpressionIndex < TextureExpressions.Num(); ++ExpressionIndex)
		{
			const FMaterialTextureParameterInfo& Expression = TextureExpressions[ExpressionIndex];
			const FMaterialParameterInfo ParameterInfo(Expression.ParameterInfo);
			const FString ParameterName = ParameterInfo.Name.ToString();
			CompiledTextureNames.Add(ParameterInfo.Name);

			bool bArcTerrainParameter = false;
			bool bParameterValid = true;
			UTexture* Value = nullptr;
			if (ParameterInfo.Name == HeightAtlasName)
			{
				bArcTerrainParameter = true;
				Value = HeightAtlas;
				bParameterValid = Value != nullptr;
			}
			else if (ParameterName.StartsWith(WeightsPrefix, ESearchCase::CaseSensitive))
			{
				bArcTerrainParameter = true;
				int32 ArrayIndex = INDEX_NONE;
				bParameterValid = TryParseArrayIndex(ParameterName, ArrayIndex)
					&& WeightAtlases.IsValidIndex(ArrayIndex)
					&& WeightAtlases[ArrayIndex] != nullptr;
				if (bParameterValid)
				{
					CompiledWeightArrayIndices.Add(ArrayIndex);
					Value = WeightAtlases[ArrayIndex];
				}
			}

			if (bArcTerrainParameter && !BoundTextureParameters.Contains(ParameterInfo))
			{
				BoundTextureParameters.Add(ParameterInfo);
				if (!bParameterValid)
				{
					LogBindingFailure(MID, ParameterInfo, TEXT("invalid texture suffix, array bounds, or texture value"));
					bSucceeded = false;
				}
				else
				{
					MID.SetTextureParameterValueByInfo(ParameterInfo, Value);
					UTexture* ReadBack = nullptr;
					const bool bResolved = MID.GetTextureParameterValue(ParameterInfo, ReadBack);
					const bool bMatches = bResolved && ReadBack == Value;
					UE_CLOGF(bDumpMaterialParameters, LogArcTerrain, Warning,"[AtlasBind] param='%ls' assoc=%d layerindex=%d exists=%ls readback='%ls'%ls",
						*ParameterName, static_cast<int32>(ParameterInfo.Association), ParameterInfo.Index,
						bResolved ? TEXT("yes") : TEXT("NO"),
						ReadBack != nullptr ? *ReadBack->GetName() : TEXT("<null>"),
						bMatches ? TEXT("") : TEXT("  <<<< the owner-aware override did NOT resolve"));
					if (!bMatches)
					{
						LogBindingFailure(MID, ParameterInfo, TEXT("texture override did not resolve to the bound value"));
						bSucceeded = false;
					}
				}
			}

			if (bDumpMaterialParameters)
			{
				UTexture* Resolved = nullptr;
				const bool bHasValue = MID.GetTextureParameterValue(ParameterInfo, Resolved);
				UTexture* OverrideValue = nullptr;
				const bool bOverridden = MID.GetTextureParameterValue(ParameterInfo, OverrideValue, true);
				UE_LOGF(LogArcTerrain, Warning,"[MatParams]   texture[%d] '%ls' type=%ls assoc=%d layerindex=%d value='%ls' overridden=%ls overridevalue='%ls'",
					TextureDiagnosticIndex, *ParameterName, TextureTypeName(TextureType),
					static_cast<int32>(ParameterInfo.Association), ParameterInfo.Index,
					(bHasValue && Resolved != nullptr) ? *Resolved->GetName() : TEXT("<null>"),
					bOverridden ? TEXT("yes") : TEXT("no"),
					OverrideValue != nullptr ? *OverrideValue->GetName() : TEXT("<null>"));

				if (TextureType == EMaterialTextureParameterType::Array2D
					|| TextureType == EMaterialTextureParameterType::Standard2D)
				{
					UTexture* UniformValue = nullptr;
					UniformExpressions.GetGameThreadTextureValue(
						TextureType, ExpressionIndex, &MID, *Resource, UniformValue, true);
					UTexture* UniformValueNoTransient = nullptr;
					UniformExpressions.GetGameThreadTextureValue(
						TextureType, ExpressionIndex, &MID, *Resource, UniformValueNoTransient, false);
					UE_LOGF(LogArcTerrain, Warning,"[UniformTex]   %ls[%d] param='%ls' assoc=%d layerindex=%d textureindex=%d resolved='%ls' resolved_no_transient='%ls'",
						TextureTypeName(TextureType), ExpressionIndex, *ParameterName,
						static_cast<int32>(ParameterInfo.Association), ParameterInfo.Index,
						Expression.TextureIndex,
						UniformValue != nullptr ? *UniformValue->GetName() : TEXT("<null>"),
						UniformValueNoTransient != nullptr ? *UniformValueNoTransient->GetName() : TEXT("<null>"));
				}
			}
			++TextureDiagnosticIndex;
		}
	}

	static const FLinearColor OneHotRGBA[4] = {
		FLinearColor(1.0f, 0.0f, 0.0f, 0.0f),
		FLinearColor(0.0f, 1.0f, 0.0f, 0.0f),
		FLinearColor(0.0f, 0.0f, 1.0f, 0.0f),
		FLinearColor(0.0f, 0.0f, 0.0f, 1.0f)};
	static TSet<FName> LoggedUnresolvedWeightParams;
	static TSet<FName> LoggedArrayCountMismatches;
	static TMap<FName, FString> LastSummaryByIdentity;
	int32 NumMaskParams = 0;
	int32 NumArrayParams = 0;
	int32 NumUnresolved = 0;

	for (int32 ExpressionIndex = 0; ExpressionIndex < NumericExpressions.Num(); ++ExpressionIndex)
	{
		const FMaterialNumericParameterInfo& Expression = NumericExpressions[ExpressionIndex];
		const FMaterialParameterInfo ParameterInfo(Expression.ParameterInfo);
		const FString ParameterName = ParameterInfo.Name.ToString();
		if (Expression.ParameterType == EMaterialParameterType::Scalar)
		{
			CompiledScalarNames.Add(ParameterInfo.Name);
		}

		bool bArcTerrainParameter = false;
		bool bVectorParameter = false;
		bool bParameterValid = true;
		float ScalarValue = 0.0f;
		FLinearColor VectorValue = FLinearColor::Black;
		int32 ChannelIndex = INDEX_NONE;
		const bool bInvResParameter = ParameterInfo.Name == HeightInvResName
			|| ParameterInfo.Name == WeightInvResName;

		if (ParameterInfo.Name == HeightInvResName)
		{
			bArcTerrainParameter = true;
			ScalarValue = HeightInvRes;
			bParameterValid = Expression.ParameterType == EMaterialParameterType::Scalar;
		}
		else if (ParameterInfo.Name == WeightInvResName)
		{
			bArcTerrainParameter = true;
			ScalarValue = WeightInvRes;
			bParameterValid = Expression.ParameterType == EMaterialParameterType::Scalar;
		}
		else if (ParameterInfo.Name == CellSizeName)
		{
			bArcTerrainParameter = true;
			ScalarValue = CellSizeCm;
			bParameterValid = Expression.ParameterType == EMaterialParameterType::Scalar;
		}
		else if (ParameterName.StartsWith(WeightMaskPrefix, ESearchCase::CaseSensitive))
		{
			bArcTerrainParameter = true;
			bVectorParameter = true;
			++NumMaskParams;
			bParameterValid = Expression.ParameterType == EMaterialParameterType::Vector
				&& TryResolveChannelIndex(ParameterName, WeightMaskPrefix, ChannelSet, ChannelIndex);
			if (bParameterValid)
			{
				VectorValue = OneHotRGBA[ChannelIndex % 4];
			}
		}
		else if (ParameterName.StartsWith(WeightArrayPrefix, ESearchCase::CaseSensitive))
		{
			bArcTerrainParameter = true;
			++NumArrayParams;
			bParameterValid = Expression.ParameterType == EMaterialParameterType::Scalar
				&& TryResolveChannelIndex(ParameterName, WeightArrayPrefix, ChannelSet, ChannelIndex);
			if (bParameterValid)
			{
				ScalarValue = static_cast<float>(ChannelIndex / 4);
			}
		}

		if (bDumpWeightConsistency && (bInvResParameter
			|| ParameterName.StartsWith(WeightMaskPrefix, ESearchCase::CaseSensitive)
			|| ParameterName.StartsWith(WeightArrayPrefix, ESearchCase::CaseSensitive)))
		{
			UE_LOGF(LogArcTerrain, Log,"[WeightParams] MID='%ls' compiled param='%ls' type=%ls assoc=%d layerindex=%d",
				*MID.GetName(), *ParameterName, *MaterialParameterTypeToString(Expression.ParameterType),
				static_cast<int32>(ParameterInfo.Association), ParameterInfo.Index);
		}

		if (bArcTerrainParameter && !BoundNumericParameters.Contains(ParameterInfo))
		{
			BoundNumericParameters.Add(ParameterInfo);
			if (!bParameterValid)
			{
				LogBindingFailure(MID, ParameterInfo, TEXT("invalid numeric type, channel suffix, or channel lookup"));
				bSucceeded = false;
				if (ParameterName.StartsWith(WeightMaskPrefix, ESearchCase::CaseSensitive)
					|| ParameterName.StartsWith(WeightArrayPrefix, ESearchCase::CaseSensitive))
				{
					++NumUnresolved;
					const FName Key(*FString::Printf(TEXT("%s|%s|%d|%d"), *MID.GetFName().ToString(),
						*ParameterName, static_cast<int32>(ParameterInfo.Association), ParameterInfo.Index));
					bool bAlreadyLogged = false;
					LoggedUnresolvedWeightParams.Add(Key, &bAlreadyLogged);
					if (!bAlreadyLogged)
					{
						UE_LOGF(LogArcTerrain, Warning,"[WeightParams] MID='%ls' compiled param='%ls' assoc=%d layerindex=%d cannot resolve channel '%ls'.",
							*MID.GetName(), *ParameterName, static_cast<int32>(ParameterInfo.Association),
							ParameterInfo.Index,
							*ParameterName.RightChop(ParameterName.StartsWith(WeightMaskPrefix,
								ESearchCase::CaseSensitive) ? WeightMaskPrefix.Len() : WeightArrayPrefix.Len()));
					}
				}
			}
			else if (bVectorParameter)
			{
				MID.SetVectorParameterValueByInfo(ParameterInfo, VectorValue);
				FLinearColor ReadBack = FLinearColor::Black;
				const bool bResolved = MID.GetVectorParameterValue(ParameterInfo, ReadBack);
				if (!bResolved || ReadBack != VectorValue)
				{
					LogBindingFailure(MID, ParameterInfo, TEXT("vector override did not resolve to the bound value"));
					bSucceeded = false;
				}
				UE_CLOGF(bDumpMaterialParameters, LogArcTerrain, Warning,"[MaterialBind] param='%ls' assoc=%d layerindex=%d vector resolved=%ls",
					*ParameterName, static_cast<int32>(ParameterInfo.Association), ParameterInfo.Index,
					(bResolved && ReadBack == VectorValue) ? TEXT("yes") : TEXT("NO"));
			}
			else
			{
				MID.SetScalarParameterValueByInfo(ParameterInfo, ScalarValue);
				float ReadBack = 0.0f;
				const bool bResolved = MID.GetScalarParameterValue(ParameterInfo, ReadBack);
				if (!bResolved || ReadBack != ScalarValue)
				{
					LogBindingFailure(MID, ParameterInfo, TEXT("scalar override did not resolve to the bound value"));
					bSucceeded = false;
				}
				UE_CLOGF(bDumpMaterialParameters, LogArcTerrain, Warning,"[MaterialBind] param='%ls' assoc=%d layerindex=%d scalar=%.9f resolved=%ls",
					*ParameterName, static_cast<int32>(ParameterInfo.Association), ParameterInfo.Index,
					ScalarValue, (bResolved && ReadBack == ScalarValue) ? TEXT("yes") : TEXT("NO"));
			}
		}

		if (bDumpMaterialParameters && Expression.ParameterType == EMaterialParameterType::Scalar)
		{
			float Value = 0.0f;
			const bool bHasValue = MID.GetScalarParameterValue(ParameterInfo, Value);
			UE_LOGF(LogArcTerrain, Warning,"[MatParams]   scalar[%d] '%ls' assoc=%d layerindex=%d value=%.9f found=%ls",
				ExpressionIndex, *ParameterName, static_cast<int32>(ParameterInfo.Association),
				ParameterInfo.Index, Value, bHasValue ? TEXT("yes") : TEXT("no"));
		}
	}

	if (bDumpMaterialParameters)
	{
		UE_LOGF(LogArcTerrain, Warning,"[MatParams] expected texture param '%ls' compiled=%ls", *HeightAtlasName.ToString(),
			CompiledTextureNames.Contains(HeightAtlasName) ? TEXT("yes") : TEXT("NO"));
		for (int32 ArrayIndex = 0; ArrayIndex < WeightAtlases.Num(); ++ArrayIndex)
		{
			const FName ExpectedName(*FString::Printf(TEXT("ArcTerrainWeights%d"), ArrayIndex));
			UE_LOGF(LogArcTerrain, Warning,"[MatParams] expected texture param '%ls' compiled=%ls", *ExpectedName.ToString(),
				CompiledTextureNames.Contains(ExpectedName) ? TEXT("yes") : TEXT("NO"));
		}
		const FName ExpectedScalarNames[] = { HeightInvResName, WeightInvResName, CellSizeName };
		for (const FName& ExpectedName : ExpectedScalarNames)
		{
			UE_LOGF(LogArcTerrain, Warning,"[MatParams] expected scalar param '%ls' compiled=%ls", *ExpectedName.ToString(),
				CompiledScalarNames.Contains(ExpectedName) ? TEXT("yes") : TEXT("NO"));
		}
	}

	if (bDumpWeightConsistency)
	{
		const bool bHasShaderMap = Resource->GetGameThreadShaderMap() != nullptr;
		const int32 ExpectedWeightArraysFromChannels = ChannelSet != nullptr
			? FMath::DivideAndRoundUp(ChannelSet->Channels.Num(), 4) : -1;
		if (bHasShaderMap && ChannelSet != nullptr
			&& CompiledWeightArrayIndices.Num() != ExpectedWeightArraysFromChannels)
		{
			const FName Key(*FString::Printf(TEXT("%s|%d|%d"), *MID.GetFName().ToString(),
				CompiledWeightArrayIndices.Num(), ExpectedWeightArraysFromChannels));
			bool bAlreadyLogged = false;
			LoggedArrayCountMismatches.Add(Key, &bAlreadyLogged);
			if (!bAlreadyLogged)
			{
				UE_LOGF(LogArcTerrain, Warning,"[WeightParams] MID='%ls' compiled %d ArcTerrainWeights<N> array texture param(s), Config.ChannelSet's %d channel(s) expect %d.",
					*MID.GetName(), CompiledWeightArrayIndices.Num(), ChannelSet->Channels.Num(),
					ExpectedWeightArraysFromChannels);
			}
		}

		const FString Summary = FString::Printf(
			TEXT("[WeightParams] MID='%s' summary: numericparams=%d mask=%d array=%d unresolved=%d compiledweightarrays=%d boundweightarrays=%d channelsetchannels=%d expectedweightarrays=%d shadermap=%s"),
			*MID.GetName(), NumericExpressions.Num(), NumMaskParams, NumArrayParams, NumUnresolved,
			CompiledWeightArrayIndices.Num(), WeightAtlases.Num(),
			ChannelSet != nullptr ? ChannelSet->Channels.Num() : -1,
			ExpectedWeightArraysFromChannels, bHasShaderMap ? TEXT("present") : TEXT("absent"));
		const FName Identity = MID.GetFName();
		const FString* const LastSummary = LastSummaryByIdentity.Find(Identity);
		if (LastSummary == nullptr || *LastSummary != Summary)
		{
			UE_LOGF(LogArcTerrain, Log,"%ls", *Summary);
			LastSummaryByIdentity.Add(Identity, Summary);
		}
	}

	return bSucceeded ? EResult::Succeeded : EResult::Failed;
}
