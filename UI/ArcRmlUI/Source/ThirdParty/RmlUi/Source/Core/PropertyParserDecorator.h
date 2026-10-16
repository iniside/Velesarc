#pragma once

#include "../../Include/RmlUi/Core/PropertyParser.h"
#include "ControlledLifetimeResource.h"

namespace Rml {

/**
    A property parser for the decorator property.
 */

class PropertyParserDecorator : public PropertyParser {
public:
	PropertyParserDecorator();
	virtual ~PropertyParserDecorator();

	/// Called to parse a decorator declaration.
	bool ParseValue(Property& property, const String& value, const ParameterMap& parameters) const override;

	static void Initialize();
	static void Shutdown();

private:
	bool ParseLayerPositionComponent(const String& token, NumericValue& out_value, int& out_axis) const;
	bool ParseLayerPosition(const StringList& tokens, NumericValue& out_x, NumericValue& out_y) const;

	static ControlledLifetimeResource<struct PropertyParserDecoratorData> parser_data;
};

} // namespace Rml
