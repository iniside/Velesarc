#pragma once

#include "Header.h"
#include "ID.h"
#include "PropertyIdSet.h"
#include "Types.h"

namespace Rml {

class StyleSheetSpecification;
class PropertyDefinition;
class PropertyDictionary;
class PropertyIdNameMap;
class ShorthandIdNameMap;
struct ShorthandDefinition;

enum class ShorthandType {
	// Normal; properties that fail to parse fall-through to the next until they parse correctly, and any
	// undeclared are not set.
	FallThrough,
	// A single failed parse will abort, and any undeclared are replicated from the last declared property.
	Replicate,
	// For 'padding', 'margin', etc; up to four properties are expected.
	Box,
	// Repeatedly resolves the full value string on each property, whether it is a normal property or another shorthand.
	RecursiveRepeat,
	// Comma-separated list of properties or shorthands, the number of declared values must match the specified.
	RecursiveCommaSeparated,
	// The 'flex' shorthand has some special behavior but otherwise acts like 'FallThrough'.
	Flex,
	// 'grid-row' and 'grid-column': start and end placements separated by '/'.
	GridLine,
	// 'grid-area': row-start, column-start, row-end and column-end placements separated by '/'.
	GridArea,
	// 'grid-template': rows and columns track lists separated by '/', optionally with area strings.
	GridTemplate,
	// 'grid': a 'grid-template' value, or an 'auto-flow' form also setting the implicit track properties.
	Grid,
	// Space-separated values in any order, each assigned to the first unset property that parses it; omitted properties are reset to their
	// default values.
	AnyOrder,
	// 'background' and 'background-image': comma-separated layers written to the decorator property given as the first item; the last layer may
	// contain a value for the color property given as the optional second item.
	BackgroundLayers,
	// 'mask': comma-separated layers whose image, compositing operator and origin and clip boxes are written to the four properties given as
	// items, in that order.
	MaskLayers,
	// A CSS '<position>' of one, two or four values, written as horizontal and vertical length-percentages to the two properties given as items.
	Position,
	// 'scrollbar-color': 'auto', or a thumb and a track color written to the two properties given as items.
	ScrollbarColor
};

/**
    A property specification stores a group of property definitions.
 */

class RMLUICORE_API PropertySpecification {
public:
	PropertySpecification(size_t reserve_num_properties, size_t reserve_num_shorthands);
	~PropertySpecification();

	/// Registers a property with a new definition.
	/// @param[in] property_name The name to register the new property under.
	/// @param[in] default_value The default value to be used for an element if it has no other definition provided.
	/// @param[in] inherited True if this property is inherited from parent to child, false otherwise.
	/// @param[in] forces_layout True if this property requires its parent to be reformatted if changed.
	/// @param[in] id If 'Invalid' then automatically assigns a new id, otherwise assigns the given id.
	/// @return The new property definition, ready to have parsers attached.
	PropertyDefinition& RegisterProperty(const String& property_name, const String& default_value, bool inherited, bool forces_layout,
		PropertyId id = PropertyId::Invalid);
	/// Returns a property definition.
	/// @param[in] id The id of the desired property.
	/// @return The appropriate property definition if it could be found, nullptr otherwise.
	const PropertyDefinition* GetProperty(PropertyId id) const;
	const PropertyDefinition* GetProperty(const String& property_name) const;
	const String& GetPropertyName(PropertyId id) const;

	/// Returns the id set of all registered property definitions.
	const PropertyIdSet& GetRegisteredProperties() const;
	/// Returns the id set of all registered inherited property definitions.
	const PropertyIdSet& GetRegisteredInheritedProperties() const;
	/// Returns the id set of all registered property definitions that may dirty the layout.
	const PropertyIdSet& GetRegisteredPropertiesForcingLayout() const;

	/// Registers a shorthand property definition.
	/// @param[in] shorthand_name The name to register the new shorthand property under.
	/// @param[in] property_names A comma-separated list of the properties this definition is shorthand for. The order
	/// in which they are specified here is the order in which the values will be processed.
	/// @param[in] type The type of shorthand to declare.
	/// @param[in] id If 'Invalid' then automatically assigns a new id, otherwise assigns the given id.
	/// @return An ID for the new shorthand, or 'Invalid' if the shorthand declaration is invalid.
	ShorthandId RegisterShorthand(const String& shorthand_name, const String& property_names, ShorthandType type,
		ShorthandId id = ShorthandId::Invalid);

	/// Registers an alternative name for a property or shorthand, declarations using it are forwarded to the target.
	/// @param[in] alias_name The alternative name, such as a vendor-prefixed property.
	/// @param[in] target_name The property or shorthand receiving the value. Empty to accept and ignore the declaration. A target that is
	/// not registered also accepts and ignores the declaration, until a property or shorthand with that name is registered.
	/// @param[in] keyword_map Keywords of the value replaced before forwarding, applied to each comma-separated item.
	void RegisterAlias(const String& alias_name, const String& target_name, const UnorderedMap<String, String>& keyword_map = {});
	/// Returns a shorthand definition.
	/// @param[in] id The id of the desired shorthand.
	/// @return The appropriate shorthand definition if it could be found, nullptr otherwise.
	const ShorthandDefinition* GetShorthand(ShorthandId id) const;
	const ShorthandDefinition* GetShorthand(const String& shorthand_name) const;
	const String& GetShorthandName(ShorthandId id) const;

	String GetRegistrySignature() const;

	/// Parse declaration by name, whether it's a property or shorthand.
	bool ParsePropertyDeclaration(PropertyDictionary& dictionary, const String& property_name, const String& property_value) const;
	/// Parse property declaration by ID.
	bool ParsePropertyDeclaration(PropertyDictionary& dictionary, PropertyId property_id, const String& property_value) const;
	/// Parses a shorthand declaration, setting any parsed and validated properties on the given dictionary.
	/// @return True if all properties were parsed successfully, false otherwise.
	bool ParseShorthandDeclaration(PropertyDictionary& dictionary, ShorthandId shorthand_id, const String& property_value) const;

	/// Sets all undefined properties in the dictionary to their defaults.
	/// @param dictionary[in-out] The dictionary to set the default values on.
	void SetPropertyDefaults(PropertyDictionary& dictionary) const;

	/// Returns the properties of dictionary converted to a string.
	String PropertiesToString(const PropertyDictionary& dictionary, bool include_name, char delimiter) const;

private:
	using Properties = Vector<UniquePtr<PropertyDefinition>>;
	using Shorthands = Vector<UniquePtr<ShorthandDefinition>>;

	Properties properties;
	Shorthands shorthands;

	UniquePtr<PropertyIdNameMap> property_map;
	UniquePtr<ShorthandIdNameMap> shorthand_map;

	PropertyIdSet property_ids;
	PropertyIdSet property_ids_inherited;
	PropertyIdSet property_ids_forcing_layout;

	struct PropertyAlias {
		String target_name;
		UnorderedMap<String, String> keyword_map;
	};
	UnorderedMap<String, PropertyAlias> aliases;

	enum class ParsePropertyResult { Success, ContainsVariable, Error };
	enum class SplitOption { None, PreserveQuotes, Whitespace, Comma };
	ParsePropertyResult ParsePropertyValues(StringList& values_list, const String& values, SplitOption split_option) const;

	bool ParseAnyOrderShorthand(PropertyDictionary& dictionary, const ShorthandDefinition& shorthand_definition,
		const StringList& property_values) const;
	bool ParseBackgroundLayersShorthand(PropertyDictionary& dictionary, const ShorthandDefinition& shorthand_definition,
		const String& property_value) const;
	bool ParseMaskLayersShorthand(PropertyDictionary& dictionary, const ShorthandDefinition& shorthand_definition, const String& property_value) const;
	bool ParsePositionShorthand(PropertyDictionary& dictionary, const ShorthandDefinition& shorthand_definition, const StringList& property_values) const;
	bool ParseScrollbarColorShorthand(PropertyDictionary& dictionary, const ShorthandDefinition& shorthand_definition,
		const StringList& property_values) const;

	friend class Rml::StyleSheetSpecification;
	friend class TestPropertySpecification;
};

} // namespace Rml
