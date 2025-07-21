# Velesarc

All code is provided as is. 
As of now, I do not provide any support for it.
Be advised that as of now code is tested and compiled agains ue5-main branch. 

# Features
* Items System - generic way of creaing data assets, with any set of properties to be configured inside editor. Items can be instanced and replicated.
* Inventory - No direct implementation, but it is subset functionality of Items System.
* Attachment System - a way to generically handle attachments when equiping items.
* Quick Bar - a generic way to create sets of editable and activatable slots, which can be edited by player, by inserting items into it.
* Replicated Commands - a way to encapsulate command to execute, and then send it from client to server or vice verse.
* Extend Game Framework - Based on lyra preconfigured way of createding custom game modes made of components, ability system and way to extend pawn without overriding base class.
* Targeting - integrated Targeting System from engine.
* Recoil & Spread - implemented mechanics for customizable recoil and spread, which can be controlled by player.

# Pull Request

List of pull requests which are required to compile plugins. List is ever changing as I refactor code to require less engine changes, or potentially PR are merged to engine.

* https://github.com/EpicGames/UnrealEngine/pull/12987 - Adds FInstanceStruct to FGameplayAbilitySpec
* https://github.com/EpicGames/UnrealEngine/pull/13603 - Exports some GameplayCamera structs and classes.
