// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "Engine/Scene.h"

#include "ArcTimeOfDayTypes.generated.h"

namespace ArcTimeOfDay
{
	inline constexpr int32 ModifierCapacity = 8;
}

struct FArcTimeOfDayModifierHandle
{
	int32 Index = INDEX_NONE;

	bool IsValid() const
	{
		return Index >= 0 && Index < ArcTimeOfDay::ModifierCapacity;
	}
};

struct FArcTimeOfDayResolvedWeatherValues
{
	float CloudCoverage = 0.0f;
	float Fog = 0.0f;
	float DustAmount = 0.0f;
	float CloudDirection = 0.0f;
	float WindIntensity = 0.0f;
	float Rain = 0.0f;
	float Snow = 0.0f;
	float FogSnowContribution = 0.0f;
};

USTRUCT(BlueprintType)
struct FArcModifiableFloat
{
	GENERATED_BODY()

	FArcModifiableFloat() = default;
	explicit FArcModifiableFloat(const float InBaseValue)
		: BaseValue(InBaseValue)
	{
	}

	float GetValue() const
	{
		return BaseValue + CachedSum;
	}

	float GetClampedValue(const float MinValue, const float MaxValue) const
	{
		return FMath::Clamp(GetValue(), MinValue, MaxValue);
	}

	void SetModifier(const FArcTimeOfDayModifierHandle Handle, const float Value)
	{
		if (!Handle.IsValid())
		{
			return;
		}
		CachedSum += Value - Values[Handle.Index];
		Values[Handle.Index] = Value;
	}

	void ClearModifier(const FArcTimeOfDayModifierHandle Handle)
	{
		SetModifier(Handle, 0.0f);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp)
	float BaseValue = 0.0f;

private:
	TStaticArray<float, ArcTimeOfDay::ModifierCapacity> Values{};
	float CachedSum = 0.0f;
};

USTRUCT(BlueprintType)
struct FArcModifiableDouble
{
	GENERATED_BODY()

	FArcModifiableDouble() = default;
	explicit FArcModifiableDouble(const double InBaseValue)
		: BaseValue(InBaseValue)
	{
	}

	double GetValue() const
	{
		return BaseValue + CachedSum;
	}

	double GetClampedValue(const double MinValue, const double MaxValue) const
	{
		return FMath::Clamp(GetValue(), MinValue, MaxValue);
	}

	double GetWrappedValue(const double Range) const
	{
		const double Remainder = FMath::Fmod(GetValue(), Range);
		return Remainder < 0.0 ? Remainder + Range : Remainder;
	}

	void SetModifier(const FArcTimeOfDayModifierHandle Handle, const double Value)
	{
		if (!Handle.IsValid())
		{
			return;
		}
		CachedSum += Value - Values[Handle.Index];
		Values[Handle.Index] = Value;
	}

	void ClearModifier(const FArcTimeOfDayModifierHandle Handle)
	{
		SetModifier(Handle, 0.0);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp)
	double BaseValue = 0.0;

private:
	TStaticArray<double, ArcTimeOfDay::ModifierCapacity> Values{};
	double CachedSum = 0.0;
};

USTRUCT(BlueprintType)
struct FArcModifiableInt
{
	GENERATED_BODY()

	FArcModifiableInt() = default;
	explicit FArcModifiableInt(const int32 InBaseValue)
		: BaseValue(InBaseValue)
	{
	}

	int32 GetValue() const
	{
		return BaseValue + CachedSum;
	}

	int32 GetClampedValue(const int32 MinValue, const int32 MaxValue) const
	{
		return FMath::Clamp(GetValue(), MinValue, MaxValue);
	}

	void SetModifier(const FArcTimeOfDayModifierHandle Handle, const int32 Value)
	{
		if (!Handle.IsValid())
		{
			return;
		}
		CachedSum += Value - Values[Handle.Index];
		Values[Handle.Index] = Value;
	}

	void ClearModifier(const FArcTimeOfDayModifierHandle Handle)
	{
		SetModifier(Handle, 0);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp)
	int32 BaseValue = 0;

private:
	TStaticArray<int32, ArcTimeOfDay::ModifierCapacity> Values{};
	int32 CachedSum = 0;
};

UENUM(BlueprintType)
enum class EArcTimeOfDaySkyMode : uint8
{
	VolumetricClouds UMETA(DisplayName="Volumetric Clouds"),
	StaticClouds UMETA(DisplayName="Static Clouds"),
	DynamicClouds2D UMETA(DisplayName="2D Dynamic Clouds"),
	NoClouds UMETA(DisplayName="No Clouds"),
	VolumetricAurora UMETA(DisplayName="Volumetric Aurora"),
	Space UMETA(DisplayName="Space"),
	VoxelClouds UMETA(DisplayName="Voxel Clouds")
};

UENUM(BlueprintType)
enum class EArcTimeOfDayColorMode : uint8
{
	SkyAtmosphere UMETA(DisplayName="Sky Atmosphere"),
	SimplifiedColor UMETA(DisplayName="Simplified Color")
};

UENUM(BlueprintType)
enum class EArcTimeOfDaySkyLightMode : uint8
{
	CaptureBased UMETA(DisplayName="Capture Based"),
	CustomCubemap UMETA(DisplayName="Custom Cubemap"),
	CubemapDynamicColor UMETA(DisplayName="Cubemap with Dynamic Color Tinting")
};

UENUM(BlueprintType)
enum class EArcTimeOfDayFogColorMode : uint8
{
	Automatic UMETA(DisplayName="Automatic"),
	UDSFogSettings UMETA(DisplayName="UDS Fog Settings"),
	SkyAtmosphere UMETA(DisplayName="Sky Atmosphere")
};

UENUM(BlueprintType)
enum class EArcTimeOfDayVolumetricRenderingMode : uint8
{
	FidelityAndPerformance UMETA(DisplayName="Fidelity + Performance"),
	MeshIntersectionsFastMovementPerformance UMETA(DisplayName="Mesh Intersections + Fast Movement + Performance"),
	MeshIntersectionsFastMovementFidelity UMETA(DisplayName="Mesh Intersections + Fast Movement + Fidelity"),
	FullCinematicQuality UMETA(DisplayName="Full Cinematic Quality")
};

UENUM(BlueprintType)
enum class EArcTimeOfDayProjectMode : uint8
{
	GameRealtime UMETA(DisplayName="Game / Real-time (Non VR)"),
	CinematicOffline UMETA(DisplayName="Cinematic / Offline"),
	VirtualReality UMETA(DisplayName="Virtual Reality")
};

UENUM(BlueprintType)
enum class EArcTimeOfDayLensFlareType : uint8
{
	ZoomChromatic UMETA(DisplayName="Zoom Chromatic"),
	Anamorphic UMETA(DisplayName="Anamorphic"),
	Prime35mm UMETA(DisplayName="35mm Prime")
};

UENUM(BlueprintType)
enum class EArcTimeOfDayPathTracerSkyResolution : uint8
{
	Resolution2048 UMETA(DisplayName="2048"),
	Resolution4096 UMETA(DisplayName="4096")
};

UENUM(BlueprintType)
enum class EArcTimeOfDaySpaceParent : uint8
{
	NoParent UMETA(DisplayName="No Parent"),
	Sun UMETA(DisplayName="Sun"),
	Moon UMETA(DisplayName="Moon")
};

UENUM(BlueprintType)
enum class EArcTimeOfDayPlanetLightSource : uint8
{
	Sun UMETA(DisplayName="Sun"),
	Moon UMETA(DisplayName="Moon"),
	Custom UMETA(DisplayName="Custom")
};

USTRUCT(BlueprintType)
struct FArcTimeOfDayPostProcessStage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="Used to label this post process component in the array. Purely for organization to help you keep track of which entry is which.")) FString Label;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Post Process Settings", ToolTip="The post process settings this component will use.")) FPostProcessSettings PostProcessSettings;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="The priority of the post process component. Will determine the order they're applied relative to other components/post process volumes.")) double Priority = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Disable During Day", ToolTip="If checked, Blend Weight will be turned down when its daytime.")) bool bDisableDuringDay = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Disable at Dawn/Dusk", ToolTip="If checked, Blend Weight will be turned down when it's dawn/dusk.")) bool bDisableAtDawnDusk = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Disable at Night", ToolTip="If checked, Blend Weight will be turned down when it's night time.")) bool bDisableAtNight = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Disable if Overcast", ToolTip="If checked, Blend Weight will be turned down when the sky is overcast.")) bool bDisableIfOvercast = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Disable if Not Overcast", ToolTip="If checked, Blend Weight will be turned down when the sky is mostly clear of clouds.")) bool bDisableIfNotOvercast = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Disable if Foggy", ToolTip="If checked, Blend Weight will be turned down when Fog is high.")) bool bDisableIfFoggy = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Disable if Not Foggy", ToolTip="If checked, Blend Weight will be turned down when Fog is low.")) bool bDisableIfNotFoggy = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Disable if Dusty", ToolTip="If checked, Blend Weight will be turned down when Dust Amount is high.")) bool bDisableIfDusty = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Disable if Not Dusty", ToolTip="If checked, Blend Weight will be turned down when Dust Amount is low.")) bool bDisableIfNotDusty = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Disable if Inside Interior", ToolTip="If Interior Adjustments are active, Blend Weight will be turned down when the player camera is inside an interior.")) bool bDisableIfInsideInterior = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Disable if Outside Interior", ToolTip="If Interior Adjustments are active, Blend Weight will be turned down when the player camera is outside/unoccluded.")) bool bDisableIfOutsideInterior = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Disable if Raining", ToolTip="If checked, Blend Weight will be turned down when rain is active on UDW (ignored if UDW is not used)")) bool bDisableIfRaining = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Disable if Not Raining", ToolTip="If checked, Blend Weight will be turned down when rain is inactive on UDW (ignored if UDW is not used)")) bool bDisableIfNotRaining = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Disable if Snowing", ToolTip="If checked, Blend Weight will be turned down when snow is active on UDW (ignored if UDW is not used)")) bool bDisableIfSnowing = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Disable if Not Snowing", ToolTip="If checked, Blend Weight will be turned down when snow is inactive on UDW (ignored if UDW is not used)")) bool bDisableIfNotSnowing = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient) bool bStatic = false;
};

USTRUCT(BlueprintType)
struct FArcTimeOfDaySpaceLayerDecal
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="Determines if this decal will move with the sun, moon, or not move.")) EArcTimeOfDaySpaceParent Parent = EArcTimeOfDaySpaceParent::NoParent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Relative Rotation", ToolTip="The rotation of the decal, relative to the actor if it has no parent, or relative to the sun or moon if parented to those.")) FRotator RelativeRotation = FRotator(30.0, 30.0, 0.0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="The visual size of the decal")) double Scale = 500.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Scale Y Axis", ToolTip="Scale the Y axis of the decal rectangle, relative to the X axis. Values other than 1 will be non square.")) double ScaleYAxis = 1.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Decal Material", ToolTip="The material used for this decal. Should be a Deferred Decal material which outputs to the Base Color.")) TSoftObjectPtr<class UMaterialInterface> DecalMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Scalar Parameter Settings", ToolTip="Can be used to configure scalar parameters on the dynamic material instance for this decal.")) TMap<FName, double> ScalarParameterSettings;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Color Parameter Settings", ToolTip="Can be used to configure color parameters on the dynamic material instance for this decal.")) TMap<FName, FLinearColor> ColorParameterSettings;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Texture Parameter Settings", ToolTip="Can be used to configure texture parameters on the dynamic material instance for this decal.")) TMap<FName, TSoftObjectPtr<class UTexture>> TextureParameterSettings;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Sort Index Offset", ToolTip="Offset sort index to enforce sorting with planets/nebula.")) int32 SortIndexOffset = 0;
};

USTRUCT(BlueprintType)
struct FArcTimeOfDaySpacePlanet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="Determines if this space object will move with the sun, moon, or not move.")) EArcTimeOfDaySpaceParent Parent = EArcTimeOfDaySpaceParent::NoParent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Relative Rotation", ToolTip="The rotation of the object, relative to the actor if it has no parent, or relative to the sun or moon if parented to those.")) FRotator RelativeRotation = FRotator(30.0, 30.0, 15.0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="The visual size of the planet/moon")) double Scale = 500.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Color Texture", ToolTip="The texture used for the color. Should be flat/unshaded.")) TSoftObjectPtr<class UTexture2D> ColorTexture;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Color Texture Tint", ToolTip="Tints the color texture overall")) FLinearColor ColorTextureTint = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Normal Map", ToolTip="Normal map for adding detail to shading from the sun")) TSoftObjectPtr<UTexture2D> NormalMap;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Normal Map Strength", ToolTip="The strength of the normal map's effect on the shading.")) double NormalMapStrength = 1.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Dark Side Tint", ToolTip="The color multiplied over the dark side of the planet. Can be set to white to make an object that doesn't visibly have shading.")) FLinearColor DarkSideTint = FLinearColor(0.017728f, 0.023713f, 0.041667f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Light Side Tint", ToolTip="The color multiplied over the light side of the planet. Would typically be white, but can be used to tint the light side as needed.")) FLinearColor LightSideTint = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Terminator Thickness", ToolTip="Multiplied over the shading gradient, to control how wide/tight the terminator (the border between the light and dark side) is.")) double TerminatorThickness = 1.5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Terminator Power", ToolTip="An exponent for the shading gradient, to control the curve of falloff from light to dark.")) double TerminatorPower = 1.5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Terminator Offset", ToolTip="An offset for the shading gradient, to have more or less of the sphere be lit.")) double TerminatorOffset = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Terminator Tint", ToolTip="A tint color applied in the middle of the terminator. Useful for planets with atmospheres like Earth, to imitate the way light filters warm along the terminator.")) FLinearColor TerminatorTint = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Light Vector", ToolTip="Choose the angle that the material will appear lit from. The sun, the moon, or the custom vector (adjustable below)")) EArcTimeOfDayPlanetLightSource LightVector = EArcTimeOfDayPlanetLightSource::Sun;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Custom Light Vector", ToolTip="If the Light Vector setting is Custom, this vector will be used for the direction light shades this planet.")) FVector CustomLightVector = FVector(1.0, 0.0, 0.0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ToolTip="Glow around the planet/moon, like light scattering through the local atmosphere")) double Glow = 2.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Glow Color", ToolTip="Tints the color of the glow added by the above setting")) FLinearColor GlowColor = FLinearColor(0.598958f, 0.598958f, 0.598958f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Glow Scale", ToolTip="Scales the glow sprite relative to the planet/moon scale")) double GlowScale = 4.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Render Atmosphere", ToolTip="If true, atmosphere color and a ring of atmosphere \"glow\" will encircle the planet.")) bool bRenderAtmosphere = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Atmosphere Strength", ToolTip="The opacity of the atmosphere color and glow")) double AtmosphereStrength = 0.65;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Atmosphere Thickness", ToolTip="The size of the atmosphere's \"ring\" around the planet's sphere.")) double AtmosphereThickness = 1.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Atmosphere Color", ToolTip="The color of the atmosphere, applied if Render Atmosphere is true.")) FLinearColor AtmosphereColor = FLinearColor(0.150146f, 0.286404f, 0.640625f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Emissive Texture", ToolTip="An emissive color texture or grayscale mask. One possible use is for city lights visible on the night side of a planet.")) TSoftObjectPtr<UTexture2D> EmissiveTexture;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Emissive Light Side Tint", ToolTip="Tints the emissive texture on the lit side of the planet")) FLinearColor EmissiveLightSideTint = FLinearColor::Black;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Emissive Dark Side Tint", ToolTip="Tints the emissive texture on the dark side of the planet")) FLinearColor EmissiveDarkSideTint = FLinearColor::Black;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Render Ring", ToolTip="Render planetary rings around the sphere")) bool bRenderRing = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Ring Pitch", ToolTip="The pitch of the ring's rotation")) double RingPitch = -15.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Ring Roll", ToolTip="The roll of the ring's rotation")) double RingRoll = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Ring Distance", ToolTip="The innter distance of the ring, from the planet surface.")) double RingDistance = 1.25;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Ring Width", ToolTip="The thickness of the ring")) double RingWidth = 2.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Ring Texture", ToolTip="The texture used for the ring. Only sampled on X axis, so should be 1 pixel tall. RGB contains color and A is used for opacity.")) TSoftObjectPtr<UTexture2D> RingTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Textures/Space/Rings/Planetary_Ring_1.Planetary_Ring_1")));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Ring Tint Color", ToolTip="Tint the color of the ring. Multiplied over the color from the Ring Texture.")) FLinearColor RingTintColor = FLinearColor(0.541667f, 0.462836f, 0.394965f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Ring Opacity", ToolTip="Scales the alpha channel of the ring texture")) double RingOpacity = 0.3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Ring Shadow Brightness", ToolTip="Sets how much the planet's shadow darkens the ring.")) double RingShadowBrightness = 0.15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Ring Shadow Softness", ToolTip="Larger values will make the planet's shadow on the ring softer.")) double RingShadowSoftness = 1.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Ring Around Local Planet", ToolTip="This is for if you want to use this planet/moon to render a ring around the local planet (the planet you're on). When this is true, only the ring decal will be rendered, centered around the local planet.")) bool bRingAroundLocalPlanet = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Mesh Deform Texture", ToolTip="Alpha texture used to deform the surface along the vertex normal (for irregularly shaped moons, asteroids)")) TSoftObjectPtr<UTexture2D> MeshDeformTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UltraDynamicSky/Textures/Space/Planets_Moons/Deform_Noise.Deform_Noise")));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Mesh Deform Strength", ToolTip="Amount to deform the mesh along the vertex normal, relative to the size of the object.")) double MeshDeformStrength = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Mesh Deform Center Level", ToolTip="Defines the level of brightness in the mesh deform texture which is unmoved by the deformation.")) double MeshDeformCenterLevel = 0.5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Mesh Deform Texture Offset", ToolTip="The first two channels offset the mesh deform texture in X and Y space. The third scales the texture.")) FVector MeshDeformTextureOffset = FVector(0.0, 0.0, 1.0);
};

USTRUCT(BlueprintType)
struct FArcTimeOfDayAdditionalCloudLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Above Main Layer", ToolTip="Layers with this enabled will be added in their array order above the main layer, counting up from the bottom. Layers with this disabled will do the opposite, with each layer below the last."))
	bool bAboveMainLayer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Layer Height", ToolTip="Scales the height of this layer of clouds relative to the main layer height."))
	double LayerHeight = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Margin", ToolTip="Scales the amount of space between this layer and the layer below (if Above Main Layer is true) or the layer above (if Above Main Layer is false)"))
	double Margin = 0.1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Cloud Formation Texture Scale", ToolTip="Scales the cloud formation texture on this layer."))
	FVector2D CloudFormationTextureScale = FVector2D(1.0, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Formation Time Offset", ToolTip="Used to offset the cloud formation for this layer, so it doesn't just look like a clone of the layer beneath it."))
	double FormationTimeOffset = 0.17;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Extinction Scale", ToolTip="Multiplied by the cloud extinction for this layer."))
	double ExtinctionScale = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Inherited Cloud Coverage", ToolTip="Scales the amount of cloud coverage this layer \"inherits\" from the main layer. This is the coverage level set in Basic Controls, and will vary with dynamic weather."))
	double InheritedCloudCoverage = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Painted Cloud Coverage", ToolTip="Scales the amount that this layer recieves coverage from the Volumetric Cloud Painter and Radial Storms."))
	double PaintedCloudCoverage = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Independent Cloud Coverage", ToolTip="Adds a constant amount of cloud coverage to this layer, which does not vary with the main layer's coverage."))
	double IndependentCloudCoverage = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Formation Texture Mip Offset", ToolTip="Offsets the mip sampled for the cloud formation texture on this layer."))
	int32 FormationTextureMipOffset = 0;
};

enum class EArcTimeOfDayCachedProperty : uint8
{
	CloudWispsOpacity = 3, MoonScale = 5, MoonPhaseAngle = 7, MoonMaterialIntensity = 8, MoonLitPercent = 9, StarsOrbit = 11,
	StarsOrientation = 12, SunRootVector = 13, MoonRootVector = 14, CloudShadowsLightVector = 16, CloudShadowsCancel = 17, MoonXAxisColor = 20, ShadingOffsetVector = 22, NightSkyGlowColor = 23,
	SkyOverallIntensity = 24, SkyLitIntensity = 25, SkySaturation = 26, ExposureBias = 27, SkyBaseColor = 29, CloudLightColor = 30, CloudDarkColor = 31, MoonScatteringColor = 33, StarsColor = 36, AuroraIntensity = 37, ShineIntensity = 38, SunHighlightRadius = 39, SunHighlightIntensity = 40,
	SunLightIntensity = 41, SunLightCastsShadows = 42, SunSpecularScale = 43, SunDisabledBrightness = 44, SunLightColor = 45, SunLightSourceAngle = 46, FogInscatteringColor = 50, FogDirectionalInscatteringColor = 51,
	SkyAmbientColor = 52, FogDensity = 53, FogSkyAtmosphereContribution = 54, FogHeightFalloff = 56, FogStartDistance = 57, AmbientFogColor = 58, FilteredMoonLightIntensity = 61,
	MoonCastingShadows = 63, MoonLightSpecularScale = 64, MoonLightSourceAngle = 65, SkyLightColor = 66, SkyLightIntensity = 67, SkyLightLowerHemisphereColor = 68,
	SkyAtmosphereLuminance = 69, MieAnisotropy = 70, MieScatteringScale = 71, RayleighScatteringColor = 72, OvercastTurbulenceStrength = 74, VolumetricCloudAlbedo = 75,
	VolumetricCloudMaxTraceDistance = 79, MultiscatteringOcclusion = 89, MacroVariation = 90, VolumetricCloudLerpToSimplified = 91, StaticCloudsLightingMask = 99,
	VolumetricCloudFloorVariationClear = 103, VolumetricCloudFloorVariationColor = 105, SkyMaterialContrastMidpoint = 107, CloudShadowTraceDistance = 108, VolumetricCloudViewSampleScale = 109, SpaceLayerBrightness = 113,
	DirectionalInscatteringExponent = 111, CloudWispsGradientVector = 112, VolumetricCloudMultiscatteringPhase1 = 115, SunScatteringColor = 116, VolCloudInnerEmitLimit = 117, SkyAtmoAbsorptionColor = 119, CloudWispsColor = 120, MultiscatteringIntensity = 121,
	VolumetricCloudFloorVariationCloudy = 123, VolumetricCloudLightRaysColor = 127, VolumetricCloudSkyAtmoContribution = 128, StaticCloudsTintColor = 130, StaticCloudsShadowColor = 131, VolumetricFogAlbedo = 134, HighFrequencyNoiseAmount = 137, SunVolumetricScatteringIntensity = 138, MoonVolumetricScatteringIntensity = 139, VolumetricFogGlobalMatExtinction = 141, VolumetricFogGroundFogExtinction = 142, FogMaxOpacity = 145,
	PhaseBlend = 146, VolumetricCloudMaterialSwitch = 147, TwoDCloudTint = 149, StaticCloudsDynamicRotation = 150, AuroraPhase = 151, StarsPhase = 152, MoonMaterialColor = 153, PaintedCloudCoverageOpacityLayer1 = 154, EnableLensFlarePostProcess = 156, CloudWispsMovementVector = 164,
	MoonAtmosphereLightIndex = 167,
	Count = 168
};

enum class EArcTimeOfDayWriterGroup : uint8
{
	None,
	Immediate,
	Max,
	High1,
	High2,
	High3,
	High4,
	Low1,
	Low2,
	Low3,
	Low4,
	Low5,
	Low6,
	Low7,
	Low8
};

struct FArcTimeOfDayCachedValue
{
	float FloatValue = 0.0f;
	FLinearColor ColorValue = FLinearColor::Black;
	bool bInitialized = false;
	bool bReached = false;
	bool bPending = false;
};

struct FArcTimeOfDayCalendarData
{
	TArray<int32> MonthLengths;
	TArray<FString> DaysOfWeek;
	bool bUsesLeapYear = true;
	int32 LeapYearMonth = 2;
	int32 WinterSolsticeOffset = 355;
	int32 DayOfWeekOffset = 0;
};

struct FArcTimeOfDayAstronomyResult
{
	FVector SunDirection = FVector::UpVector;
	FVector MoonDirection = FVector::DownVector;
	FVector MoonAlignment = FVector::ForwardVector;
	FVector MoonPhaseAngle = FVector::ZeroVector;
	FLinearColor StarsOrientation = FLinearColor::Black;
	float StarsOrbit = 0.0f;
	double MoonDistance = 385000.56;
};
