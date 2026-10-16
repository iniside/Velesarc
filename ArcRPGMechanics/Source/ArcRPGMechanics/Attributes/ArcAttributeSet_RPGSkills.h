// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"

#include "ArcAttributeSet_RPGSkills.generated.h"

USTRUCT()
struct ARCRPGMECHANICS_API FArcAttributeSet_RPGSkills : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) FArcAttribute OneHanded;
	UPROPERTY(EditAnywhere) FArcAttribute MaxOneHanded;
	UPROPERTY(EditAnywhere) FArcAttribute TwoHanded;
	UPROPERTY(EditAnywhere) FArcAttribute MaxTwoHanded;
	UPROPERTY(EditAnywhere) FArcAttribute Polearms;
	UPROPERTY(EditAnywhere) FArcAttribute MaxPolearms;
	UPROPERTY(EditAnywhere) FArcAttribute Unarmed;
	UPROPERTY(EditAnywhere) FArcAttribute MaxUnarmed;
	UPROPERTY(EditAnywhere) FArcAttribute Bows;
	UPROPERTY(EditAnywhere) FArcAttribute MaxBows;
	UPROPERTY(EditAnywhere) FArcAttribute Crossbows;
	UPROPERTY(EditAnywhere) FArcAttribute MaxCrossbows;
	UPROPERTY(EditAnywhere) FArcAttribute Throwing;
	UPROPERTY(EditAnywhere) FArcAttribute MaxThrowing;
	UPROPERTY(EditAnywhere) FArcAttribute Shields;
	UPROPERTY(EditAnywhere) FArcAttribute MaxShields;
	UPROPERTY(EditAnywhere) FArcAttribute Parrying;
	UPROPERTY(EditAnywhere) FArcAttribute MaxParrying;
	UPROPERTY(EditAnywhere) FArcAttribute Dodging;
	UPROPERTY(EditAnywhere) FArcAttribute MaxDodging;
	UPROPERTY(EditAnywhere) FArcAttribute Leadership;
	UPROPERTY(EditAnywhere) FArcAttribute MaxLeadership;

	UPROPERTY(EditAnywhere) FArcAttribute Athletics;
	UPROPERTY(EditAnywhere) FArcAttribute MaxAthletics;
	UPROPERTY(EditAnywhere) FArcAttribute Swimming;
	UPROPERTY(EditAnywhere) FArcAttribute MaxSwimming;
	UPROPERTY(EditAnywhere) FArcAttribute Climbing;
	UPROPERTY(EditAnywhere) FArcAttribute MaxClimbing;
	UPROPERTY(EditAnywhere) FArcAttribute Acrobatics;
	UPROPERTY(EditAnywhere) FArcAttribute MaxAcrobatics;
	UPROPERTY(EditAnywhere) FArcAttribute Riding;
	UPROPERTY(EditAnywhere) FArcAttribute MaxRiding;

	UPROPERTY(EditAnywhere) FArcAttribute Stealth;
	UPROPERTY(EditAnywhere) FArcAttribute MaxStealth;
	UPROPERTY(EditAnywhere) FArcAttribute Lockpicking;
	UPROPERTY(EditAnywhere) FArcAttribute MaxLockpicking;
	UPROPERTY(EditAnywhere) FArcAttribute Pickpocketing;
	UPROPERTY(EditAnywhere) FArcAttribute MaxPickpocketing;
	UPROPERTY(EditAnywhere) FArcAttribute Camouflage;
	UPROPERTY(EditAnywhere) FArcAttribute MaxCamouflage;

	UPROPERTY(EditAnywhere) FArcAttribute Toughness;
	UPROPERTY(EditAnywhere) FArcAttribute MaxToughness;
	UPROPERTY(EditAnywhere) FArcAttribute FieldMedicine;
	UPROPERTY(EditAnywhere) FArcAttribute MaxFieldMedicine;
	UPROPERTY(EditAnywhere) FArcAttribute Hunting;
	UPROPERTY(EditAnywhere) FArcAttribute MaxHunting;
	UPROPERTY(EditAnywhere) FArcAttribute Survival;
	UPROPERTY(EditAnywhere) FArcAttribute MaxSurvival;
	UPROPERTY(EditAnywhere) FArcAttribute Trapping;
	UPROPERTY(EditAnywhere) FArcAttribute MaxTrapping;

	UPROPERTY(EditAnywhere) FArcAttribute Carpentry;
	UPROPERTY(EditAnywhere) FArcAttribute MaxCarpentry;
	UPROPERTY(EditAnywhere) FArcAttribute Tailoring;
	UPROPERTY(EditAnywhere) FArcAttribute MaxTailoring;
	UPROPERTY(EditAnywhere) FArcAttribute Leatherworking;
	UPROPERTY(EditAnywhere) FArcAttribute MaxLeatherworking;
	UPROPERTY(EditAnywhere) FArcAttribute Tanning;
	UPROPERTY(EditAnywhere) FArcAttribute MaxTanning;
	UPROPERTY(EditAnywhere) FArcAttribute Armorsmithing;
	UPROPERTY(EditAnywhere) FArcAttribute MaxArmorsmithing;
	UPROPERTY(EditAnywhere) FArcAttribute Weaponsmithing;
	UPROPERTY(EditAnywhere) FArcAttribute MaxWeaponsmithing;
	UPROPERTY(EditAnywhere) FArcAttribute Blacksmithing;
	UPROPERTY(EditAnywhere) FArcAttribute MaxBlacksmithing;
	UPROPERTY(EditAnywhere) FArcAttribute Alchemy;
	UPROPERTY(EditAnywhere) FArcAttribute MaxAlchemy;
	UPROPERTY(EditAnywhere) FArcAttribute JewelryMaking;
	UPROPERTY(EditAnywhere) FArcAttribute MaxJewelryMaking;
	UPROPERTY(EditAnywhere) FArcAttribute Butchering;
	UPROPERTY(EditAnywhere) FArcAttribute MaxButchering;
	UPROPERTY(EditAnywhere) FArcAttribute Cooking;
	UPROPERTY(EditAnywhere) FArcAttribute MaxCooking;
	UPROPERTY(EditAnywhere) FArcAttribute WinemakingAndBrewing;
	UPROPERTY(EditAnywhere) FArcAttribute MaxWinemakingAndBrewing;
	UPROPERTY(EditAnywhere) FArcAttribute Baking;
	UPROPERTY(EditAnywhere) FArcAttribute MaxBaking;
	UPROPERTY(EditAnywhere) FArcAttribute Fletching;
	UPROPERTY(EditAnywhere) FArcAttribute MaxFletching;
	UPROPERTY(EditAnywhere) FArcAttribute Mining;
	UPROPERTY(EditAnywhere) FArcAttribute MaxMining;
	UPROPERTY(EditAnywhere) FArcAttribute Woodcutting;
	UPROPERTY(EditAnywhere) FArcAttribute MaxWoodcutting;
	UPROPERTY(EditAnywhere) FArcAttribute Skinning;
	UPROPERTY(EditAnywhere) FArcAttribute MaxSkinning;
	UPROPERTY(EditAnywhere) FArcAttribute Herbalism;
	UPROPERTY(EditAnywhere) FArcAttribute MaxHerbalism;
	UPROPERTY(EditAnywhere) FArcAttribute Fishing;
	UPROPERTY(EditAnywhere) FArcAttribute MaxFishing;
	UPROPERTY(EditAnywhere) FArcAttribute Farming;
	UPROPERTY(EditAnywhere) FArcAttribute MaxFarming;
	UPROPERTY(EditAnywhere) FArcAttribute AnimalHusbandry;
	UPROPERTY(EditAnywhere) FArcAttribute MaxAnimalHusbandry;
	UPROPERTY(EditAnywhere) FArcAttribute ArmorEnchanting;
	UPROPERTY(EditAnywhere) FArcAttribute MaxArmorEnchanting;
	UPROPERTY(EditAnywhere) FArcAttribute WeaponEnchanting;
	UPROPERTY(EditAnywhere) FArcAttribute MaxWeaponEnchanting;
	UPROPERTY(EditAnywhere) FArcAttribute ToolEnchanting;
	UPROPERTY(EditAnywhere) FArcAttribute MaxToolEnchanting;
	UPROPERTY(EditAnywhere) FArcAttribute ClothEnchanting;
	UPROPERTY(EditAnywhere) FArcAttribute MaxClothEnchanting;
	UPROPERTY(EditAnywhere) FArcAttribute LeatherEnchanting;
	UPROPERTY(EditAnywhere) FArcAttribute MaxLeatherEnchanting;

	UPROPERTY(EditAnywhere) FArcAttribute Channeling;
	UPROPERTY(EditAnywhere) FArcAttribute MaxChanneling;
	UPROPERTY(EditAnywhere) FArcAttribute EntropyControl;
	UPROPERTY(EditAnywhere) FArcAttribute MaxEntropyControl;
	UPROPERTY(EditAnywhere) FArcAttribute Weaving;
	UPROPERTY(EditAnywhere) FArcAttribute MaxWeaving;
	UPROPERTY(EditAnywhere) FArcAttribute Ritualism;
	UPROPERTY(EditAnywhere) FArcAttribute MaxRitualism;
	UPROPERTY(EditAnywhere) FArcAttribute Aiming;
	UPROPERTY(EditAnywhere) FArcAttribute MaxAiming;
	UPROPERTY(EditAnywhere) FArcAttribute Concentration;
	UPROPERTY(EditAnywhere) FArcAttribute MaxConcentration;
	UPROPERTY(EditAnywhere) FArcAttribute Imposition;
	UPROPERTY(EditAnywhere) FArcAttribute MaxImposition;
	UPROPERTY(EditAnywhere) FArcAttribute Insight;
	UPROPERTY(EditAnywhere) FArcAttribute MaxInsight;

	UPROPERTY(EditAnywhere) FArcAttribute Fire;
	UPROPERTY(EditAnywhere) FArcAttribute MaxFire;
	UPROPERTY(EditAnywhere) FArcAttribute Stasis;
	UPROPERTY(EditAnywhere) FArcAttribute MaxStasis;
	UPROPERTY(EditAnywhere) FArcAttribute Electricity;
	UPROPERTY(EditAnywhere) FArcAttribute MaxElectricity;
	UPROPERTY(EditAnywhere) FArcAttribute Shadow;
	UPROPERTY(EditAnywhere) FArcAttribute MaxShadow;
	UPROPERTY(EditAnywhere) FArcAttribute Darkness;
	UPROPERTY(EditAnywhere) FArcAttribute MaxDarkness;
	UPROPERTY(EditAnywhere) FArcAttribute Light;
	UPROPERTY(EditAnywhere) FArcAttribute MaxLight;
	UPROPERTY(EditAnywhere) FArcAttribute Death;
	UPROPERTY(EditAnywhere) FArcAttribute MaxDeath;
	UPROPERTY(EditAnywhere) FArcAttribute Plague;
	UPROPERTY(EditAnywhere) FArcAttribute MaxPlague;
	UPROPERTY(EditAnywhere) FArcAttribute Force;
	UPROPERTY(EditAnywhere) FArcAttribute MaxForce;
	UPROPERTY(EditAnywhere) FArcAttribute Resonance;
	UPROPERTY(EditAnywhere) FArcAttribute MaxResonance;
	UPROPERTY(EditAnywhere) FArcAttribute Life;
	UPROPERTY(EditAnywhere) FArcAttribute MaxLife;
	UPROPERTY(EditAnywhere) FArcAttribute Prime;
	UPROPERTY(EditAnywhere) FArcAttribute MaxPrime;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, OneHanded)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxOneHanded)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, TwoHanded)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxTwoHanded)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Polearms)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxPolearms)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Unarmed)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxUnarmed)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Bows)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxBows)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Crossbows)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxCrossbows)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Throwing)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxThrowing)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Shields)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxShields)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Parrying)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxParrying)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Dodging)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxDodging)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Leadership)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxLeadership)

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Athletics)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxAthletics)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Swimming)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxSwimming)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Climbing)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxClimbing)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Acrobatics)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxAcrobatics)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Riding)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxRiding)

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Stealth)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxStealth)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Lockpicking)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxLockpicking)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Pickpocketing)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxPickpocketing)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Camouflage)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxCamouflage)

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Toughness)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxToughness)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, FieldMedicine)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxFieldMedicine)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Hunting)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxHunting)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Survival)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxSurvival)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Trapping)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxTrapping)

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Carpentry)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxCarpentry)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Tailoring)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxTailoring)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Leatherworking)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxLeatherworking)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Tanning)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxTanning)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Armorsmithing)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxArmorsmithing)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Weaponsmithing)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxWeaponsmithing)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Blacksmithing)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxBlacksmithing)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Alchemy)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxAlchemy)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, JewelryMaking)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxJewelryMaking)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Butchering)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxButchering)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Cooking)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxCooking)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, WinemakingAndBrewing)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxWinemakingAndBrewing)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Baking)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxBaking)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Fletching)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxFletching)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Mining)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxMining)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Woodcutting)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxWoodcutting)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Skinning)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxSkinning)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Herbalism)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxHerbalism)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Fishing)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxFishing)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Farming)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxFarming)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, AnimalHusbandry)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxAnimalHusbandry)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, ArmorEnchanting)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxArmorEnchanting)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, WeaponEnchanting)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxWeaponEnchanting)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, ToolEnchanting)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxToolEnchanting)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, ClothEnchanting)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxClothEnchanting)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, LeatherEnchanting)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxLeatherEnchanting)

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Channeling)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxChanneling)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, EntropyControl)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxEntropyControl)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Weaving)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxWeaving)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Ritualism)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxRitualism)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Aiming)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxAiming)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Concentration)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxConcentration)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Imposition)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxImposition)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Insight)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxInsight)

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Fire)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxFire)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Stasis)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxStasis)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Electricity)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxElectricity)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Shadow)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxShadow)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Darkness)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxDarkness)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Light)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxLight)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Death)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxDeath)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Plague)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxPlague)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Force)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxForce)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Resonance)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxResonance)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Life)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxLife)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, Prime)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGSkills, MaxPrime)
};

template <>
struct TMassFragmentTraits<FArcAttributeSet_RPGSkills>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
