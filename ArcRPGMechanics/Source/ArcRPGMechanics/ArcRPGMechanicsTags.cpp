// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRPGMechanicsTags.h"

namespace ArcRPGMechanicsTags
{
	UE_DEFINE_GAMEPLAY_TAG(Damage_Physical, "ArcEffect.Damage.Physical");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Magical, "ArcEffect.Damage.Magical");

	UE_DEFINE_GAMEPLAY_TAG(Damage_Slash, "ArcEffect.Damage.Physical.Slash");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Pierce, "ArcEffect.Damage.Physical.Pierce");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Crush, "ArcEffect.Damage.Physical.Crush");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Fire, "ArcEffect.Damage.Magical.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Cold, "ArcEffect.Damage.Magical.Cold");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Lightning, "ArcEffect.Damage.Magical.Lightning");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Necrotic, "ArcEffect.Damage.Magical.Necrotic");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Poison, "ArcEffect.Damage.Magical.Poison");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Acid, "ArcEffect.Damage.Magical.Acid");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Sonic, "ArcEffect.Damage.Magical.Sonic");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Radiant, "ArcEffect.Damage.Magical.Radiant");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Void, "ArcEffect.Damage.Magical.Void");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Shadow, "ArcEffect.Damage.Magical.Shadow");

	UE_DEFINE_GAMEPLAY_TAG(HitZone_Torso, "ArcMech.Hit.Zone.Torso");
	UE_DEFINE_GAMEPLAY_TAG(HitZone_Legs, "ArcMech.Hit.Zone.Legs");
	UE_DEFINE_GAMEPLAY_TAG(HitZone_Head, "ArcMech.Hit.Zone.Head");
	UE_DEFINE_GAMEPLAY_TAG(HitZone_Arms, "ArcMech.Hit.Zone.Arms");
	UE_DEFINE_GAMEPLAY_TAG(HitZone_Feet, "ArcMech.Hit.Zone.Feet");

	UE_DEFINE_GAMEPLAY_TAG(AttackClass, "ArcRPG.AttackClass");
	UE_DEFINE_GAMEPLAY_TAG(AttackClass_Melee, "ArcRPG.AttackClass.Melee");
	UE_DEFINE_GAMEPLAY_TAG(AttackClass_Unarmed, "ArcRPG.AttackClass.Melee.Unarmed");
	UE_DEFINE_GAMEPLAY_TAG(AttackClass_Ranged, "ArcRPG.AttackClass.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(AttackClass_Spell, "ArcRPG.AttackClass.Spell");
	UE_DEFINE_GAMEPLAY_TAG(AttackClass_Weapon, "ArcRPG.AttackClass.Weapon");

	UE_DEFINE_GAMEPLAY_TAG(WeaponType, "ArcRPG.WeaponType");
	UE_DEFINE_GAMEPLAY_TAG(WeaponType_OneHanded, "ArcRPG.WeaponType.OneHanded");
	UE_DEFINE_GAMEPLAY_TAG(WeaponType_TwoHanded, "ArcRPG.WeaponType.TwoHanded");
	UE_DEFINE_GAMEPLAY_TAG(WeaponType_Polearm, "ArcRPG.WeaponType.Polearm");
	UE_DEFINE_GAMEPLAY_TAG(WeaponType_Unarmed, "ArcRPG.WeaponType.Unarmed");
	UE_DEFINE_GAMEPLAY_TAG(WeaponType_Bow, "ArcRPG.WeaponType.Bow");
	UE_DEFINE_GAMEPLAY_TAG(WeaponType_Crossbow, "ArcRPG.WeaponType.Crossbow");
	UE_DEFINE_GAMEPLAY_TAG(WeaponType_Throwing, "ArcRPG.WeaponType.Throwing");
	UE_DEFINE_GAMEPLAY_TAG(WeaponType_Staff, "ArcRPG.WeaponType.Staff");

	UE_DEFINE_GAMEPLAY_TAG(SpellElement, "ArcSpell.Element");
	UE_DEFINE_GAMEPLAY_TAG(SpellWoven, "ArcSpell.Woven");

	UE_DEFINE_GAMEPLAY_TAG(ItemWeapon, "Item.Weapon");

	UE_DEFINE_GAMEPLAY_TAG(Defense_Shield, "ArcRPG.Defense.Shield");
	UE_DEFINE_GAMEPLAY_TAG(Defense_Armed, "ArcRPG.Defense.Armed");
}
