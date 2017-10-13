#include <mbgl/style/expression/parse.hpp>
#include <mbgl/style/expression/compound_expression.hpp>
#include <mbgl/style/expression/parse/at.hpp>
#include <mbgl/style/expression/parse/array_assertion.hpp>
#include <mbgl/style/expression/parse/case.hpp>
#include <mbgl/style/expression/parse/coalesce.hpp>
#include <mbgl/style/expression/parse/compound_expression.hpp>
#include <mbgl/style/expression/parse/curve.hpp>
#include <mbgl/style/expression/parse/in.hpp>
#include <mbgl/style/expression/parse/let.hpp>
#include <mbgl/style/expression/parse/literal.hpp>
#include <mbgl/style/expression/parse/match.hpp>
#include <mbgl/style/expression/type.hpp>
#include <mbgl/style/conversion/get_json_type.hpp>
#include <mbgl/style/expression/check_subtype.hpp>


namespace mbgl {
namespace style {
namespace expression {

ParseResult parseExpression(const mbgl::style::conversion::Value& value, ParsingContext context)
{
    using namespace mbgl::style::conversion;
    
    ParseResult parsed;
    
    if (isArray(value)) {
        const std::size_t length = arrayLength(value);
        if (length == 0) {
            context.error(R"(Expected an array with at least one element. If you wanted a literal array, use ["literal", []].)");
            return ParseResult();
        }
        
        const optional<std::string> op = toString(arrayMember(value, 0));
        if (!op) {
            context.error(
                "Expression name must be a string, but found " + getJSONType(arrayMember(value, 0)) +
                    R"( instead. If you wanted a literal array, use ["literal", [...]].)",
                0
            );
            return ParseResult();
        }
        
        if (*op == "literal") {
            if (length != 2) {
                context.error(
                    "'literal' expression requires exactly one argument, but found " + std::to_string(length - 1) + " instead."
                );
                return ParseResult();
            }
            
            parsed = ParseLiteral::parse(arrayMember(value, 1), context);
        } else if (*op == "match") {
            parsed = ParseMatch::parse(value, context);
        } else if (*op == "curve") {
            parsed = ParseCurve::parse(value, context);
        } else if (*op == "coalesce") {
            parsed = ParseCoalesce::parse(value, context);
        } else if (*op == "case") {
            parsed = ParseCase::parse(value, context);
        } else if (*op == "array") {
            parsed = ParseArrayAssertion::parse(value, context);
        } else if (*op == "let") {
            parsed = ParseLet::parse(value, context);
        } else if (*op == "var") {
            parsed = ParseVar::parse(value, context);
        } else if (*op == "at") {
            parsed = ParseAt::parse(value, context);
        } else if (*op == "contains") {
            parsed = ParseIn::parse(value, context);
        } else {
            parsed = ParseCompoundExpression::parse(*op, value, context);
        }
    } else {
        if (isObject(value)) {
            context.error(R"(Bare objects invalid. Use ["literal", {...}] instead.)");
            return ParseResult();
        }
        
        parsed = ParseLiteral::parse(value, context);
    }
    
    if (!parsed) {
        assert(context.errors.size() > 0);
    } else if (context.expected) {
        auto wrapForType = [&](const std::string& wrapper, std::unique_ptr<Expression> expression) {
            std::vector<std::unique_ptr<Expression>> args;
            args.push_back(std::move(expression));
            return createCompoundExpression(wrapper, std::move(args), context);
        };
        
        const type::Type actual = (*parsed)->getType();
        const type::Type expected = *context.expected;
        if (expected == type::Color && (actual == type::String || actual == type::Value)) {
            parsed = wrapForType("to-color", std::move(*parsed));
        } else if (expected != type::Value && actual == type::Value) {
            if (expected == type::String) {
                parsed = wrapForType("string", std::move(*parsed));
            } else if (expected == type::Number) {
                parsed = wrapForType("number", std::move(*parsed));
            } else if (expected == type::Boolean) {
                parsed = wrapForType("boolean", std::move(*parsed));
            }
        }
    
        checkSubtype(*(context.expected), (*parsed)->getType(), context);
        if (context.errors.size() > 0) {
            return ParseResult();
        }
    }
    
    return parsed;
}

} // namespace expression
} // namespace style
} // namespace mbgl
