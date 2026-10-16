#include "ElementStyle.h"
#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/Context.h"
#include "../../Include/RmlUi/Core/Core.h"
#include "../../Include/RmlUi/Core/DecorationTypes.h"
#include "../../Include/RmlUi/Core/ElementDocument.h"
#include "../../Include/RmlUi/Core/ElementUtilities.h"
#include "../../Include/RmlUi/Core/FontEngineInterface.h"
#include "../../Include/RmlUi/Core/Log.h"
#include "../../Include/RmlUi/Core/Math.h"
#include "../../Include/RmlUi/Core/Profiling.h"
#include "../../Include/RmlUi/Core/Property.h"
#include "../../Include/RmlUi/Core/PropertyDefinition.h"
#include "../../Include/RmlUi/Core/PropertyDictionary.h"
#include "../../Include/RmlUi/Core/PropertyIdSet.h"
#include "../../Include/RmlUi/Core/StyleSheet.h"
#include "../../Include/RmlUi/Core/StyleSheetSpecification.h"
#include "../../Include/RmlUi/Core/TransformPrimitive.h"
#include "ArcPerfInternal.h"
#include "CalcExpression.h"
#include "ComputeProperty.h"
#include "ControlledLifetimeResource.h"
#include "ElementAnimation.h"
#include "ElementDefinition.h"
#include "PropertiesIterator.h"
#include "PropertyParserContent.h"
#include "PropertyShorthandDefinition.h"
#include <algorithm>
#include <optional>

namespace Rml {

struct ElementStyleData {
	const PropertyDictionary empty_property_dictionary;
	Property returned_property_storage;
#ifdef RMLUI_ARC_PERF
	UnorderedMap<const Element*, Vector<const ElementDefinition*>> share_tuples;
#endif
};
static ControlledLifetimeResource<ElementStyleData> element_style_data;

#ifdef RMLUI_ARC_PERF
namespace ElementStyleSharing {
	bool DefinitionUsesVariables(const ElementDefinition* definition)
	{
		if (!definition)
			return false;
		const PropertyDictionary& properties = definition->GetProperties();
		if (!properties.GetVarShorthands().empty())
			return true;
		for (const PropertyMap::value_type& entry : properties.GetProperties())
		{
			if (entry.second.unit == Unit::VAR_EXPRESSION || entry.second.unit == Unit::SHORTHAND_PLACEHOLDER)
				return true;
		}
		return false;
	}
} // namespace ElementStyleSharing
#endif

static PseudoClassState operator|(PseudoClassState lhs, PseudoClassState rhs)
{
	return PseudoClassState(int(lhs) | int(rhs));
}
static PseudoClassState operator&(PseudoClassState lhs, PseudoClassState rhs)
{
	return PseudoClassState(int(lhs) & int(rhs));
}

ElementStyle::ElementStyle(Element* _element)
{
	element = _element;
}

static const Property* SelectCascadedProperty(const Property* inline_property, const Property* definition_property)
{
	if (inline_property && !(definition_property && definition_property->important && !inline_property->important))
		return inline_property;
	return definition_property;
}

const Property* ElementStyle::GetLocalProperty(PropertyId id, const PropertyDictionary& inline_properties, const ElementDefinition* definition)
{
	return SelectCascadedProperty(inline_properties.GetProperty(id), definition ? definition->GetProperty(id) : nullptr);
}

const Property* ElementStyle::GetLocalCustomProperty(const String& name, const PropertyDictionary& inline_properties,
	const ElementDefinition* definition)
{
	return SelectCascadedProperty(inline_properties.GetCustomProperty(name), definition ? definition->GetCustomProperty(name) : nullptr);
}

const Property* ElementStyle::GetLocalCustomProperty(const PropertySources& sources, const String& name)
{
	const Property* inline_property = sources.inline_properties.GetCustomProperty(name);
	if (!inline_property && sources.keyframe_element_inline_properties)
		inline_property = sources.keyframe_element_inline_properties->GetCustomProperty(name);
	return SelectCascadedProperty(inline_property, sources.definition ? sources.definition->GetCustomProperty(name) : nullptr);
}

const Property* ElementStyle::GetLocalShorthand(ShorthandId id, const PropertyDictionary& inline_properties, const ElementDefinition* definition)
{
	return SelectCascadedProperty(inline_properties.GetVarShorthand(id), definition ? definition->GetProperties().GetVarShorthand(id) : nullptr);
}

const Property* ElementStyle::ResolveCssWideKeyword(PropertyId id, const Property* property, Element* element)
{
	if (!property || (property->unit != Unit::INHERIT && property->unit != Unit::INITIAL))
		return property;

	const PropertyDefinition* property_definition = StyleSheetSpecification::GetProperty(id);
	if (!property_definition)
		return nullptr;

	Element* parent = element->GetParentNode();
	if (property->unit == Unit::INITIAL || !parent)
		return property_definition->GetDefaultValue();

	return parent->GetStyle()->GetProperty(id);
}

ElementStyle::PropertyElementPair ElementStyle::GetSpecifiedProperty(const PropertySources& sources, PropertyId id)
{
	const PropertyDefinition* property_definition = StyleSheetSpecification::GetProperty(id);

	const Property* local_property = GetLocalProperty(id, sources.inline_properties, sources.definition);
	if (local_property)
	{
		if (local_property->unit == Unit::INITIAL && property_definition)
			return {property_definition->GetDefaultValue(), nullptr};

		if (local_property->unit == Unit::INHERIT && property_definition)
		{
			if (Element* parent = sources.element->GetParentNode())
				return GetSpecifiedProperty(parent->GetStyle()->GetPropertySources(), id);
			return {property_definition->GetDefaultValue(), nullptr};
		}

		return {local_property, sources.element};
	}

	if (!property_definition)
		return {};

	// If we can inherit this property, return our parent's property.
	if (property_definition->IsInherited())
	{
		Element* parent = sources.element->GetParentNode();
		while (parent)
		{
			const Property* parent_property = parent->GetStyle()->GetLocalProperty(id);
			if (parent_property && parent_property->unit == Unit::INITIAL)
				return {property_definition->GetDefaultValue(), nullptr};
			if (parent_property && parent_property->unit != Unit::INHERIT)
				return {parent_property, parent};

			parent = parent->GetParentNode();
		}
	}

	// No property available! Return the default value.
	return {property_definition->GetDefaultValue(), nullptr};
}

ElementStyle::PropertyElementPair ElementStyle::GetSpecifiedCustomProperty(const PropertySources& sources, const String& name)
{
	if (const Property* local_property = GetLocalCustomProperty(sources, name))
		return {local_property, sources.element};

	Element* parent = sources.element->GetParentNode();
	while (parent)
	{
		ElementStyle* parent_style = parent->GetStyle();
		if (const Property* variable_property =
				ElementStyle::GetLocalCustomProperty(name, parent_style->inline_properties, parent_style->definition.get()))
		{
			return {variable_property, parent};
		}

		parent = parent->GetParentNode();
	}

	return {};
}

ElementStyle::PropertyElementPair ElementStyle::GetSpecifiedShorthand(const PropertySources& sources, ShorthandId id)
{
	const Property* local_shorthand = GetLocalShorthand(id, sources.inline_properties, sources.definition);
	if (local_shorthand)
		return {local_shorthand, sources.element};

	const ShorthandDefinition* shorthand_definition = StyleSheetSpecification::GetShorthand(id);
	if (!shorthand_definition)
		return {};

	if (shorthand_definition->inherited)
	{
		Element* parent = sources.element->GetParentNode();
		while (parent)
		{
			const Property* parent_shorthand = parent->GetStyle()->GetLocalShorthand(id);
			if (parent_shorthand)
				return {parent_shorthand, parent};

			parent = parent->GetParentNode();
		}
	}

	return {};
}

ElementStyle::SubstitutionResult ElementStyle::SubstituteVariableOnce(const PropertySources& sources, const String& value,
	SmallUnorderedSet<String>& variable_dependencies, SmallUnorderedSet<String>& cycle_chain, String& result)
{
	const size_t begin = value.find("var(");
	const size_t end_name = (begin != String::npos ? value.find_first_of(",)", begin) : String::npos);
	if (begin == String::npos || end_name == String::npos)
		return SubstitutionResult::NoVariablesFound;

	const bool has_fallback = (value[end_name] == ',');

	size_t end_parenthesis = end_name;
	if (has_fallback)
	{
		int depth = 1;
		for (end_parenthesis = end_name + 1; end_parenthesis < value.size(); end_parenthesis++)
		{
			const char c = value[end_parenthesis];
			if (c == '(')
				depth += 1;
			else if (c == ')')
				depth -= 1;
			if (depth == 0)
				break;
		}
	}
	if (end_parenthesis >= value.size())
		return SubstitutionResult::NoVariablesFound;

	String var_name = StringUtilities::StripWhitespace(StringView{value, begin + 4, end_name - begin - 4});
	const bool cycle_detected = (!cycle_chain.insert(var_name).second);
	if (cycle_detected)
	{
		Log::Message(Log::LT_ERROR, "Invalid substitution, cycle detected with variable: %s. In element: %s", var_name.c_str(),
			sources.element->GetAddress().c_str());
		return SubstitutionResult::Error;
	}

	auto SubstituteResult = [&](String substitute_value) {
		cycle_chain.erase(var_name);
		variable_dependencies.insert(std::move(var_name));
		result = value.substr(0, begin) + std::move(substitute_value) + value.substr(end_parenthesis + 1);
		return SubstitutionResult::Substituted;
	};

	const auto [custom_property, property_element] = GetSpecifiedCustomProperty(sources, var_name);
	const bool valid_property = (custom_property && property_element);
	if (!valid_property && has_fallback)
	{
		return SubstituteResult(StringUtilities::StripWhitespace(StringView{value, end_name + 1, end_parenthesis - end_name - 1}));
	}

	if (!valid_property)
	{
		Log::Message(Log::LT_ERROR, "Invalid substitution, variable '%s' not defined. In element: %s", var_name.c_str(),
			sources.element->GetAddress().c_str());
		return SubstitutionResult::Error;
	}

	String substitution_value;
	if (custom_property->unit == Unit::VAR_EXPRESSION)
	{
		// Reset cycle chain if (and only if) moving up to a new element.
		SmallUnorderedSet<String> new_cycle_chain;
		SmallUnorderedSet<String>& next_cycle_chain = (property_element == sources.element ? cycle_chain : new_cycle_chain);

		// We cannot always use the definition or inline properties from the style of the property element. Particularly
		// in TransitionPropertyChanges, these are not yet set for the current element. Instead, use the passed-in
		// values when the property element is the passed-in element. Otherwise, for ancestor elements, we assume they
		// are fully determined here, so fetch them from their elements.
		const PropertySources& next_sources = (property_element == sources.element ? sources : property_element->GetStyle()->GetPropertySources());

		if (!SubstituteVariables(next_sources, custom_property->Get<String>(), variable_dependencies, next_cycle_chain, substitution_value))
			return SubstitutionResult::Error;
	}
	else
	{
		substitution_value = custom_property->Get<String>();
	}

	return SubstituteResult(std::move(substitution_value));
}

bool ElementStyle::SubstituteVariables(const PropertySources& sources, String value, SmallUnorderedSet<String>& variable_dependencies,
	SmallUnorderedSet<String>& cycle_chain, String& result)
{
	bool looping = true;
	for (int i = 0; looping; i++)
	{
		SubstitutionResult substitution = SubstituteVariableOnce(sources, value, variable_dependencies, cycle_chain, result);
		switch (substitution)
		{
		case SubstitutionResult::Substituted:
		{
			value = std::move(result);
		}
		break;
		case SubstitutionResult::NoVariablesFound:
		{
			if (i == 0)
			{
				Log::Message(Log::LT_ERROR, "Invalid substitution, expected 'var()': %s. In element: %s", value.c_str(),
					sources.element->GetAddress().c_str());
				return false;
			}
			looping = false;
		}
		break;
		case SubstitutionResult::Error: return false;
		}
	}

	result = std::move(value);
	return true;
}

const Property* ElementStyle::ResolveVariables(const PropertySources& sources, const PropertyDictionary& substituted_shorthands, PropertyId id,
	const Property* property, SmallUnorderedSet<String>& variable_dependencies, Property& property_storage)
{
	if (property->unit == Unit::SHORTHAND_PLACEHOLDER)
	{
		const Property* property_from_shorthand = substituted_shorthands.GetProperty(id);
		if (!property_from_shorthand)
		{
			Log::Message(Log::LT_ERROR, "Property '%s' could not be resolved: Pending substitution from a shorthand.",
				StyleSheetSpecification::GetPropertyName(id).c_str());
			return nullptr;
		}

		return property_from_shorthand;
	}

	if (property->unit != Unit::VAR_EXPRESSION)
		return property;

	RMLUI_ARC_PERF_SCOPE(StyleVarResolve);
	RMLUI_ARC_PERF_COUNT(VarResolutions, 1);
	RMLUI_ARC_PERF_CODE(const ArcPerf::NamedTimer arc_perf_timer(ArcPerf::NamedList::VarResolutions, StyleSheetSpecification::GetPropertyName(id));)

	String substitution_result;
	SmallUnorderedSet<String> cycle_chain;
	if (!SubstituteVariables(sources, property->Get<String>(), variable_dependencies, cycle_chain, substitution_result))
		return nullptr;

	const PropertyDefinition* property_definition = StyleSheetSpecification::GetProperty(id);
	if (!property_definition)
	{
		Log::Message(Log::LT_ERROR, "Invalid substitution, property '%s' not defined.", StyleSheetSpecification::GetPropertyName(id).c_str());
		return nullptr;
	}

	if (!property_definition->ParseValue(property_storage, substitution_result))
	{
		Log::Message(Log::LT_ERROR, "Invalid substitution, property '%s' has invalid value: %s", StyleSheetSpecification::GetPropertyName(id).c_str(),
			substitution_result.c_str());
		return nullptr;
	}
	return &property_storage;
}

const Property* ElementStyle::ResolveVariablesWithShorthandExpansion(const PropertySources& sources, PropertyId id, const Property* property,
	std::optional<PropertyDictionary>& substituted_shorthands, SmallUnorderedSet<String>& variable_dependencies, Property& property_storage)
{
	if (!property)
		return nullptr;
	if (property->unit != Unit::SHORTHAND_PLACEHOLDER && property->unit != Unit::VAR_EXPRESSION)
		return property;
	if (property->unit == Unit::SHORTHAND_PLACEHOLDER && !substituted_shorthands.has_value())
	{
		// Lazy-compute shorthand expansion.
		substituted_shorthands.emplace();
		ExpandVarShorthands(*substituted_shorthands, sources, std::nullopt);
	}
	variable_dependencies.clear();
	return ResolveVariables(sources, (substituted_shorthands ? *substituted_shorthands : element_style_data->empty_property_dictionary), id, property,
		variable_dependencies, property_storage);
}

void ElementStyle::ExpandVarShorthands(PropertyDictionary& out_substituted_shorthands, const PropertySources& sources,
	std::optional<DirtyPropertiesRef> dirty_properties)
{
	RMLUI_ARC_PERF_SCOPE(StyleVarShorthands);
	RMLUI_ARC_PERF_COUNT(VarShorthandExpansions, 1);
	out_substituted_shorthands.Clear();
	SmallUnorderedSet<String> variable_dependencies;
	SmallUnorderedSet<String> cycle_chain;

	for (const PropertyDictionary* properties : {sources.definition ? &sources.definition->GetProperties() : nullptr, &sources.inline_properties})
	{
		if (!properties)
			continue;

		for (const auto& [shorthand_id, property] : properties->GetVarShorthands())
		{
			RMLUI_ASSERT(property.unit == Unit::VAR_EXPRESSION);
			if (GetLocalShorthand(shorthand_id, sources.inline_properties, sources.definition) != &property)
				continue;

			variable_dependencies.clear();
			cycle_chain.clear();

			String substitution_result;
			if (!SubstituteVariables(sources, property.Get<String>(), variable_dependencies, cycle_chain, substitution_result))
				continue;

			if (!StyleSheetSpecification::GetPropertySpecification().ParseShorthandDeclaration(out_substituted_shorthands, shorthand_id,
					substitution_result))
				continue;

			if (dirty_properties.has_value())
			{
				const bool dirty_variables_used = std::any_of(variable_dependencies.begin(), variable_dependencies.end(),
					[&](const String& variable_name) { return dirty_properties->variables.count(variable_name) != 0; });

				// Dirty all properties of the current shorthand either if: (1) it uses a dirtied variable, or (2) the shorthand itself is dirty.
				if (dirty_variables_used || dirty_properties->var_shorthands.count(shorthand_id) != 0)
				{
					const PropertyIdSet ids = StyleSheetSpecification::GetShorthandUnderlyingProperties(shorthand_id);
					for (const PropertyId id : ids)
						dirty_properties->properties.Insert(id);
				}
			}
		}
	}
}

void ElementStyle::TransitionPropertyChanges(const PropertySources& sources, PropertyIdSet& changed_properties,
	const ElementDefinition* new_definition)
{
	// Apply transition to relevant properties if a transition is defined on the element. Properties that are part of a
	// transition are removed from the properties list.

	RMLUI_ASSERT(sources.element);
	if (!sources.definition || !new_definition || changed_properties.Empty())
		return;

	// We get the local property directly here to intercept property changes even before the computed values are ready.
	const Property* transition_property = GetLocalProperty(PropertyId::Transition, sources.inline_properties, new_definition);
	if (!transition_property)
		return;
	if (transition_property->value.GetType() != Variant::TRANSITIONLIST)
		return;

	const TransitionList& transition_list = transition_property->value.GetReference<TransitionList>();
	if (transition_list.none)
		return;

	SmallUnorderedSet<String> variable_dependencies;

	Property property_storage_old;
	Property property_storage_new;
	std::optional<PropertyDictionary> substituted_shorthands_old;
	std::optional<PropertyDictionary> substituted_shorthands_new;

	PropertySources new_sources{sources.element, new_definition, element_style_data->empty_property_dictionary};

	auto add_transition = [&](const Transition& transition) {
		const Property* start_value = GetSpecifiedProperty(sources, transition.id).property;
		start_value = ResolveVariablesWithShorthandExpansion(sources, transition.id, start_value, substituted_shorthands_old, variable_dependencies,
			property_storage_old);

		const Property* target_value = GetSpecifiedProperty(new_sources, transition.id).property;
		target_value = ResolveVariablesWithShorthandExpansion(new_sources, transition.id, target_value, substituted_shorthands_new,
			variable_dependencies, property_storage_new);

		bool transition_added = false;
		if (start_value && target_value && (*start_value != *target_value) && Math::Max(transition.duration, 0.f) + transition.delay > 0.f)
			transition_added = sources.element->StartTransition(transition, *start_value, *target_value);

		return transition_added;
	};

	if (transition_list.all)
	{
		Transition transition = transition_list.transitions[0];
		for (auto it = changed_properties.begin(); it != changed_properties.end();)
		{
			transition.id = *it;
			if (add_transition(transition))
				it = changed_properties.Erase(it);
			else
				++it;
		}
	}
	else
	{
		for (const Transition& transition : transition_list.transitions)
		{
			if (changed_properties.Contains(transition.id))
			{
				if (add_transition(transition))
					changed_properties.Erase(transition.id);
			}
		}
	}
}

void ElementStyle::UpdateDefinition()
{
	RMLUI_ZoneScoped;
	RMLUI_ARC_PERF_SCOPE(DefinitionApply);

	SharedPtr<const ElementDefinition> new_definition;
	pseudo_element_rules = 0;

	if (const StyleSheet* style_sheet = element->GetStyleSheet())
	{
		new_definition = style_sheet->GetElementDefinition(element, &pseudo_element_rules);
	}

	// Switch the property definitions if the definition has changed.
	if (new_definition != definition)
	{
		RMLUI_ARC_PERF_COUNT(DefinitionsChanged, 1);
		PropertyIdSet changed_properties;
		UnorderedSet<String> changed_variables;
		UnorderedSet<ShorthandId> changed_shorthands;

		if (definition)
		{
			changed_properties = definition->GetPropertyIds();
			for (const auto& [name, _] : definition->GetProperties().GetCustomProperties())
				changed_variables.insert(name);
			for (const auto& [shorthand_id, _] : definition->GetProperties().GetVarShorthands())
				changed_shorthands.insert(shorthand_id);
		}

		if (new_definition)
		{
			changed_properties |= new_definition->GetPropertyIds();
			for (const auto& [name, _] : new_definition->GetProperties().GetCustomProperties())
				changed_variables.insert(name);
			for (const auto& [shorthand_id, _] : new_definition->GetProperties().GetVarShorthands())
				changed_shorthands.insert(shorthand_id);
		}

		if (definition && new_definition)
		{
			// Remove properties that compare equal from the changed list. Except if they can take variables: In this
			// case, the two properties may compare equal in value but represent different resolved properties depending
			// on their context. In that case we need to proceed to resolve them fully.
			const PropertyIdSet properties_in_both_definitions = (definition->GetPropertyIds() & new_definition->GetPropertyIds());

			for (PropertyId id : properties_in_both_definitions)
			{
				const Property* p0 = definition->GetProperty(id);
				const Property* p1 = new_definition->GetProperty(id);
				if (p0 && p1 && *p0 == *p1 && p0->important == p1->important && p0->unit != Unit::VAR_EXPRESSION &&
					p1->unit != Unit::SHORTHAND_PLACEHOLDER)
					changed_properties.Erase(id);
			}

			// Transition changed properties if transition property is set
			TransitionPropertyChanges(GetPropertySources(), changed_properties, new_definition.get());
		}

		definition = new_definition;

		dirty_properties |= changed_properties;
		dirty_variables.insert(std::make_move_iterator(changed_variables.begin()), std::make_move_iterator(changed_variables.end()));
		dirty_var_shorthands.insert(changed_shorthands.begin(), changed_shorthands.end());
	}
}

bool ElementStyle::SetPseudoClass(const String& pseudo_class, bool activate, bool override_class)
{
	bool changed = false;

	if (activate)
	{
		PseudoClassState& state = pseudo_classes[pseudo_class];
		changed = (state == PseudoClassState::Clear);
		state = (state | (override_class ? PseudoClassState::Override : PseudoClassState::Set));
	}
	else
	{
		auto it = pseudo_classes.find(pseudo_class);
		if (it != pseudo_classes.end())
		{
			PseudoClassState& state = it->second;
			state = (state & (override_class ? PseudoClassState::Set : PseudoClassState::Override));
			if (state == PseudoClassState::Clear)
			{
				pseudo_classes.erase(it);
				changed = true;
			}
		}
	}

	return changed;
}

bool ElementStyle::IsPseudoClassSet(const String& pseudo_class) const
{
	return (pseudo_classes.count(pseudo_class) == 1);
}

const PseudoClassMap& ElementStyle::GetActivePseudoClasses() const
{
	return pseudo_classes;
}

bool ElementStyle::SetClass(const String& class_name, bool activate)
{
	const auto class_location = std::find(classes.begin(), classes.end(), class_name);

	bool changed = false;
	if (activate)
	{
		if (class_location == classes.end())
		{
			classes.push_back(class_name);
			changed = true;
		}
	}
	else
	{
		if (class_location != classes.end())
		{
			classes.erase(class_location);
			changed = true;
		}
	}

	return changed;
}

bool ElementStyle::IsClassSet(const String& class_name) const
{
	return std::find(classes.begin(), classes.end(), class_name) != classes.end();
}

void ElementStyle::SetClassNames(const String& class_names)
{
	classes.clear();
	StringUtilities::ExpandString(classes, class_names, ' ');
}

String ElementStyle::GetClassNames() const
{
	String class_names;
	for (size_t i = 0; i < classes.size(); i++)
	{
		if (i != 0)
		{
			class_names += " ";
		}
		class_names += classes[i];
	}

	return class_names;
}

const StringList& ElementStyle::GetClassNameList() const
{
	return classes;
}

bool ElementStyle::SetProperty(PropertyId id, const Property& property)
{
	Property new_property = property;

	new_property.definition = StyleSheetSpecification::GetProperty(id);
	if (!new_property.definition)
		return false;

	inline_properties.SetProperty(id, new_property);
	DirtyProperty(id);

	return true;
}

void ElementStyle::SetCustomProperty(const String& name, const Property& property)
{
	inline_properties.SetCustomProperty(name, property);
	dirty_variables.insert(name);
	element->MarkNeedsUpdate();
}

void ElementStyle::SetVarShorthand(ShorthandId id, const Property& property)
{
	inline_properties.SetVarShorthand(id, property);
	dirty_var_shorthands.insert(id);
	element->MarkNeedsUpdate();
}

void ElementStyle::RemoveProperty(PropertyId id)
{
	int size_before = inline_properties.GetNumProperties();
	inline_properties.RemoveProperty(id);

	if (inline_properties.GetNumProperties() != size_before)
		DirtyProperty(id);
}

void ElementStyle::RemoveCustomProperty(const String& name)
{
	if (inline_properties.RemoveCustomProperty(name))
	{
		dirty_variables.insert(name);
		element->MarkNeedsUpdate();
	}
}

void ElementStyle::RemoveVarShorthand(ShorthandId id)
{
	if (inline_properties.RemoveVarShorthand(id))
	{
		dirty_var_shorthands.insert(id);
		element->MarkNeedsUpdate();
	}
}

const Property* ElementStyle::GetProperty(PropertyId id) const
{
	const PropertyElementPair result = GetSpecifiedProperty(GetPropertySources(), id);
	if (!result.element)
		return result.property;

	SmallUnorderedSet<String> variable_dependencies;
	const Property* property =
		result.element->GetStyle()->ResolveVariables(id, result.property, variable_dependencies, element_style_data->returned_property_storage);
	return ResolveCssWideKeyword(id, property, result.element);
}

const Property* ElementStyle::GetCustomProperty(const String& name) const
{
	const PropertyElementPair result = GetSpecifiedCustomProperty(GetPropertySources(), name);
	if (!result.element || !result.property || result.property->unit != Unit::VAR_EXPRESSION)
		return result.property;

	SmallUnorderedSet<String> variable_dependencies;
	SmallUnorderedSet<String> cycle_chain;
	String substitution_result;
	if (!SubstituteVariables(result.element->GetStyle()->GetPropertySources(), result.property->Get<String>(), variable_dependencies, cycle_chain,
			substitution_result))
		return nullptr;

	element_style_data->returned_property_storage = Property{substitution_result, Unit::STRING};
	return &element_style_data->returned_property_storage;
}

const Property* ElementStyle::GetLocalProperty(PropertyId id) const
{
	return GetLocalProperty(id, inline_properties, definition.get());
}

const Property* ElementStyle::GetLocalPropertyWithResolvedVariables(PropertyId id) const
{
	const Property* property = GetLocalProperty(id, inline_properties, definition.get());
	if (!property)
		return nullptr;

	SmallUnorderedSet<String> variable_dependencies;
	property = ResolveVariables(id, property, variable_dependencies, element_style_data->returned_property_storage);
	return ResolveCssWideKeyword(id, property, element);
}

const Property* ElementStyle::GetLocalCustomProperty(const String& name) const
{
	return GetLocalCustomProperty(name, inline_properties, definition.get());
}

const Property* ElementStyle::GetLocalShorthand(ShorthandId id) const
{
	return GetLocalShorthand(id, inline_properties, definition.get());
}

const PropertyDictionary& ElementStyle::GetLocalStyleProperties() const
{
	return inline_properties;
}

static Units GetPropertyUnits(const Property& property)
{
	if (property.unit == Unit::CALC && property.value.GetType() == Variant::CALCEXPRESSIONPTR)
	{
		if (const CalcExpressionPtr& calc = property.value.GetReference<CalcExpressionPtr>())
			return calc->GetUnits();
	}
	return property.unit;
}

static float ComputeLength(NumericValue value, Element* element)
{
	float font_size = 0.f;
	float doc_font_size = 0.f;
	float dp_ratio = 1.0f;
	Vector2f vp_dimensions(1.0f);

	if (Any(value.unit & Unit::DP_SCALABLE_LENGTH))
	{
		if (Context* context = element->GetContext())
			dp_ratio = context->GetDensityIndependentPixelRatio();
	}

	switch (value.unit)
	{
	case Unit::EM: font_size = element->GetComputedValues().font_size(); break;
	case Unit::REM:
		if (ElementDocument* document = element->GetOwnerDocument())
			doc_font_size = document->GetComputedValues().font_size();
		else
			doc_font_size = DefaultComputedValues().font_size();
		break;
	case Unit::VW:
	case Unit::VH:
		if (Context* context = element->GetContext())
			vp_dimensions = Vector2f(context->GetDimensions());
		break;
	default: break;
	}

	const float result = ComputeLength(value, font_size, doc_font_size, dp_ratio, vp_dimensions);
	return result;
}

float ElementStyle::ResolveNumericValue(NumericValue value, float base_value) const
{
	if (value.unit == Unit::DEVICE_PX)
		return value.number;
	else if (Any(value.unit & Unit::LENGTH))
		return ComputeLength(value, element);

	switch (value.unit)
	{
	case Unit::NUMBER: return value.number * base_value;
	case Unit::PERCENT: return value.number * base_value * 0.01f;
	case Unit::X: return value.number;
	case Unit::DEG:
	case Unit::RAD: return ComputeAngle(value);
	default: break;
	}

	RMLUI_ERROR;
	return 0.f;
}

float ElementStyle::ResolveRelativeLength(NumericValue value, RelativeTarget relative_target) const
{
	// There is an exception on font-size properties, as 'em' units here refer to parent font size instead
	if (Any(value.unit & Unit::LENGTH) && !(value.unit == Unit::EM && relative_target == RelativeTarget::ParentFontSize))
	{
		const float result = ComputeLength(value, element);
		return result;
	}

	float base_value = 0.0f;

	switch (relative_target)
	{
	case RelativeTarget::None: base_value = 1.0f; break;
	case RelativeTarget::ContainingBlockWidth: base_value = element->GetContainingBlock().x; break;
	case RelativeTarget::ContainingBlockHeight: base_value = element->GetContainingBlock().y; break;
	case RelativeTarget::FontSize: base_value = element->GetComputedValues().font_size(); break;
	case RelativeTarget::ParentFontSize:
	{
		auto p = element->GetParentNode();
		base_value = (p ? p->GetComputedValues().font_size() : DefaultComputedValues().font_size());
	}
	break;
	case RelativeTarget::LineHeight: base_value = element->GetLineHeight(); break;
	}

	float scale_value = 0.0f;

	switch (value.unit)
	{
	case Unit::EM:
	case Unit::NUMBER: scale_value = value.number; break;
	case Unit::PERCENT: scale_value = value.number * 0.01f; break;
	default: break;
	}

	return base_value * scale_value;
}

void ElementStyle::DirtyInheritedProperties()
{
	dirty_properties |= StyleSheetSpecification::GetRegisteredInheritedProperties();
	element->MarkNeedsUpdate();
}

void ElementStyle::DirtyPropertiesWithUnits(Units units)
{
	// Dirty all the properties of this element that use the unit(s).
	for (auto it = Iterate(); !it.AtEnd(); ++it)
	{
		auto name_property_pair = *it;
		PropertyId id = name_property_pair.first;
		const Property& property = name_property_pair.second;
		if (Any(GetPropertyUnits(property) & units))
			DirtyProperty(id);
	}
}

void ElementStyle::DirtyPropertiesWithUnitsRecursive(Units units)
{
	DirtyPropertiesWithUnits(units);

	// Now dirty all of our descendant's properties that use the unit(s).
	int num_children = element->GetNumChildren(true);
	for (int i = 0; i < num_children; ++i)
		element->GetChild(i)->GetStyle()->DirtyPropertiesWithUnitsRecursive(units);
}

bool ElementStyle::AnyPropertiesDirty() const
{
	return !dirty_properties.Empty() || !dirty_variables.empty() || !dirty_var_shorthands.empty();
}

const Property* ElementStyle::ResolveKeyFrameProperty(PropertyId id, const Property* property, const PropertyDictionary& block_properties,
	std::optional<PropertyDictionary>& block_substituted_shorthands, SmallUnorderedSet<String>& variable_dependencies, Property& property_storage)
{
	variable_dependencies.clear();
	PropertySources sources{element, definition.get(), block_properties, &inline_properties};
	return ResolveVariablesWithShorthandExpansion(sources, id, property, block_substituted_shorthands, variable_dependencies, property_storage);
}

PlainPropertiesIterator ElementStyle::Iterate() const
{
	return PlainPropertiesIterator{
		inline_properties.GetProperties(),
		definition ? &definition->GetProperties().GetProperties() : nullptr,
	};
}

PropertyIdSet ElementStyle::GetInheritKeywordProperties() const
{
	PropertyIdSet result;
	if (definition)
		result = definition->GetInheritKeywordPropertyIds();
	for (const auto& [id, property] : inline_properties.GetProperties())
	{
		if (property.unit == Unit::INHERIT)
			result.Insert(id);
	}
	return result;
}

UniquePtr<AllPropertiesIterator> ElementStyle::IterateAll(Element* filter_inherited_by) const
{
	return MakeUnique<AllPropertiesIterator>(inline_properties, definition ? &definition->GetProperties() : nullptr,
		filter_inherited_by ? filter_inherited_by->GetStyle() : nullptr);
}

void ElementStyle::Initialize()
{
	element_style_data.Initialize();
}
void ElementStyle::Shutdown()
{
	element_style_data.Shutdown();
}

void ElementStyle::ClearSharedStyles()
{
#ifdef RMLUI_ARC_PERF
	element_style_data->share_tuples.clear();
#endif
}

void ElementStyle::DirtyProperty(PropertyId id)
{
	dirty_properties.Insert(id);
	element->MarkNeedsUpdate();
}

PropertyIdSet ElementStyle::ComputeValues(Style::ComputedValues& values, const Style::ComputedValues* parent_values,
	const Style::ComputedValues* document_values, bool values_are_default_initialized, float dp_ratio, Vector2f vp_dimensions)
{
	if (!AnyPropertiesDirty())
		return PropertyIdSet();

	RMLUI_ZoneScopedC(0xFF7F50);
	RMLUI_ARC_PERF_SCOPE(StyleCompute);
	RMLUI_ARC_PERF_CODE({
		RMLUI_ARC_PERF_COUNT(StyleComputes, 1);
		if (ArcPerf::Detailed())
		{
			for (const String& variable_name : dirty_variables)
				ArcPerf::RecordNamed(ArcPerf::NamedList::DirtyCssVariables, variable_name, 0.0, 1);
		}
		if (values_are_default_initialized && ArcPerf::Enabled())
		{
			RMLUI_ARC_PERF_COUNT(StyleFirstComputes, 1);
			const Element* parent = element->GetParentNode();
			if (parent && inline_properties.Empty() && element->animations.empty() && element->GetPseudoElementType() == PseudoElement::None &&
				!ElementStyleSharing::DefinitionUsesVariables(definition.get()))
			{
				RMLUI_ARC_PERF_COUNT(StyleShareCandidates, 1);
				Vector<const ElementDefinition*>& definitions = element_style_data->share_tuples[parent];
				if (std::find(definitions.begin(), definitions.end(), definition.get()) == definitions.end())
				{
					definitions.push_back(definition.get());
					RMLUI_ARC_PERF_COUNT(StyleShareTuples, 1);
				}
				else
				{
					RMLUI_ARC_PERF_COUNT(StyleShareHits, 1);
				}
			}
		}
	})

	// We're a bit conservative here and recompute shorthand substitutions also if any variables have been dirtied at
	// all. @performance We could be more strict and only recompute shorthands if any variables they *use* have been dirtied.
	if (!dirty_variables.empty() || !dirty_var_shorthands.empty())
	{
		if ((definition && definition->HasVarShorthands()) || !inline_properties.GetVarShorthands().empty())
			ExpandVarShorthands(expanded_shorthands, GetPropertySources(), GetDirtyPropertiesRef());
		else if (!expanded_shorthands.Empty())
			expanded_shorthands.Clear();
	}

	// Generally, this is how it works:
	//   1. Assign default values (clears any removed properties)
	//   2. Inherit inheritable values from parent
	//   3. Assign any local properties (from inline style or stylesheet)
	//   4. Dirty properties in children that are inherited

	Property property_storage;
	const float font_size_before = values.font_size();
	const Style::LineHeight line_height_before = values.line_height();

	// The next flag is just a small optimization, if the element was just created we don't need to copy all the default values.
	if (!values_are_default_initialized)
	{
		// This needs to be done in case some properties were removed and thus not in our local style anymore.
		// If we skipped this, the old dirty value would be unmodified, instead, now it is set to its default value.
		// Strictly speaking, we only really need to do this for the dirty, non-inherited values. However, in most
		// cases it seems simply assigning all non-inherited values is faster than iterating the dirty properties.
		values.CopyNonInherited(DefaultComputedValues());
	}

	if (parent_values)
		values.CopyInherited(*parent_values);
	else if (!values_are_default_initialized)
		values.CopyInherited(DefaultComputedValues());

	SmallUnorderedSet<String> variable_dependencies;
	bool dirty_em_properties = false;

	// Always do font-size first if dirty, because of em-relative values
	if (dirty_properties.Contains(PropertyId::FontSize))
	{
		if (const Property* property = GetLocalProperty(PropertyId::FontSize))
		{
			variable_dependencies.clear();
			property = ResolveVariables(PropertyId::FontSize, property, variable_dependencies, property_storage);
			if (property && property->unit == Unit::INHERIT)
				values.font_size(parent_values ? parent_values->font_size() : DefaultComputedValues().font_size());
			else if ((property = ResolveCssWideKeyword(PropertyId::FontSize, property, element)) != nullptr)
				values.font_size(ComputeFontsize(property->GetNumericValue(), values, parent_values, document_values, dp_ratio, vp_dimensions));
		}
		else if (parent_values)
			values.font_size(parent_values->font_size());

		if (font_size_before != values.font_size())
		{
			dirty_em_properties = true;
			dirty_properties.Insert(PropertyId::LineHeight);
		}
	}
	else
	{
		values.font_size(font_size_before);
	}

	const float font_size = values.font_size();
	const float document_font_size = (document_values ? document_values->font_size() : DefaultComputedValues().font_size());

	// Since vertical-align depends on line-height we compute this before iteration
	if (dirty_properties.Contains(PropertyId::LineHeight))
	{
		const Property* property = GetLocalProperty(PropertyId::LineHeight);
		bool inherit_line_height = !property;
		if (property)
		{
			variable_dependencies.clear();
			property = ResolveVariables(PropertyId::LineHeight, property, variable_dependencies, property_storage);
			if (property && property->unit == Unit::INHERIT)
				inherit_line_height = true;
			else if ((property = ResolveCssWideKeyword(PropertyId::LineHeight, property, element)) != nullptr)
				values.line_height(ComputeLineHeight(property, font_size, document_font_size, dp_ratio, vp_dimensions));
		}

		if (inherit_line_height && parent_values)
		{
			// Line height has a special inheritance case for numbers/percent: they inherit them directly instead of computed length, but for lengths,
			// they inherit the length. See CSS specs for details. Percent is already converted to number.
			const Style::LineHeight parent_line_height = parent_values->line_height();
			if (parent_line_height.inherit_type == Style::LineHeight::Number)
				values.line_height(Style::LineHeight(font_size * parent_line_height.inherit_value, Style::LineHeight::Number,
					parent_line_height.inherit_value));
			else if (parent_line_height.inherit_type == Style::LineHeight::Normal)
				values.line_height(Style::LineHeight(font_size * Style::LineHeight::NormalFallbackFactor, Style::LineHeight::Normal, 0.f));
			else
				values.line_height(parent_line_height);
		}

		if (line_height_before.value != values.line_height().value || line_height_before.inherit_value != values.line_height().inherit_value)
			dirty_properties.Insert(PropertyId::VerticalAlign);
	}
	else
	{
		values.line_height(line_height_before);
	}

	bool dirty_font_face_handle = false;
	bool border_current_color[4] = {true, true, true, true};
	bool background_current_color = false;
	bool image_current_color = false;

	for (auto it = Iterate(); !it.AtEnd(); ++it)
	{
		variable_dependencies.clear();
		auto id_property_pair = *it;
		const PropertyId id = id_property_pair.first;
		RMLUI_ARC_PERF_COUNT(PropertiesComputed, 1);
		const Property* property = ResolveVariables(id, &id_property_pair.second, variable_dependencies, property_storage);
		if (!property)
			continue;

		for (const String& variable_name : variable_dependencies)
		{
			if (dirty_variables.count(variable_name) != 0)
				dirty_properties.Insert(id);
		}

		if (dirty_em_properties && Any(GetPropertyUnits(*property) & Unit::EM))
			dirty_properties.Insert(id);

		float property_font_size = font_size;
		if (property->unit == Unit::INHERIT || property->unit == Unit::INITIAL)
		{
			if (property->unit == Unit::INHERIT && parent_values)
				property_font_size = parent_values->font_size();
			property = ResolveCssWideKeyword(id, property, element);
			if (!property)
				continue;
		}

		if (id >= PropertyId::BorderTopColor && id <= PropertyId::BorderLeftColor)
			border_current_color[(int)id - (int)PropertyId::BorderTopColor] = (property->unit != Unit::COLOUR);
		else if (id == PropertyId::BackgroundColor)
			background_current_color = (property->unit != Unit::COLOUR);
		else if (id == PropertyId::ImageColor)
			image_current_color = (property->unit != Unit::COLOUR);
		else if (id == PropertyId::Color && property->unit != Unit::COLOUR)
			continue;

		ComputeValue(values, dp_ratio, vp_dimensions, property_font_size, document_font_size, dirty_font_face_handle, id, property);
	}

	// CSS Color 4: currentcolor resolves to the element's own 'color', so these sides follow every change of it.
	for (int side = 0; side < 4; side++)
	{
		if (!border_current_color[side])
			continue;
		const PropertyId border_color_id = PropertyId((int)PropertyId::BorderTopColor + side);
		switch (border_color_id)
		{
		case PropertyId::BorderTopColor: values.border_top_color(values.color()); break;
		case PropertyId::BorderRightColor: values.border_right_color(values.color()); break;
		case PropertyId::BorderBottomColor: values.border_bottom_color(values.color()); break;
		case PropertyId::BorderLeftColor: values.border_left_color(values.color()); break;
		default: break;
		}
		if (dirty_properties.Contains(PropertyId::Color))
			dirty_properties.Insert(border_color_id);
	}

	if (background_current_color)
	{
		values.background_color(values.color());
		if (dirty_properties.Contains(PropertyId::Color))
			dirty_properties.Insert(PropertyId::BackgroundColor);
	}
	if (image_current_color)
	{
		values.image_color(values.color());
		if (dirty_properties.Contains(PropertyId::Color))
			dirty_properties.Insert(PropertyId::ImageColor);
	}
	if (dirty_properties.Contains(PropertyId::Color))
	{
		if (const Property* box_shadow = GetLocalPropertyWithResolvedVariables(PropertyId::BoxShadow);
			box_shadow && box_shadow->value.GetType() == Variant::BOXSHADOWLIST)
		{
			const BoxShadowList& shadow_list = box_shadow->value.GetReference<BoxShadowList>();
			if (std::any_of(shadow_list.begin(), shadow_list.end(), [](const BoxShadow& shadow) { return shadow.current_color; }))
				dirty_properties.Insert(PropertyId::BoxShadow);
		}
		for (const PropertyId id : {PropertyId::ScrollbarThumbColor, PropertyId::ScrollbarTrackColor})
		{
			const Property* scrollbar_color = GetProperty(id);
			if (scrollbar_color && IsCurrentColorKeyword(id, *scrollbar_color))
				dirty_properties.Insert(id);
		}
	}

	if (values.border_top_style() == Style::BorderStyle::None)
		values.border_top_width(0);
	if (values.border_right_style() == Style::BorderStyle::None)
		values.border_right_width(0);
	if (values.border_bottom_style() == Style::BorderStyle::None)
		values.border_bottom_width(0);
	if (values.border_left_style() == Style::BorderStyle::None)
		values.border_left_width(0);

	if (element->GetPseudoElementType() != PseudoElement::None)
	{
		if (values_are_default_initialized || dirty_properties.Contains(PropertyId::Content))
			dirty_properties.Insert(PropertyId::Display);

		const Property* content = GetLocalPropertyWithResolvedVariables(PropertyId::Content);
		if (!content || content->unit != Unit::STRING || !PropertyParserContent::GeneratesBox(content->Get<String>()))
			values.display(Style::Display::None);
	}

	// The font-face handle is nulled when local font properties are set. In that case we need to retrieve a new handle.
	if (dirty_font_face_handle)
	{
		RMLUI_ZoneScopedN("FontFaceHandle");
		RMLUI_ARC_PERF_SCOPE(StyleFontFace);
		RMLUI_ARC_PERF_COUNT(FontFaceLookups, 1);
		values.font_face_handle(
			GetFontEngineInterface()->GetFontFaceHandle(values.font_family(), values.font_style(), values.font_weight(), (int)values.font_size()));
	}

	if (values.line_height().inherit_type == Style::LineHeight::Normal)
	{
		const FontFaceHandle font_face_handle = values.font_face_handle();
		const float normal_line_height = (font_face_handle ? GetFontEngineInterface()->GetFontMetrics(font_face_handle).line_spacing
														   : values.font_size() * Style::LineHeight::NormalFallbackFactor);
		if (normal_line_height != values.line_height().value)
		{
			values.line_height(Style::LineHeight(normal_line_height, Style::LineHeight::Normal, 0.f));
			dirty_properties.Insert(PropertyId::LineHeight);
			dirty_properties.Insert(PropertyId::VerticalAlign);
			if (const Property* vertical_align = GetLocalPropertyWithResolvedVariables(PropertyId::VerticalAlign))
				values.vertical_align(ComputeVerticalAlign(vertical_align, normal_line_height, font_size, document_font_size, dp_ratio, vp_dimensions));
		}
	}

	// Next, pass inheritable dirty properties onto our children
	PropertyIdSet dirty_inherited_properties = (dirty_properties & StyleSheetSpecification::GetRegisteredInheritedProperties());

	// Special case for text-overflow: It's not really inherited, but the value is used by the element's children. Insert
	// it here so they are notified of a change.
	if (dirty_properties.Contains(PropertyId::TextOverflow))
		dirty_inherited_properties.Insert(PropertyId::TextOverflow);

	// Propagate dirty inherited properties to children. Var shorthands are local to the element and never inherited.
	if (!dirty_inherited_properties.Empty() || !dirty_variables.empty())
	{
		for (int i = 0; i < element->GetNumChildren(true); i++)
		{
			auto child = element->GetChild(i);
			child->GetStyle()->dirty_properties |= dirty_inherited_properties;
			child->GetStyle()->dirty_variables.insert(dirty_variables.begin(), dirty_variables.end());
			child->MarkNeedsUpdate();
		}
	}

	if (!dirty_properties.Empty())
	{
		for (int i = 0; i < element->GetNumChildren(true); i++)
		{
			Element* child = element->GetChild(i);
			ElementStyle* child_style = child->GetStyle();
			const PropertyIdSet inherit_keyword_properties = (dirty_properties & child_style->GetInheritKeywordProperties());
			if (!inherit_keyword_properties.Empty())
			{
				child_style->dirty_properties |= inherit_keyword_properties;
				child->MarkNeedsUpdate();
			}
		}
	}

	PropertyIdSet result(std::move(dirty_properties));
	dirty_properties.Clear();
	dirty_variables.clear();
	dirty_var_shorthands.clear();
	return result;
}

ElementStyle::PropertySources ElementStyle::GetPropertySources() const
{
	return {element, definition.get(), inline_properties};
}

ElementStyle::DirtyPropertiesRef ElementStyle::GetDirtyPropertiesRef()
{
	return {dirty_properties, dirty_variables, dirty_var_shorthands};
}

const Property* ElementStyle::ResolveVariables(PropertyId id, const Property* property, SmallUnorderedSet<String>& variable_dependencies,
	Property& property_storage) const
{
	return ResolveVariables(GetPropertySources(), expanded_shorthands, id, property, variable_dependencies, property_storage);
}

void ElementStyle::ComputeValue(Style::ComputedValues& values, float dp_ratio, Vector2f vp_dimensions, const float font_size,
	const float document_font_size, bool& dirty_font_face_handle, const PropertyId id, const Property* p)
{
	using namespace Style;

	// clang-format off
	switch (id)
	{
	case PropertyId::MarginTop:
		values.margin_top(ComputeLengthPercentageAuto(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::MarginRight:
		values.margin_right(ComputeLengthPercentageAuto(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::MarginBottom:
		values.margin_bottom(ComputeLengthPercentageAuto(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::MarginLeft:
		values.margin_left(ComputeLengthPercentageAuto(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;

	case PropertyId::PaddingTop:
		values.padding_top(ComputeLengthPercentage(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::PaddingRight:
		values.padding_right(ComputeLengthPercentage(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::PaddingBottom:
		values.padding_bottom(ComputeLengthPercentage(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::PaddingLeft:
		values.padding_left(ComputeLengthPercentage(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;

	case PropertyId::BorderTopWidth:
		values.border_top_width(ComputeBorderWidth(ComputeLength(p->GetNumericValue(), font_size, document_font_size, dp_ratio, vp_dimensions)));
		break;
	case PropertyId::BorderRightWidth:
		values.border_right_width(
			ComputeBorderWidth(ComputeLength(p->GetNumericValue(), font_size, document_font_size, dp_ratio, vp_dimensions)));
		break;
	case PropertyId::BorderBottomWidth:
		values.border_bottom_width(
			ComputeBorderWidth(ComputeLength(p->GetNumericValue(), font_size, document_font_size, dp_ratio, vp_dimensions)));
		break;
	case PropertyId::BorderLeftWidth:
		values.border_left_width(ComputeBorderWidth(ComputeLength(p->GetNumericValue(), font_size, document_font_size, dp_ratio, vp_dimensions)));
		break;

	case PropertyId::BorderTopColor:
		if (p->unit == Unit::COLOUR)
			values.border_top_color(p->Get<Colourb>());
		break;
	case PropertyId::BorderRightColor:
		if (p->unit == Unit::COLOUR)
			values.border_right_color(p->Get<Colourb>());
		break;
	case PropertyId::BorderBottomColor:
		if (p->unit == Unit::COLOUR)
			values.border_bottom_color(p->Get<Colourb>());
		break;
	case PropertyId::BorderLeftColor:
		if (p->unit == Unit::COLOUR)
			values.border_left_color(p->Get<Colourb>());
		break;

	case PropertyId::BorderTopStyle:
		values.border_top_style((BorderStyle)p->Get<int>());
		break;
	case PropertyId::BorderRightStyle:
		values.border_right_style((BorderStyle)p->Get<int>());
		break;
	case PropertyId::BorderBottomStyle:
		values.border_bottom_style((BorderStyle)p->Get<int>());
		break;
	case PropertyId::BorderLeftStyle:
		values.border_left_style((BorderStyle)p->Get<int>());
		break;

	case PropertyId::BorderTopLeftRadius:
		values.border_top_left_radius(ComputeLengthPercentage(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::BorderTopRightRadius:
		values.border_top_right_radius(ComputeLengthPercentage(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::BorderBottomRightRadius:
		values.border_bottom_right_radius(ComputeLengthPercentage(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::BorderBottomLeftRadius:
		values.border_bottom_left_radius(ComputeLengthPercentage(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;

	case PropertyId::Display:
		values.display((Display)p->Get<int>());
		break;
	case PropertyId::Position:
		values.position((Position)p->Get<int>());
		break;

	case PropertyId::Top:
		values.top(ComputeLengthPercentageAuto(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::Right:
		values.right(ComputeLengthPercentageAuto(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::Bottom:
		values.bottom(ComputeLengthPercentageAuto(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::Left:
		values.left(ComputeLengthPercentageAuto(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;

	case PropertyId::Float:
		values.float_((Float)p->Get<int>());
		break;
	case PropertyId::Clear:
		values.clear((Clear)p->Get<int>());
		break;
	case PropertyId::BoxSizing:
		values.box_sizing((BoxSizing)p->Get<int>());
		break;

	case PropertyId::ZIndex:
		values.z_index((p->unit == Unit::KEYWORD ? ZIndex(ZIndex::Auto) : ZIndex(ZIndex::Number, p->Get<float>())));
		break;

	case PropertyId::Width:
		values.width(ComputeLengthPercentageAuto(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::MinWidth:
		values.min_width(ComputeLengthPercentage(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::MaxWidth:
		values.max_width(ComputeMaxSize(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;

	case PropertyId::Height:
		values.height(ComputeLengthPercentageAuto(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::MinHeight:
		values.min_height(ComputeLengthPercentage(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::MaxHeight:
		values.max_height(ComputeMaxSize(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;

	case PropertyId::LineHeight:
		// (Line-height computed above)
		break;
	case PropertyId::VerticalAlign:
		values.vertical_align(ComputeVerticalAlign(p, values.line_height().value, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;

	case PropertyId::OverflowX:
		values.overflow_x((Overflow)p->Get< int >());
		break;
	case PropertyId::OverflowY:
		values.overflow_y((Overflow)p->Get< int >());
		break;
	case PropertyId::Clip:
		values.clip(ComputeClip(p));
		break;
	case PropertyId::Visibility:
		values.visibility((Visibility)p->Get< int >());
		break;
	case PropertyId::TextOverflow:
		values.text_overflow(p->unit == Unit::KEYWORD ? p->Get<TextOverflow>() : TextOverflow::String);
		break;

	case PropertyId::BackgroundColor:
		if (p->unit == Unit::COLOUR)
			values.background_color(p->Get<Colourb>());
		break;
	case PropertyId::Color:
		if (p->unit == Unit::COLOUR)
			values.color(p->Get<Colourb>());
		break;
	case PropertyId::ImageColor:
		if (p->unit == Unit::COLOUR)
			values.image_color(p->Get<Colourb>());
		break;
	case PropertyId::Opacity:
		values.opacity(p->Get<float>());
		break;

	case PropertyId::FontFamily:
		// Fetched from element's properties.
		dirty_font_face_handle = true;
		break;
	case PropertyId::FontStyle:
		values.font_style((FontStyle)p->Get< int >());
		dirty_font_face_handle = true;
		break;
	case PropertyId::FontWeight:
		values.font_weight((FontWeight)p->Get< int >());
		dirty_font_face_handle = true;
		break;
	case PropertyId::FontSize:
		// (font-size computed above)
		dirty_font_face_handle = true;
		break;
	case PropertyId::FontKerning:
		values.font_kerning((FontKerning)p->Get<int>());
		dirty_font_face_handle = true;
		break;
	case PropertyId::LetterSpacing:
		values.has_letter_spacing(p->unit != Unit::KEYWORD);
		dirty_font_face_handle = true;
		break;

	case PropertyId::TextAlign:
		values.text_align((TextAlign)p->Get<int>());
		break;
	case PropertyId::TextDecoration:
		values.text_decoration((TextDecoration)p->Get<int>());
		break;
	case PropertyId::TextTransform:
		values.text_transform((TextTransform)p->Get<int>());
		break;
	case PropertyId::WhiteSpace:
		values.white_space((WhiteSpace)p->Get<int>());
		break;
	case PropertyId::WordBreak:
		values.word_break((WordBreak)p->Get<int>());
		break;

	case PropertyId::RowGap:
		values.row_gap(ComputeLengthPercentage(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::ColumnGap:
		values.column_gap(ComputeLengthPercentage(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;

	case PropertyId::Drag:
		values.drag((Drag)p->Get< int >());
		break;
	case PropertyId::TabIndex:
		values.tab_index((TabIndex)p->Get< int >());
		break;
	case PropertyId::Focus:
		values.focus((Focus)p->Get<int>());
		break;
	case PropertyId::ScrollbarMargin:
		values.scrollbar_margin(ComputeLength(p->GetNumericValue(), font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::OverscrollBehavior:
		values.overscroll_behavior((OverscrollBehavior)p->Get<int>());
		break;
	case PropertyId::PointerEvents:
		values.pointer_events((PointerEvents)p->Get<int>());
		break;

	case PropertyId::Perspective:
		values.perspective(p->unit == Unit::KEYWORD ? 0.f : ComputeLength(p->GetNumericValue(), font_size, document_font_size, dp_ratio, vp_dimensions));
		values.has_local_perspective(values.perspective() > 0.f);
		break;
	case PropertyId::PerspectiveOriginX:
		values.perspective_origin_x(ComputeOrigin(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::PerspectiveOriginY:
		values.perspective_origin_y(ComputeOrigin(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;

	case PropertyId::Transform:
		values.has_local_transform(p->Get<TransformPtr>() != nullptr);
		break;
	case PropertyId::TransformOriginX:
		values.transform_origin_x(ComputeOrigin(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::TransformOriginY:
		values.transform_origin_y(ComputeOrigin(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;
	case PropertyId::TransformOriginZ:
		values.transform_origin_z(ComputeLength(p->GetNumericValue(), font_size, document_font_size, dp_ratio, vp_dimensions));
		break;

	case PropertyId::Decorator:
		values.has_decorator(p->unit == Unit::DECORATOR && p->value.GetType() == Variant::DECORATORSPTR && p->value.GetReference<DecoratorsPtr>());
		break;
	case PropertyId::MaskImage:
		values.has_mask_image(p->unit == Unit::DECORATOR && p->value.GetType() == Variant::DECORATORSPTR && p->value.GetReference<DecoratorsPtr>());
		break;
	case PropertyId::FontEffect:
		values.has_font_effect(p->unit == Unit::FONTEFFECT && p->value.GetType() == Variant::FONTEFFECTSPTR && p->value.GetReference<FontEffectsPtr>());
		break;
	case PropertyId::Filter:
		values.has_filter(p->unit == Unit::FILTER && p->value.GetType() == Variant::FILTERSPTR && p->value.GetReference<FiltersPtr>());
		break;
	case PropertyId::BackdropFilter:
		values.has_backdrop_filter(p->unit == Unit::FILTER && p->value.GetType() == Variant::FILTERSPTR && p->value.GetReference<FiltersPtr>());
		break;
	case PropertyId::BoxShadow:
		values.has_box_shadow(p->unit == Unit::BOXSHADOWLIST && p->value.GetType() == Variant::BOXSHADOWLIST && !p->value.GetReference<BoxShadowList>().empty());
		break;

	case PropertyId::FlexBasis:
		values.flex_basis(ComputeLengthPercentageAuto(p, font_size, document_font_size, dp_ratio, vp_dimensions));
		break;

	case PropertyId::RmlUi_Language:
		values.language(p->Get<String>());
		break;
	case PropertyId::RmlUi_Direction:
		values.direction(p->Get<Direction>());
		break;

	// Fetched from element's properties.
	case PropertyId::Cursor:
	case PropertyId::Transition:
	case PropertyId::Animation:
	case PropertyId::AlignContent:
	case PropertyId::AlignItems:
	case PropertyId::AlignSelf:
	case PropertyId::FlexDirection:
	case PropertyId::FlexGrow:
	case PropertyId::FlexShrink:
	case PropertyId::FlexWrap:
	case PropertyId::JustifyContent:
	case PropertyId::JustifyItems:
	case PropertyId::JustifySelf:
	case PropertyId::GridTemplateColumns:
	case PropertyId::GridTemplateRows:
	case PropertyId::GridTemplateAreas:
	case PropertyId::GridAutoColumns:
	case PropertyId::GridAutoRows:
	case PropertyId::GridAutoFlow:
	case PropertyId::GridColumnStart:
	case PropertyId::GridColumnEnd:
	case PropertyId::GridRowStart:
	case PropertyId::GridRowEnd:
	case PropertyId::AspectRatio:
	case PropertyId::TextShadow:
	case PropertyId::WebkitTextStrokeWidth:
	case PropertyId::WebkitTextStrokeColor:
	case PropertyId::Content:
		break;
	// Navigation properties. Must be manually retrieved with 'GetProperty()'.
	case PropertyId::NavUp:
	case PropertyId::NavDown:
	case PropertyId::NavLeft:
	case PropertyId::NavRight:
		break;
	// Unhandled properties. Must be manually retrieved with 'GetProperty()'.
	case PropertyId::FillImage:
	case PropertyId::CaretColor:
	case PropertyId::MaskComposite:
	case PropertyId::MaskOrigin:
	case PropertyId::MaskClip:
	case PropertyId::BackgroundClip:
	case PropertyId::BackgroundOrigin:
	case PropertyId::ObjectFit:
	case PropertyId::ObjectPositionX:
	case PropertyId::ObjectPositionY:
	case PropertyId::ScrollbarWidth:
	case PropertyId::ScrollbarThumbColor:
	case PropertyId::ScrollbarTrackColor:
		break;
	// Invalid properties
	case PropertyId::Invalid:
	case PropertyId::NumDefinedIds:
	case PropertyId::MaxNumIds:
		break;
	}
	// clang-format on
}
} // namespace Rml
