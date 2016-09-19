//
//  expressions.cpp
//  FloydSpeak
//
//  Created by Marcus Zetterquist on 03/08/16.
//  Copyright © 2016 Marcus Zetterquist. All rights reserved.
//

#include "expressions.h"

#include "text_parser.h"
#include "statements.h"
#include "parser_value.h"
#include "json_writer.h"
#include "utils.h"
#include "json_support.h"
#include <cmath>
#include "parser_primitives.h"


namespace floyd_parser {

using std::pair;
using std::string;
using std::vector;
using std::shared_ptr;
using std::make_shared;



string expression_to_json_string(const expression_t& e);





QUARK_UNIT_TEST("", "math_operation2_expr_t==()", "", ""){
	const auto a = expression_t::make_math_operation2(
		expression_t::math2_operation::k_add,
		expression_t::make_constant(3),
		expression_t::make_constant(4));
	const auto b = expression_t::make_math_operation2(
		expression_t::math2_operation::k_add,
		expression_t::make_constant(3),
		expression_t::make_constant(4));
	QUARK_TEST_VERIFY(a == b);
}

bool math_operation2_expr_t::operator==(const math_operation2_expr_t& other) const {
	return _operation == other._operation && *_left == *other._left && *_right == *other._right;
}

bool math_operation1_expr_t::operator==(const math_operation1_expr_t& other) const {
	return _operation == other._operation && *_input == *other._input;
}

bool conditional_operator_expr_t::operator==(const conditional_operator_expr_t& other) const {
	return *_condition == *other._condition && *_a == *other._a && *_b == *other._b;
}

bool resolve_variable_expr_t::operator==(const resolve_variable_expr_t& other) const{
	return _variable_name == other._variable_name;
}

bool resolve_member_expr_t::operator==(const resolve_member_expr_t& other) const{
	return _parent_address == other._parent_address && _member_name == other._member_name;
}

bool lookup_element_expr_t::operator==(const lookup_element_expr_t& other) const{
	return _parent_address == other._parent_address && _lookup_key == other._lookup_key ;
}




//////////////////////////////////////////////////		expression_t



bool expression_t::check_invariant() const{
	QUARK_ASSERT(_debug_aaaaaaaaaaaaaaaaaaaaaaa.size() > 0);

	//	Make sure exactly ONE pointer is set.
	QUARK_ASSERT(
		(_constant ? 1 : 0)
		+ (_math1 ? 1 : 0)
		+ (_math2 ? 1 : 0)
		+ (_conditional_operator ? 1 : 0)
		+ (_call ? 1 : 0)
		+ (_resolve_variable ? 1 : 0)
		+ (_resolve_member ? 1 : 0)
		+ (_lookup_element ? 1 : 0)
		 == 1);

	QUARK_ASSERT(_resolved_expression_type && _resolved_expression_type->check_invariant());
	return true;
}

bool expression_t::operator==(const expression_t& other) const {
	QUARK_ASSERT(check_invariant());
	QUARK_ASSERT(other.check_invariant());

	if(_constant){
		return compare_shared_values(_constant, other._constant);
	}
	else if(_math1){
		return compare_shared_values(_math1, other._math1);
	}
	else if(_math2){
		return compare_shared_values(_math2, other._math2);
	}
	else if(_conditional_operator){
		return compare_shared_values(_conditional_operator, other._conditional_operator);
	}
	else if(_call){
		return compare_shared_values(_call, other._call);
	}
	else if(_resolve_variable){
		return compare_shared_values(_resolve_variable, other._resolve_variable);
	}
	else if(_resolve_member){
		return compare_shared_values(_resolve_member, other._resolve_member);
	}
	else if(_lookup_element){
		return compare_shared_values(_lookup_element, other._lookup_element);
	}
	else{
		QUARK_ASSERT(false);
		return false;
	}
}


expression_t expression_t::make_constant(const value_t& value){
	QUARK_ASSERT(value.check_invariant());

	const auto resolved_expression_type = value.get_type();
	auto result = expression_t();
	result._constant = std::make_shared<value_t>(value);
	result._resolved_expression_type = value.get_type();
	result._debug_aaaaaaaaaaaaaaaaaaaaaaa = expression_to_json_string(result);
	QUARK_ASSERT(result.check_invariant());
	return result;
}

expression_t expression_t::make_constant(const char s[]){
	return make_constant(value_t(s));
}
expression_t expression_t::make_constant(const std::string& s){
	return make_constant(value_t(s));
}

expression_t expression_t::make_constant(const int i){
	return make_constant(value_t(i));
}

expression_t expression_t::make_constant(const bool i){
	return make_constant(value_t(i));
}

expression_t expression_t::make_constant(const float f){
	return make_constant(value_t(f));
}


expression_t expression_t::make_math_operation1(math1_operation op, const expression_t& input){
	QUARK_ASSERT(input.check_invariant());

	auto input2 = make_shared<expression_t>(input);

	auto result = expression_t();
	result._math1 = std::make_shared<math_operation1_expr_t>(math_operation1_expr_t{ op, input2 });
	result._resolved_expression_type = input.get_expression_type();
	result._debug_aaaaaaaaaaaaaaaaaaaaaaa = expression_to_json_string(result);
	QUARK_ASSERT(result.check_invariant());
	return result;
}

expression_t expression_t::make_math_operation2(math2_operation op, const expression_t& left, const expression_t& right){
	QUARK_ASSERT(left.check_invariant());
	QUARK_ASSERT(right.check_invariant());

	auto left2 = make_shared<expression_t>(left);
	auto right2 = make_shared<expression_t>(right);

	auto result = expression_t();
	result._math2 = std::make_shared<math_operation2_expr_t>(math_operation2_expr_t{ op, left2, right2 });
	result._resolved_expression_type = left.get_expression_type();
	result._debug_aaaaaaaaaaaaaaaaaaaaaaa = expression_to_json_string(result);
	QUARK_ASSERT(result.check_invariant());
	return result;
}

expression_t expression_t::make_conditional_operator(const expression_t& condition, const expression_t& a, const expression_t& b){
	QUARK_ASSERT(condition.check_invariant());
	QUARK_ASSERT(a.check_invariant());
	QUARK_ASSERT(b.check_invariant());

	auto condition2 = make_shared<expression_t>(condition);
	auto a2 = make_shared<expression_t>(a);
	auto b2 = make_shared<expression_t>(b);

	auto result = expression_t();
	result._conditional_operator = std::make_shared<conditional_operator_expr_t>(conditional_operator_expr_t{ condition2, a2,b2 });
	result._resolved_expression_type = a.get_expression_type();
	result._debug_aaaaaaaaaaaaaaaaaaaaaaa = expression_to_json_string(result);
	QUARK_ASSERT(result.check_invariant());
	return result;
}

expression_t expression_t::make_function_call(const type_identifier_t& function, const std::vector<expression_t>& inputs, const shared_ptr<const type_def_t>& resolved_expression_type){
	QUARK_ASSERT(function.check_invariant());
	for(const auto arg: inputs){
		QUARK_ASSERT(arg.check_invariant());
	}
	QUARK_ASSERT(resolved_expression_type && resolved_expression_type->check_invariant());

	auto result = expression_t();
	result._call = std::make_shared<function_call_expr_t>(function_call_expr_t{ function, inputs });
	result._resolved_expression_type = resolved_expression_type;
	result._debug_aaaaaaaaaaaaaaaaaaaaaaa = expression_to_json_string(result);
	QUARK_ASSERT(result.check_invariant());
	return result;
}


expression_t expression_t::make_resolve_variable(const std::string& variable, const shared_ptr<const type_def_t>& resolved_expression_type){
	QUARK_ASSERT(variable.size() > 0);
	QUARK_ASSERT(resolved_expression_type && resolved_expression_type->check_invariant());

	auto result = expression_t();
	result._resolve_variable = std::make_shared<resolve_variable_expr_t>(resolve_variable_expr_t{ variable });
	result._resolved_expression_type = resolved_expression_type;
	result._debug_aaaaaaaaaaaaaaaaaaaaaaa = expression_to_json_string(result);
	QUARK_ASSERT(result.check_invariant());
	return result;
}

expression_t expression_t::make_resolve_member(const expression_t& parent_address, const std::string& member_name, const shared_ptr<const type_def_t>& resolved_expression_type){
	QUARK_ASSERT(parent_address.check_invariant());
	QUARK_ASSERT(member_name.size() > 0);
	QUARK_ASSERT(resolved_expression_type && resolved_expression_type->check_invariant());

	auto result = expression_t();
	result._resolve_member = make_shared<resolve_member_expr_t>(resolve_member_expr_t{ parent_address, member_name });
	result._resolved_expression_type = resolved_expression_type;
	result._debug_aaaaaaaaaaaaaaaaaaaaaaa = expression_to_json_string(result);
	QUARK_ASSERT(result.check_invariant());
	return result;
}

expression_t expression_t::make_lookup(const expression_t& parent_address, const expression_t& lookup_key, const shared_ptr<const type_def_t>& resolved_expression_type){
	QUARK_ASSERT(parent_address.check_invariant());
	QUARK_ASSERT(lookup_key.check_invariant());
	QUARK_ASSERT(resolved_expression_type && resolved_expression_type->check_invariant());

	auto result = expression_t();
	result._lookup_element = std::make_shared<lookup_element_expr_t>(lookup_element_expr_t{ parent_address, lookup_key });
	result._resolved_expression_type = resolved_expression_type;
	result._debug_aaaaaaaaaaaaaaaaaaaaaaa = expression_to_json_string(result);
	QUARK_ASSERT(result.check_invariant());
	return result;
}



string operation_to_string(const expression_t::math2_operation& op){

	if(op == expression_t::math2_operation::k_add){
		return "+";
	}
	else if(op == expression_t::math2_operation::k_subtract){
		return "-";
	}
	else if(op == expression_t::math2_operation::k_multiply){
		return "*";
	}
	else if(op == expression_t::math2_operation::k_divide){
		return "/";
	}
	else if(op == expression_t::math2_operation::k_remainder){
		return "%";
	}

	else if(op == expression_t::math2_operation::k_smaller_or_equal){
		return "<=";
	}
	else if(op == expression_t::math2_operation::k_smaller){
		return "<";
	}
	else if(op == expression_t::math2_operation::k_larger_or_equal){
		return ">=";
	}
	else if(op == expression_t::math2_operation::k_larger){
		return ">";
	}

	else if(op == expression_t::math2_operation::k_logical_equal){
		return "==";
	}
	else if(op == expression_t::math2_operation::k_logical_nonequal){
		return "!=";
	}
	else if(op == expression_t::math2_operation::k_logical_and){
		return "&&";
	}
	else if(op == expression_t::math2_operation::k_logical_or){
		return "||";
	}

	else{
		QUARK_ASSERT(false);
	}
}

QUARK_UNIT_TESTQ("operation_to_string()", ""){
	quark::ut_compare(operation_to_string(expression_t::math2_operation::k_add), "+");
}
QUARK_UNIT_TESTQ("operation_to_string()", ""){
	quark::ut_compare(operation_to_string(expression_t::math2_operation::k_subtract), "-");
}
QUARK_UNIT_TESTQ("operation_to_string()", ""){
	quark::ut_compare(operation_to_string(expression_t::math2_operation::k_multiply), "*");
}
QUARK_UNIT_TESTQ("operation_to_string()", ""){
	quark::ut_compare(operation_to_string(expression_t::math2_operation::k_divide), "/");
}
QUARK_UNIT_TESTQ("operation_to_string()", ""){
	quark::ut_compare(operation_to_string(expression_t::math2_operation::k_remainder), "%");
}


QUARK_UNIT_TESTQ("operation_to_string()", ""){
	quark::ut_compare(operation_to_string(expression_t::math2_operation::k_smaller_or_equal), "<=");
}
QUARK_UNIT_TESTQ("operation_to_string()", ""){
	quark::ut_compare(operation_to_string(expression_t::math2_operation::k_smaller), "<");
}
QUARK_UNIT_TESTQ("operation_to_string()", ""){
	quark::ut_compare(operation_to_string(expression_t::math2_operation::k_larger_or_equal), ">=");
}
QUARK_UNIT_TESTQ("operation_to_string()", ""){
	quark::ut_compare(operation_to_string(expression_t::math2_operation::k_larger), ">");
}

QUARK_UNIT_TESTQ("operation_to_string()", ""){
	quark::ut_compare(operation_to_string(expression_t::math2_operation::k_logical_equal), "==");
}
QUARK_UNIT_TESTQ("operation_to_string()", ""){
	quark::ut_compare(operation_to_string(expression_t::math2_operation::k_logical_nonequal), "!=");
}
QUARK_UNIT_TESTQ("operation_to_string()", ""){
	quark::ut_compare(operation_to_string(expression_t::math2_operation::k_logical_and), "&&");
}
QUARK_UNIT_TESTQ("operation_to_string()", ""){
	quark::ut_compare(operation_to_string(expression_t::math2_operation::k_logical_or), "||");
}



string operation_to_string(const expression_t::math1_operation& op){
	if(op == expression_t::math1_operation::negate){
		return "negate";
	}
	else{
		QUARK_ASSERT(false);
	}
}

QUARK_UNIT_TESTQ("operation_to_string()", ""){
	quark::ut_compare(operation_to_string(expression_t::math1_operation::negate), "negate");
}


void trace(const expression_t& e){
	QUARK_ASSERT(e.check_invariant());
	const auto json = expression_to_json(e);
	const auto s = json_to_compact_string(json);
	QUARK_TRACE(s);
}




////////////////////////////////////////////		JSON SUPPORT



/*
	An expression is a json array where entries may be other json arrays.
	["+", ["+", 1, 2], ["k", 10]]
*/
json_value_t expression_to_json(const expression_t& e){
	const auto expression_base_type = e._resolved_expression_type->get_type();
	json_value_t type;
	if(expression_base_type == base_type::k_null){
		type = json_value_t();
	}
	else{
		const auto type_string = e._resolved_expression_type->to_string();
		type = json_value_t(std::string("<") + type_string + ">");
	}

	if(e._constant){
		return json_value_t::make_array_skip_nulls({ json_value_t("k"), value_to_json(*e._constant), type });
	}
	else if(e._math2){
		const auto e2 = *e._math2;
		const auto left = expression_to_json(*e2._left);
		const auto right = expression_to_json(*e2._right);
		return json_value_t::make_array_skip_nulls({ json_value_t(operation_to_string(e2._operation)), left, right, type });
	}
	else if(e._math1){
		const auto e2 = *e._math1;
		const auto input = expression_to_json(*e2._input);
		return json_value_t::make_array_skip_nulls({ json_value_t(operation_to_string(e2._operation)), input, type });
	}
	else if(e._conditional_operator){
		const auto e2 = *e._conditional_operator;
		const auto condition = expression_to_json(*e2._condition);
		const auto a = expression_to_json(*e2._a);
		const auto b = expression_to_json(*e2._b);
		return json_value_t::make_array_skip_nulls({ json_value_t("?:"), condition, a, b, type });
	}
	else if(e._call){
		const auto& call_function = *e._call;
		vector<json_value_t>  args_json;
		for(const auto& i: call_function._inputs){
			const auto arg_expr = expression_to_json(i);
			args_json.push_back(arg_expr);
		}
		return json_value_t::make_array_skip_nulls({ json_value_t("call"), json_value_t(call_function._function.to_string()), args_json, type });
	}
	else if(e._resolve_variable){
		const auto e2 = *e._resolve_variable;
		return json_value_t::make_array_skip_nulls({ json_value_t("@"), json_value_t(e2._variable_name), type });
	}
	else if(e._resolve_member){
		const auto e2 = *e._resolve_member;
		return json_value_t::make_array_skip_nulls({ json_value_t("->"), expression_to_json(e2._parent_address), json_value_t(e2._member_name), type });
	}
	else if(e._lookup_element){
		const auto e2 = *e._lookup_element;
		const auto lookup_key = expression_to_json(e2._lookup_key);
		const auto parent_address = expression_to_json(e2._parent_address);
		return json_value_t::make_array_skip_nulls({ json_value_t("[-]"), parent_address, lookup_key, type });
	}
	else{
		QUARK_ASSERT(false);
	}
}

string expression_to_json_string(const expression_t& e){
	const auto json = expression_to_json(e);
	return json_to_compact_string(json);
}

QUARK_UNIT_TESTQ("expression_to_json()", "constants"){
	quark::ut_compare(expression_to_json_string(expression_t::make_constant(13)), R"(["k", 13, "<int>"])");
	quark::ut_compare(expression_to_json_string(expression_t::make_constant("xyz")), R"(["k", "xyz", "<string>"])");
	quark::ut_compare(expression_to_json_string(expression_t::make_constant(14.0f)), R"(["k", 14, "<float>"])");
	quark::ut_compare(expression_to_json_string(expression_t::make_constant(true)), R"(["k", true, "<bool>"])");
	quark::ut_compare(expression_to_json_string(expression_t::make_constant(false)), R"(["k", false, "<bool>"])");
}

QUARK_UNIT_TESTQ("expression_to_json()", "math1"){
	quark::ut_compare(
		expression_to_json_string(
			expression_t::make_math_operation1(expression_t::math1_operation::negate, expression_t::make_constant(2))),
		R"(["negate", ["k", 2, "<int>"], "<int>"])"
	);
}

QUARK_UNIT_TESTQ("expression_to_json()", "math2"){
	quark::ut_compare(
		expression_to_json_string(
			expression_t::make_math_operation2(expression_t::math2_operation::k_add, expression_t::make_constant(2), expression_t::make_constant(3))),
		R"(["+", ["k", 2, "<int>"], ["k", 3, "<int>"], "<int>"])"
	);
}

QUARK_UNIT_TESTQ("expression_to_json()", "call"){
	quark::ut_compare(
		expression_to_json_string(
			expression_t::make_function_call(
				type_identifier_t::make("my_func"),
				{
					expression_t::make_constant("xyz"),
					expression_t::make_constant(123)
				},
				type_def_t::make_string_typedef()
			)
		),
		R"(["call", "my_func", [["k", "xyz", "<string>"], ["k", 123, "<int>"]], "<string>"])"
	);
}

QUARK_UNIT_TESTQ("expression_to_json()", "lookup"){
	quark::ut_compare(
		expression_to_json_string(
			expression_t::make_lookup(
				expression_t::make_resolve_variable("hello", type_def_t::make_string_typedef()),
				expression_t::make_constant("xyz"),
				type_def_t::make_string_typedef()
			)
		),
		R"(["[-]", ["@", "hello", "<string>"], ["k", "xyz", "<string>"], "<string>"])"
	);
}


expression_t::math2_operation string_to_math2_op(const string& op){
	if(op == "+"){
		return expression_t::math2_operation::k_add;
	}
	else if(op == "-"){
		return expression_t::math2_operation::k_subtract;
	}
	else if(op == "*"){
		return expression_t::math2_operation::k_multiply;
	}
	else if(op == "/"){
		return expression_t::math2_operation::k_divide;
	}
	else if(op == "%"){
		return expression_t::math2_operation::k_remainder;
	}

	else if(op == "<="){
		return expression_t::math2_operation::k_smaller_or_equal;
	}
	else if(op == "<"){
		return expression_t::math2_operation::k_smaller;
	}
	else if(op == ">="){
		return expression_t::math2_operation::k_larger_or_equal;
	}
	else if(op == ">"){
		return expression_t::math2_operation::k_larger;
	}

	else if(op == "=="){
		return expression_t::math2_operation::k_logical_equal;
	}
	else if(op == "!="){
		return expression_t::math2_operation::k_logical_nonequal;
	}
	else if(op == "&&"){
		return expression_t::math2_operation::k_logical_and;
	}
	else if(op == "||"){
		return expression_t::math2_operation::k_logical_or;
	}

	else{
		QUARK_ASSERT(false);
	}
}


bool is_math1_op(const string& op){
	return op == "neg";
}

bool is_math2_op(const string& op){
	//???	HOw to handle lookup?
	QUARK_ASSERT(op != "[-]");

	return
		op == "+" || op == "-" || op == "*" || op == "/" || op == "%"
		|| op == "<=" || op == "<" || op == ">=" || op == ">"
		|| op == "==" || op == "!=" || op == "&&" || op == "||";
}



	//////////////////////////////////////////////////		visit()


/*
expression_t visit(const visit_expression_i& v, const expression_t& e){
	if(e._constant){
		return v.visit_expression_i__on_constant(*e._constant);
	}
	else if(e._math1){
		return v.visit_expression_i__on_math1(*e._math1);
	}
	else if(e._math2){
		return v.visit_expression_i__on_math2(*e._math2);
	}
	else if(e._call){
		return v.visit_expression_i__on_call(*e._call);
	}
	else if(e._resolve_variable){
		return v.visit_expression_i__on_resolve_variable(*e._resolve_variable);
	}
	else if(e._resolve_member){
		return v.visit_expression_i__on_resolve_member(*e._resolve_member);
	}
	else if(e._lookup_element){
		return v.visit_expression_i__on_lookup_element(*e._lookup_element);
	}
	else{
		QUARK_ASSERT(false);
	}
}
*/




}	//	floyd_parser

