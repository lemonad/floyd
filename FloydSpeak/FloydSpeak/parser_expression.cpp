//
//  parse_expression.cpp
//  FloydSpeak
//
//  Created by Marcus Zetterquist on 26/07/16.
//  Copyright © 2016 Marcus Zetterquist. All rights reserved.
//

#include "parser_expression.hpp"


#include "text_parser.h"
#include "parser_statement.hpp"

#include <cmath>



namespace floyd_parser {


using std::pair;
using std::string;
using std::vector;
using std::shared_ptr;
using std::make_shared;


QUARK_UNIT_TEST("", "math_operation2_expr_t==()", "", ""){
	const auto a = make_math_operation2_expr(math_operation2_expr_t::add, make_constant(3), make_constant(4));
	const auto b = make_math_operation2_expr(math_operation2_expr_t::add, make_constant(3), make_constant(4));
	QUARK_TEST_VERIFY(a == b);
}








pair<expression_t, string> parse_summands(const identifiers_t& identifiers, const string& s, int depth);


expression_t negate_expression(const expression_t& e){
	//	Shortcut: directly negate numeric constants. This makes parse tree cleaner and is non-lossy.
	if(e._constant){
		const value_t& value = *e._constant;
		if(value.get_type() == make_type_identifier("int")){
			return make_constant(value_t{-value.get_int()});
		}
		else if(value.get_type() == make_type_identifier("float")){
			return make_constant(value_t{-value.get_float()});
		}
	}

	return make_math_operation1(math_operation1_expr_t::negate, e);
}

float parse_float(const string& pos){
	size_t end = -1;
	auto res = std::stof(pos, &end);
	if(isnan(res) || end == 0){
		throw std::runtime_error("EEE_WRONG_CHAR");
	}
	return res;
}



/*
	Constant literal
		3
		3.0
		"three"
	Function call
		f ()
		f(g())
		f(a + "xyz")
		f(a + "xyz", 1000 * 3)
	Variable
		x1
		hello2

	FUTURE
		- Add member access: my_data.name.first_name
		- Add lambda / local function
*/
pair<expression_t, string> parse_single_internal(const identifiers_t& identifiers, const string& s) {
	QUARK_ASSERT(identifiers.check_invariant());
	QUARK_ASSERT(s.size() > 0);

	string pos = s;

	//	" => string constant.
	if(peek_string(pos, "\"")){
		pos = pos.substr(1);
		const auto string_constant_pos = read_until(pos, "\"");

		pos = string_constant_pos.second;
		pos = pos.substr(1);
		return { value_t{ string_constant_pos.first }, pos };
	}

	// [0-9] and "."  => numeric constant.
	else if((number_chars + ".").find(pos[0]) != string::npos){
		const auto number_pos = read_while(pos, number_chars + ".");
		if(number_pos.first.empty()){
			throw std::runtime_error("EEE_WRONG_CHAR");
		}

		//	If it contains a "." its a float, else an int.
		if(number_pos.first.find('.') != string::npos){
			pos = pos.substr(number_pos.first.size());
			float value = parse_float(number_pos.first);
			return { value_t{value}, pos };
		}
		else{
			pos = pos.substr(number_pos.first.size());
			int value = atoi(number_pos.first.c_str());
			return { value_t{value}, pos };
		}
	}

	/*
		Start of identifier: can be variable access or function call.

		"hello"
		"f ()"
		"f(x + 10)"
	*/
	else if(identifier_chars.find(pos[0]) != string::npos){
		const auto identifier_pos = read_required_identifier(pos);

		string p2 = skip_whitespace(identifier_pos.second);

		//	Function call.
		if(!p2.empty() && p2[0] == '('){
			//	Lookup function!
			if(identifiers._functions.find(identifier_pos.first) == identifiers._functions.end()){
				throw std::runtime_error("Unknown function \"" + identifier_pos.first + "\"");
			}

			const auto arg_list_pos = get_balanced(p2);
			const auto args = trim_ends(arg_list_pos.first);

			p2 = args;
			vector<std::shared_ptr<expression_t>> args_expressions;
			while(!p2.empty()){
				const auto p3 = read_until(skip_whitespace(p2), ",");
				expression_t arg_expre = parse_expression(identifiers, p3.first);
				args_expressions.push_back(std::make_shared<expression_t>(arg_expre));
				p2 = p3.second[0] == ',' ? p3.second.substr(1) : p3.second;
			}
			//	??? Check types of arguments and count.

			return { function_call_expr_t{identifier_pos.first, args_expressions }, arg_list_pos.second };
		}

		//	Variable-read.
		else{
			//	Lookup value!
			if(identifiers._constant_values.find(identifier_pos.first) == identifiers._constant_values.end()){
				throw std::runtime_error("Unknown identifier \"" + identifier_pos.first + "\"");
			}
			return { variable_read_expr_t{identifier_pos.first }, p2 };
		}
	}
	else{
		throw std::runtime_error("EEE_WRONG_CHAR");
	}
}

pair<expression_t, string> parse_single(const identifiers_t& identifiers, const string& s){
	QUARK_ASSERT(identifiers.check_invariant());

	const auto result = parse_single_internal(identifiers, s);
	trace(result.first);
	return result;
}


shared_ptr<const function_def_expr_t> make_log_function(){
	vector<arg_t> args{ {make_type_identifier("float"), "value"} };
	function_body_t body{
		{
			make__return_statement(
				return_statement_t{ std::make_shared<expression_t>(make_constant(value_t(123.f))) }
			)
		}
	};

	return make_shared<const function_def_expr_t>(function_def_expr_t{ make_type_identifier("float"), args, body });
}

shared_ptr<const function_def_expr_t> make_log2_function(){
	vector<arg_t> args{ {make_type_identifier("string"), "s"}, {make_type_identifier("float"), "v"} };
	function_body_t body{
		{
			make__return_statement(
				return_statement_t{ make_shared<expression_t>(make_constant(value_t(456.7f))) }
			)
		}
	};

	return make_shared<const function_def_expr_t>(function_def_expr_t{ make_type_identifier("float"), args, body });
}

shared_ptr<const function_def_expr_t> make_return5(){
	vector<arg_t> args{};
	function_body_t body{
		{
			make__return_statement(
				return_statement_t{ make_shared<expression_t>(make_constant(value_t(5))) }
			)
		}
	};

	return make_shared<const function_def_expr_t>(function_def_expr_t{ make_type_identifier("int"), args, body });
}


identifiers_t make_test_functions(){
	identifiers_t result;
	result._functions["log"] = make_log_function();
	result._functions["log2"] = make_log2_function();
	result._functions["f"] = make_log_function();
	result._functions["return5"] = make_return5();
	return result;
}


QUARK_UNIT_TESTQ("parse_single", "number"){
	identifiers_t identifiers;
	QUARK_TEST_VERIFY((parse_single(identifiers, "9.0") == pair<expression_t, string>(value_t{ 9.0f }, "")));
}

QUARK_UNIT_TESTQ("parse_single", "function call"){
	const auto identifiers = make_test_functions();
	const auto a = parse_single(identifiers, "log(34.5)");
	QUARK_TEST_VERIFY(a.first._call_function_expr->_function_name == "log");
	QUARK_TEST_VERIFY(a.first._call_function_expr->_inputs.size() == 1);
	QUARK_TEST_VERIFY(*a.first._call_function_expr->_inputs[0]->_constant == value_t(34.5f));
	QUARK_TEST_VERIFY(a.second == "");
}

QUARK_UNIT_TESTQ("parse_single", "function call with two args"){
	const auto identifiers = make_test_functions();
	const auto a = parse_single(identifiers, "log2(\"123\" + \"xyz\", 1000 * 3)");
	QUARK_TEST_VERIFY(a.first._call_function_expr->_function_name == "log2");
	QUARK_TEST_VERIFY(a.first._call_function_expr->_inputs.size() == 2);
	QUARK_TEST_VERIFY(a.first._call_function_expr->_inputs[0]->_math_operation2_expr);
	QUARK_TEST_VERIFY(a.first._call_function_expr->_inputs[1]->_math_operation2_expr);
	QUARK_TEST_VERIFY(a.second == "");
}

QUARK_UNIT_TESTQ("parse_single", "nested function calls"){
	const auto identifiers = make_test_functions();
	const auto a = parse_single(identifiers, "log2(2.1, f(3.14))");
	QUARK_TEST_VERIFY(a.first._call_function_expr->_function_name == "log2");
	QUARK_TEST_VERIFY(a.first._call_function_expr->_inputs.size() == 2);
	QUARK_TEST_VERIFY(a.first._call_function_expr->_inputs[0]->_constant);
	QUARK_TEST_VERIFY(a.first._call_function_expr->_inputs[1]->_call_function_expr->_function_name == "f");
	QUARK_TEST_VERIFY(a.first._call_function_expr->_inputs[1]->_call_function_expr->_inputs.size() == 1);
	QUARK_TEST_VERIFY(*a.first._call_function_expr->_inputs[1]->_call_function_expr->_inputs[0] == value_t(3.14f));
	QUARK_TEST_VERIFY(a.second == "");
}


// Parse a constant or an expression in parenthesis
pair<expression_t, string> parse_atom(const identifiers_t& identifiers, const string& s, int depth) {
	string pos = skip_whitespace(s);

	//	Handle the sign before parenthesis (or before number)
	auto negative = false;
	while(pos.size() > 0 && (is_whitespace(pos[0]) || (pos[0] == '-' || pos[0] == '+'))){
		if(pos[0] == '-') {
			negative = !negative;
		}
		pos = pos.substr(1);
	}

	// Check if there is parenthesis
	if(pos.size() > 0 && pos[0] == '(') {
		pos = pos.substr(1);

		auto res = parse_summands(identifiers, pos, depth);
		pos = skip_whitespace(res.second);
		if(!(res.second.size() > 0 && res.second[0] == ')')) {
			// Unmatched opening parenthesis
			throw std::runtime_error("EEE_PARENTHESIS");
		}
		pos = pos.substr(1);
		const auto expr = res.first;
		return { negative ? negate_expression(expr) : expr, pos };
	}

	//	Parse constant literal / function call / variable-access.
	if(pos.size() > 0){
		const auto single_pos = parse_single(identifiers, pos);
		return { negative ? negate_expression(single_pos.first) : single_pos.first, single_pos.second };
	}

	throw std::runtime_error("Expected number");
}

//### more tests here!
QUARK_UNIT_TEST("", "parse_atom", "", ""){
	identifiers_t identifiers;

	QUARK_TEST_VERIFY((parse_atom(identifiers, "0.0", 0) == pair<expression_t, string>(value_t{ 0.0f }, "")));
	QUARK_TEST_VERIFY((parse_atom(identifiers, "9.0", 0) == pair<expression_t, string>(value_t{ 9.0f }, "")));
	QUARK_TEST_VERIFY((parse_atom(identifiers, "12345.0", 0) == pair<expression_t, string>(value_t{ 12345.0f }, "")));

	QUARK_TEST_VERIFY((parse_atom(identifiers, "10.0", 0) == pair<expression_t, string>(value_t{ 10.0f }, "")));
	QUARK_TEST_VERIFY((parse_atom(identifiers, "-10.0", 0) == pair<expression_t, string>(value_t{ -10.0f }, "")));
	QUARK_TEST_VERIFY((parse_atom(identifiers, "+10.0", 0) == pair<expression_t, string>(value_t{ 10.0f }, "")));

	QUARK_TEST_VERIFY((parse_atom(identifiers, "4.0+", 0) == pair<expression_t, string>(value_t{ 4.0f }, "+")));


	QUARK_TEST_VERIFY((parse_atom(identifiers, "\"hello\"", 0) == pair<expression_t, string>(value_t{ "hello" }, "")));
}


pair<expression_t, string> parse_factors(const identifiers_t& identifiers, const string& s, int depth) {
	const auto num1_pos = parse_atom(identifiers, s, depth);
	auto result_expression = num1_pos.first;
	string pos = skip_whitespace(num1_pos.second);
	while(!pos.empty() && (pos[0] == '*' || pos[0] == '/')){
		const auto op_pos = read_char(pos);
		const auto expression2_pos = parse_atom(identifiers, op_pos.second, depth);
		if(op_pos.first == '/') {
			result_expression = make_math_operation2_expr(math_operation2_expr_t::divide, result_expression, expression2_pos.first);
		}
		else{
			result_expression = make_math_operation2_expr(math_operation2_expr_t::multiply, result_expression, expression2_pos.first);
		}
		pos = skip_whitespace(expression2_pos.second);
	}
	return { result_expression, pos };
}

pair<expression_t, string> parse_summands(const identifiers_t& identifiers, const string& s, int depth) {
	const auto num1_pos = parse_factors(identifiers, s, depth);
	auto result_expression = num1_pos.first;
	string pos = num1_pos.second;
	while(!pos.empty() && (pos[0] == '-' || pos[0] == '+')) {
		const auto op_pos = read_char(pos);

		const auto expression2_pos = parse_factors(identifiers, op_pos.second, depth);
		if(op_pos.first == '-'){
			result_expression = make_math_operation2_expr(math_operation2_expr_t::subtract, result_expression, expression2_pos.first);
		}
		else{
			result_expression = make_math_operation2_expr(math_operation2_expr_t::add, result_expression, expression2_pos.first);
		}

		pos = skip_whitespace(expression2_pos.second);
	}
	return { result_expression, pos };
}




string operation_to_string(const math_operation2_expr_t::operation& op){
	if(op == math_operation2_expr_t::add){
		return "add";
	}
	else if(op == math_operation2_expr_t::subtract){
		return "subtract";
	}
	else if(op == math_operation2_expr_t::multiply){
		return "multiply";
	}
	else if(op == math_operation2_expr_t::divide){
		return "divide";
	}
	else{
		QUARK_ASSERT(false);
	}
}

string operation_to_string(const math_operation1_expr_t::operation& op){
	if(op == math_operation1_expr_t::negate){
		return "negate";
	}
	else{
		QUARK_ASSERT(false);
	}
}




void trace(const function_def_expr_t& e){
	QUARK_SCOPED_TRACE("function_def_expr_t");

	{
		QUARK_SCOPED_TRACE("return");
		trace(e._return_type);
	}
	{
		trace_vec("arguments", e._args);
	}
	{
		trace(e._body);
	}
}

void trace(const math_operation2_expr_t& e){
	string s = "math_operation2_expr_t: " + operation_to_string(e._operation);
	QUARK_SCOPED_TRACE(s);
	trace(*e._left);
	trace(*e._right);
}
void trace(const math_operation1_expr_t& e){
	string s = "math_operation1_expr_t: " + operation_to_string(e._operation);
	QUARK_SCOPED_TRACE(s);
	trace(*e._input);
}

void trace(const function_call_expr_t& e){
	string s = "function_call_expr_t: " + e._function_name;
	QUARK_SCOPED_TRACE(s);
	for(const auto i: e._inputs){
		trace(*i);
	}
}
void trace(const variable_read_expr_t& e){
	string s = "variable_read_expr_t: " + e._variable_name;
}


void trace(const expression_t& e){
	if(e._constant){
		trace(*e._constant);
	}
	else if(e._function_def_expr){
		const function_def_expr_t& temp = *e._function_def_expr;
		trace(temp);
	}
	else if(e._math_operation2_expr){
		trace(*e._math_operation2_expr);
	}
	else if(e._math_operation1_expr){
		trace(*e._math_operation1_expr);
	}
	else if(e._nop_expr){
		QUARK_TRACE("NOP");
	}
	else if(e._call_function_expr){
		trace(*e._call_function_expr);
	}
	else if(e._variable_read_expr){
		trace(*e._variable_read_expr);
	}
	else{
		QUARK_ASSERT(false);
	}
}




expression_t parse_expression(const identifiers_t& identifiers, string expression){
	if(expression.empty()){
		throw std::runtime_error("EEE_WRONG_CHAR");
	}
	auto result = parse_summands(identifiers, expression, 0);
	if(!result.second.empty()){
		throw std::runtime_error("EEE_WRONG_CHAR");
	}

	QUARK_TRACE("Expression: \"" + expression + "\"");
	trace(result.first);
	return result.first;
}


//??? where are unit tests???
}


