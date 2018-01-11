//
//  parser_evaluator.cpp
//  FloydSpeak
//
//  Created by Marcus Zetterquist on 26/07/16.
//  Copyright © 2016 Marcus Zetterquist. All rights reserved.
//

#include "floyd_interpreter.h"

#include "parse_expression.h"
#include "parse_statement.h"
#include "statements.h"
#include "floyd_parser.h"
#include "parser_value.h"
#include "pass2.h"
#include "pass3.h"
#include "json_support.h"

#include <cmath>
#include <sys/time.h>

#include <thread>
#include <chrono>

namespace floyd_interpreter {


using std::vector;
using std::string;
using std::pair;
using std::shared_ptr;
using std::unique_ptr;
using std::make_shared;

using namespace floyd_ast;



std::pair<interpreter_t, expression_t> evaluate_call_expression(const interpreter_t& vm, const expression_t& e);
value_t make_struct_instance(const interpreter_t& vm, const floyd_basics::typeid_t& struct_type);

namespace {


	std::pair<interpreter_t, shared_ptr<value_t>> execute_statements(const interpreter_t& vm, const vector<shared_ptr<statement_t>>& statements);

	bool compare_float_approx(float value, float expected){
		float diff = static_cast<float>(fabs(value - expected));
		return diff < 0.00001;
	}


	interpreter_t begin_subenv(const interpreter_t& vm){
		QUARK_ASSERT(vm.check_invariant());

		auto vm2 = vm;

		auto parent_env = vm2._call_stack.back();
		auto new_environment = environment_t::make_environment(vm2, parent_env);
		vm2._call_stack.push_back(new_environment);

//		QUARK_TRACE(json_to_pretty_string(interpreter_to_json(vm2)));
		return vm2;
	}



	std::pair<interpreter_t, shared_ptr<value_t>> execute_statements_in_env(
		const interpreter_t& vm,
		const std::vector<std::shared_ptr<statement_t>>& statements,
		const std::map<std::string, floyd_ast::value_t>& values
	){
		QUARK_ASSERT(vm.check_invariant());

		auto vm2 = begin_subenv(vm);

		vm2._call_stack.back()->_values.insert(values.begin(), values.end());

		const auto r = execute_statements(vm2, statements);
		vm2 = r.first;
		vm2._call_stack.pop_back();
		return { vm2, r.second };
	}

	//	Output is the RETURN VALUE of the executed statement, if any.
	std::pair<interpreter_t, shared_ptr<value_t>> execute_statement(const interpreter_t& vm0, const statement_t& statement){
		QUARK_ASSERT(vm0.check_invariant());
		QUARK_ASSERT(statement.check_invariant());

		auto vm2 = vm0;

		if(statement._bind){
			const auto& s = statement._bind;
			const auto name = s->_new_variable_name;
			if(vm2._call_stack.back()->_values.count(name) != 0){
				throw std::runtime_error("Local value already exists.");
			}
			const auto result = evaluate_expression(vm2, s->_expression);
			vm2 = result.first;
			const auto result_value = result.second;

			//??? Should accept function defintion too! Make function definition at type of literal?
			if(!result_value.is_literal()){
				throw std::runtime_error("Cannot evaluate expression.");
			}


			if(result_value.is_literal() == false){
				throw std::runtime_error("Cannot evaluate expression.");
			}

			const auto dest_type = s->_bindtype;
			const auto source_type = result_value.get_literal().get_type();
			if((dest_type == source_type) == false){
				if(source_type.is_null()){
					//	Workaround to allow calling functions without return value, like print().
				}
				else{
					throw std::runtime_error("Types not compatible in bind.");
				}
			}

			vm2._call_stack.back()->_values[name] = result_value.get_literal();
			return { vm2, {}};
		}
		else if(statement._block){
			const auto& s = statement._block;
			return execute_statements_in_env(vm2, s->_statements, {});
		}
		else if(statement._return){
			const auto& s = statement._return;
			const auto expr = s->_expression;
			const auto result = evaluate_expression(vm2, expr);
			vm2 = result.first;
			const auto result_value = result.second;

			if(!result_value.is_literal()){
				throw std::runtime_error("undefined");
			}

			return { vm2, make_shared<value_t>(result_value.get_literal()) };
		}
		else if(statement._if){
			const auto& s = statement._if;
			const auto condition_result = evaluate_expression(vm2, s->_condition);
			vm2 = condition_result.first;
			const auto condition_result_value = condition_result.second;
			if(!condition_result_value.is_literal() || !condition_result_value.get_literal().is_bool()){
				throw std::runtime_error("Boolean condition required.");
			}

			bool r = condition_result_value.get_literal().get_bool();
			if(r){
				return execute_statements_in_env(vm2, s->_then_statements, {});
			}
			else{
				return execute_statements_in_env(vm2, s->_else_statements, {});
			}
		}
		else if(statement._for){
			const auto& s = statement._for;

			const auto start_value0 = evaluate_expression(vm2, s->_start_expression);
			vm2 = start_value0.first;
			const auto start_value = start_value0.second.get_literal().get_int();

			const auto end_value0 = evaluate_expression(vm2, s->_end_expression);
			vm2 = end_value0.first;
			const auto end_value = end_value0.second.get_literal().get_int();

			for(int x = start_value ; x <= end_value ; x++){
				const std::map<std::string, floyd_ast::value_t> values = { { s->_iterator_name, value_t(x)} };
				const auto result = execute_statements_in_env(vm2, s->_body, values);
				vm2 = result.first;
				const auto return_value = result.second;
				if(return_value != nullptr){
					return { vm2, return_value };
				}
			}

			return { vm2, {} };
		}
		else{
			QUARK_ASSERT(false);
		}
	}

	/*
		Return value:
			null = statements were all executed through.
			value = return statement returned a value.
	*/
	std::pair<interpreter_t, shared_ptr<value_t>> execute_statements(const interpreter_t& vm, const vector<shared_ptr<statement_t>>& statements){
		QUARK_ASSERT(vm.check_invariant());
		for(const auto i: statements){ QUARK_ASSERT(i->check_invariant()); };

		auto vm2 = vm;

		int statement_index = 0;
		while(statement_index < statements.size()){
			const auto statement = statements[statement_index];
			const auto& r = execute_statement(vm2, *statement);
			vm2 = r.first;
			if(r.second){
				return { vm2, r.second };
			}
			statement_index++;
		}
		return { vm2, {}};
	}


}	//	unnamed


value_t make_default_value(const interpreter_t& vm, const floyd_basics::typeid_t& t){
	QUARK_ASSERT(vm.check_invariant());
	QUARK_ASSERT(t.check_invariant());

	const auto type = t.get_base_type();
	if(type == floyd_basics::base_type::k_bool){
		return value_t(false);
	}
	else if(type == floyd_basics::base_type::k_int){
		return value_t(0);
	}
	else if(type == floyd_basics::base_type::k_float){
		return value_t(0.0f);
	}
	else if(type == floyd_basics::base_type::k_string){
		return value_t("");
	}
	else if(type == floyd_basics::base_type::k_struct){
		return make_struct_instance(vm, t);
	}
	else if(type == floyd_basics::base_type::k_vector){
		QUARK_ASSERT(false);
	}
	else if(type == floyd_basics::base_type::k_function){
		QUARK_ASSERT(false);
	}
	else{
		QUARK_ASSERT(false);
	}
}

value_t make_struct_instance(const interpreter_t& vm, const floyd_basics::typeid_t& struct_type){
	QUARK_ASSERT(vm.check_invariant());
	QUARK_ASSERT(!struct_type.is_null() && struct_type.check_invariant());

	return value_t();
/*
	const auto k = struct_type.to_string();
	const auto& objects = vm._ast.get_objects();
	const auto struct_def_it = objects.find(k);
	if(struct_def_it == objects.end()){
		throw std::runtime_error("Undefined struct type!");
	}

	const auto struct_def = struct_def_it->second;

	std::map<std::string, value_t> member_values;
	for(int i = 0 ; i < struct_def->_members.size() ; i++){
		const auto& member_def = struct_def->_members[i];

		const auto member_type = member_def._type;
		if(!member_type.is_null()){
			throw std::runtime_error("Undefined struct type!");
		}

		//	If there is an initial value for this member, use that. Else use default value for this type.
		value_t value;
		if(member_def._value){
			value = *member_def._value;
		}
		else{
			value = make_default_value(vm, member_def._type);
		}
		member_values[member_def._name] = value;
	}
	auto instance = make_shared<struct_instance_t>(struct_instance_t(struct_type, member_values));
	return value_t(instance);
*/
}

//	const auto it = find_if(s._state.begin(), s._state.end(), [&] (const member_t& e) { return e._name == name; } );

floyd_ast::value_t resolve_env_variable_deep(const interpreter_t& vm, const shared_ptr<environment_t>& env, const std::string& s){
	QUARK_ASSERT(vm.check_invariant());
	QUARK_ASSERT(env && env->check_invariant());
	QUARK_ASSERT(s.size() > 0);

	const auto it = env->_values.find(s);
	if(it != env->_values.end()){
		return it->second;
	}
	else if(env->_parent_env){
		return resolve_env_variable_deep(vm, env->_parent_env, s);
	}
	else{
		throw std::runtime_error("Undefined variable \"" + s + "\"!");
	}
}

floyd_ast::value_t resolve_env_variable(const interpreter_t& vm, const std::string& s){
	QUARK_ASSERT(vm.check_invariant());
	QUARK_ASSERT(s.size() > 0);

	return resolve_env_variable_deep(vm, vm._call_stack.back(), s);
}

value_t find_global_symbol(const interpreter_t& vm, const string& s){
	const auto v = resolve_env_variable(vm, s);
	if(v.is_null()){
		throw std::runtime_error("Undefined function!");
	}
	return v;
}

std::pair<interpreter_t, expression_t> evaluate_expression(const interpreter_t& vm, const expression_t& e){
	QUARK_ASSERT(vm.check_invariant());
	QUARK_ASSERT(e.check_invariant());

	auto vm2 = vm;

	if(e.is_literal()){
		return {vm2, e};
	}

	const auto op = e.get_operation();

	if(op == floyd_basics::expression_type::k_resolve_member){
		const auto expr = e.get_resolve_member();
		const auto parent_expr = evaluate_expression(vm2, *expr->_parent_address);
		if(parent_expr.second.is_literal() && parent_expr.second.get_literal().is_struct()){
			vm2 = parent_expr.first;
			const auto struct_instance = parent_expr.second.get_literal().get_struct();
			const value_t value = struct_instance->_member_values[expr->_member_name];
			return { vm2, expression_t::make_literal(value)};
		}
		else{
			throw std::runtime_error("Resolve member failed.");
		}
	}
	else if(op == floyd_basics::expression_type::k_variable){
		const auto expr = e.get_variable();
		const value_t value = resolve_env_variable(vm2, expr->_variable);
		return {vm2, expression_t::make_literal(value)};
	}

	else if(op == floyd_basics::expression_type::k_call){
		return evaluate_call_expression(vm2, e);
	}
	else if(op == floyd_basics::expression_type::k_define_function){
		const auto expr = e.get_function_definition();
		return {vm2, expression_t::make_literal(make_function_value(expr->_def))};
	}

	//	This can be desugared at compile time.
	else if(op == floyd_basics::expression_type::k_arithmetic_unary_minus__1){
		const auto expr = e.get_unary_minus();
		const auto& expr2 = evaluate_expression(vm2, *expr->_expr);
		vm2 = expr2.first;

		if(expr2.second.is_literal()){
			const auto& c = expr2.second.get_literal();
			vm2 = expr2.first;

			if(c.is_int()){
				return evaluate_expression(
					vm2,
					expression_t::make_simple_expression__2(floyd_basics::expression_type::k_arithmetic_subtract__2, expression_t::make_literal_int(0), expr2.second)
				);
			}
			else if(c.is_float()){
				return evaluate_expression(
					vm2,
					expression_t::make_simple_expression__2(floyd_basics::expression_type::k_arithmetic_subtract__2, expression_t::make_literal_float(0.0f), expr2.second)
				);
			}
			else{
				throw std::runtime_error("Unary minus won't work on expressions of type \"" + json_to_compact_string(typeid_to_json(c.get_type())) + "\".");
			}
		}
		else{
			throw std::runtime_error("Could not evaluate condition in conditional expression.");
		}
	}

	//	Special-case since it uses 3 expressions & uses shortcut evaluation.
	else if(op == floyd_basics::expression_type::k_conditional_operator3){
		const auto expr = e.get_conditional();
		const auto cond_result = evaluate_expression(vm2, *expr->_condition);
		vm2 = cond_result.first;

		if(cond_result.second.is_literal() && cond_result.second.get_literal().is_bool()){
			const bool cond_flag = cond_result.second.get_literal().get_bool();

			//	!!! Only evaluate the CHOSEN expression. Not that importan since functions are pure.
			if(cond_flag){
				return evaluate_expression(vm2, *expr->_a);
			}
			else{
				return evaluate_expression(vm2, *expr->_b);
			}
		}
		else{
			throw std::runtime_error("Could not evaluate condition in conditional expression.");
		}
	}

	//	First evaluate all inputs to our operation.
	const auto simple2_expr = e.get_simple__2();
	const auto left_expr = evaluate_expression(vm2, *simple2_expr->_left);
	vm2 = left_expr.first;
	const auto right_expr = evaluate_expression(vm2, *simple2_expr->_right);
	vm2 = right_expr.first;

	//	Both left and right are constants, replace the math_operation with a constant!
	if(left_expr.second.is_literal() && right_expr.second.is_literal()){
		//	Perform math operation on the two constants => new constant.
		const auto left_constant = left_expr.second.get_literal();
		const auto right_constant = right_expr.second.get_literal();

		//	Is operation supported by all types?
		{
			if(op == floyd_basics::expression_type::k_comparison_smaller_or_equal__2){
				long diff = value_t::compare_value_true_deep(left_constant, right_constant);
				return {vm2, expression_t::make_literal_bool(diff <= 0)};
			}
			else if(op == floyd_basics::expression_type::k_comparison_smaller__2){
				long diff = value_t::compare_value_true_deep(left_constant, right_constant);
				return {vm2, expression_t::make_literal_bool(diff < 0)};
			}
			else if(op == floyd_basics::expression_type::k_comparison_larger_or_equal__2){
				long diff = value_t::compare_value_true_deep(left_constant, right_constant);
				return {vm2, expression_t::make_literal_bool(diff >= 0)};
			}
			else if(op == floyd_basics::expression_type::k_comparison_larger__2){
				long diff = value_t::compare_value_true_deep(left_constant, right_constant);
				return {vm2, expression_t::make_literal_bool(diff > 0)};
			}


			else if(op == floyd_basics::expression_type::k_logical_equal__2){
				long diff = value_t::compare_value_true_deep(left_constant, right_constant);
				return {vm2, expression_t::make_literal_bool(diff == 0)};
			}
			else if(op == floyd_basics::expression_type::k_logical_nonequal__2){
				long diff = value_t::compare_value_true_deep(left_constant, right_constant);
				return {vm2, expression_t::make_literal_bool(diff != 0)};
			}
		}

		//	bool
		if(left_constant.is_bool() && right_constant.is_bool()){
			const bool left = left_constant.get_bool();
			const bool right = right_constant.get_bool();

			if(op == floyd_basics::expression_type::k_arithmetic_add__2
			|| op == floyd_basics::expression_type::k_arithmetic_subtract__2
			|| op == floyd_basics::expression_type::k_arithmetic_multiply__2
			|| op == floyd_basics::expression_type::k_arithmetic_divide__2
			|| op == floyd_basics::expression_type::k_arithmetic_remainder__2
			){
				throw std::runtime_error("Arithmetics on bool not allowed.");
			}

			else if(op == floyd_basics::expression_type::k_logical_and__2){
				return {vm2, expression_t::make_literal_bool(left && right)};
			}
			else if(op == floyd_basics::expression_type::k_logical_or__2){
				return {vm2, expression_t::make_literal_bool(left || right)};
			}
			else{
				QUARK_ASSERT(false);
			}
		}

		//	int
		else if(left_constant.is_int() && right_constant.is_int()){
			const int left = left_constant.get_int();
			const int right = right_constant.get_int();

			if(op == floyd_basics::expression_type::k_arithmetic_add__2){
				return {vm2, expression_t::make_literal_int(left + right)};
			}
			else if(op == floyd_basics::expression_type::k_arithmetic_subtract__2){
				return {vm2, expression_t::make_literal_int(left - right)};
			}
			else if(op == floyd_basics::expression_type::k_arithmetic_multiply__2){
				return {vm2, expression_t::make_literal_int(left * right)};
			}
			else if(op == floyd_basics::expression_type::k_arithmetic_divide__2){
				if(right == 0){
					throw std::runtime_error("EEE_DIVIDE_BY_ZERO");
				}
				return {vm2, expression_t::make_literal_int(left / right)};
			}
			else if(op == floyd_basics::expression_type::k_arithmetic_remainder__2){
				if(right == 0){
					throw std::runtime_error("EEE_DIVIDE_BY_ZERO");
				}
				return {vm2, expression_t::make_literal_int(left % right)};
			}

			else if(op == floyd_basics::expression_type::k_logical_and__2){
				return {vm2, expression_t::make_literal_bool((left != 0) && (right != 0))};
			}
			else if(op == floyd_basics::expression_type::k_logical_or__2){
				return {vm2, expression_t::make_literal_bool((left != 0) || (right != 0))};
			}
			else{
				QUARK_ASSERT(false);
			}
		}

		//	float
		else if(left_constant.is_float() && right_constant.is_float()){
			const float left = left_constant.get_float();
			const float right = right_constant.get_float();

			if(op == floyd_basics::expression_type::k_arithmetic_add__2){
				return {vm2, expression_t::make_literal_float(left + right)};
			}
			else if(op == floyd_basics::expression_type::k_arithmetic_subtract__2){
				return {vm2, expression_t::make_literal_float(left - right)};
			}
			else if(op == floyd_basics::expression_type::k_arithmetic_multiply__2){
				return {vm2, expression_t::make_literal_float(left * right)};
			}
			else if(op == floyd_basics::expression_type::k_arithmetic_divide__2){
				if(right == 0.0f){
					throw std::runtime_error("EEE_DIVIDE_BY_ZERO");
				}
				return {vm2, expression_t::make_literal_float(left / right)};
			}
			else if(op == floyd_basics::expression_type::k_arithmetic_remainder__2){
				throw std::runtime_error("Modulo operation on float not allowed.");
			}


			else if(op == floyd_basics::expression_type::k_logical_and__2){
				return {vm2, expression_t::make_literal_bool((left != 0.0f) && (right != 0.0f))};
			}
			else if(op == floyd_basics::expression_type::k_logical_or__2){
				return {vm2, expression_t::make_literal_bool((left != 0.0f) || (right != 0.0f))};
			}
			else{
				QUARK_ASSERT(false);
			}
		}

		//	string
		else if(left_constant.is_string() && right_constant.is_string()){
			const string left = left_constant.get_string();
			const string right = right_constant.get_string();

			if(op == floyd_basics::expression_type::k_arithmetic_add__2){
				return {vm2, expression_t::make_literal_string(left + right)};
			}

			else if(op == floyd_basics::expression_type::k_arithmetic_subtract__2){
				throw std::runtime_error("Operation not allowed on string.");
			}
			else if(op == floyd_basics::expression_type::k_arithmetic_multiply__2){
				throw std::runtime_error("Operation not allowed on string.");
			}
			else if(op == floyd_basics::expression_type::k_arithmetic_divide__2){
				throw std::runtime_error("Operation not allowed on string.");
			}
			else if(op == floyd_basics::expression_type::k_arithmetic_remainder__2){
				throw std::runtime_error("Operation not allowed on string.");
			}


			else if(op == floyd_basics::expression_type::k_logical_and__2){
				throw std::runtime_error("Operation not allowed on string.");
			}
			else if(op == floyd_basics::expression_type::k_logical_or__2){
				throw std::runtime_error("Operation not allowed on string.");
			}
			else{
				QUARK_ASSERT(false);
			}
		}
		//	struct
		else if(left_constant.is_struct() && right_constant.is_struct()){
			const auto left = left_constant.get_struct();
			const auto right = right_constant.get_struct();

			if(!(left->_struct_type == right->_struct_type)){
				throw std::runtime_error("Struct type mismatch.");
			}

			if(op == floyd_basics::expression_type::k_arithmetic_add__2){
				//??? allow
				throw std::runtime_error("Operation not allowed on struct.");
			}
			else if(op == floyd_basics::expression_type::k_arithmetic_subtract__2){
				throw std::runtime_error("Operation not allowed on struct.");
			}
			else if(op == floyd_basics::expression_type::k_arithmetic_multiply__2){
				throw std::runtime_error("Operation not allowed on struct.");
			}
			else if(op == floyd_basics::expression_type::k_arithmetic_divide__2){
				throw std::runtime_error("Operation not allowed on struct.");
			}
			else if(op == floyd_basics::expression_type::k_arithmetic_remainder__2){
				throw std::runtime_error("Operation not allowed on struct.");
			}

			else if(op == floyd_basics::expression_type::k_logical_and__2){
				throw std::runtime_error("Operation not allowed on struct.");
			}
			else if(op == floyd_basics::expression_type::k_logical_or__2){
				throw std::runtime_error("Operation not allowed on struct.");
			}
			else{
				QUARK_ASSERT(false);
			}
		}

		else if(left_constant.is_vector() && right_constant.is_vector()){
			QUARK_ASSERT(false);
		}
		else{
			throw std::runtime_error("Arithmetics failed.");
		}
	}

	//	Else use a math_operation expression to perform the calculation later.
	//	We make a NEW math_operation since sub-nodes may have been evaluated.
	else{
		return {vm2, expression_t::make_simple_expression__2(op, left_expr.second, right_expr.second)};
	}
}


std::pair<interpreter_t, std::shared_ptr<value_t>> call_function(const interpreter_t& vm, const floyd_ast::value_t& f, const vector<value_t>& args){
	QUARK_ASSERT(vm.check_invariant());
	QUARK_ASSERT(f.check_invariant());
	for(const auto i: args){ QUARK_ASSERT(i.check_invariant()); };

	auto vm2 = vm;

	///??? COMPARE arg count and types.

	if(f.is_function() == false){
		throw std::runtime_error("Cannot call non-function.");
	}

	const auto& function_def = f.get_function()->_def;
	if(function_def._host_function != 0){
		const auto r = vm2.call_host_function(function_def._host_function, args);
		return { r.first, make_shared<value_t>(r.second) };
	}
	else{

		//	Always use global scope.
		//	Future: support closures by linking to function env where function is defined.
		auto parent_env = vm2._call_stack[0];
		auto new_environment = environment_t::make_environment(vm2, parent_env);

		//	Copy input arguments to the function scope.
		for(int i = 0 ; i < function_def._args.size() ; i++){
			const auto& arg_name = function_def._args[i]._name;
			const auto& arg_value = args[i];
			new_environment->_values[arg_name] = arg_value;
		}
		vm2._call_stack.push_back(new_environment);

//		QUARK_TRACE(json_to_pretty_string(interpreter_to_json(vm2)));

		const auto r = execute_statements(vm2, function_def._statements);

		vm2 = r.first;
		vm2._call_stack.pop_back();

		if(!r.second){
			throw std::runtime_error("function missing return statement");
		}
		else{
			return {vm2, r.second };
		}
	}
}

bool all_literals(const vector<expression_t>& e){
	for(const auto& i: e){
		if(!i.is_literal()){
			return false;
		}
	}
	return true;
}

std::pair<interpreter_t, expression_t> evaluate_call_expression(const interpreter_t& vm, const expression_t& e){
	QUARK_ASSERT(vm.check_invariant());
	QUARK_ASSERT(e.check_invariant());
	QUARK_ASSERT(e.get_operation() == floyd_basics::expression_type::k_call);

	auto vm2 = vm;
	const auto call = e.get_function_call();
	QUARK_ASSERT(call);

	//	Simplify each input expression: expression[0]: which function to call, expression[1]: first argument if any.
	const auto function = evaluate_expression(vm2, *call->_function);
	vm2 = function.first;
	vector<expression_t> args2;
	for(int i = 0 ; i < call->_args.size() ; i++){
		const auto t = evaluate_expression(vm2, call->_args[i]);
		vm2 = t.first;
		args2.push_back(t.second);
	}

	//	If not all input expressions could be evaluated, return a (maybe simplified) expression.
	if(function.second.is_literal() == false || all_literals(args2) == false){
		return {vm2, expression_t::make_function_call(function.second, args2, e.get_result_type())};
	}

	//	Get function value and arg values.
	const auto& function_value = function.second.get_literal();
	if(function_value.is_function() == false){
		throw std::runtime_error("Cannot call non-function.");
	}

	vector<value_t> args3;
	for(const auto& i: args2){
		args3.push_back(i.get_literal());
	}

	const auto& result = call_function(vm2, function_value, args3);
	vm2 = result.first;
	return { vm2, expression_t::make_literal(*result.second)};
}



json_t interpreter_to_json(const interpreter_t& vm){
	vector<json_t> callstack;
	for(const auto& e: vm._call_stack){
		std::map<string, json_t> values;
		for(const auto&v: e->_values){
			values[v.first] = value_to_json(v.second);
		}

		const auto& env = json_t::make_object({
//			{ "parent_env", e->_parent_env ? e->_parent_env->_object_id : json_t() },
//			{ "object_id", json_t(double(e->_object_id)) },
			{ "values", values }
		});
		callstack.push_back(env);
	}

	return json_t::make_object({
		{ "ast", ast_to_json(vm._ast) },
		{ "callstack", json_t::make_array(callstack) }
	});
}



void test__evaluate_expression(const expression_t& e, const expression_t& expected){
	const ast_t ast;
	const interpreter_t interpreter(ast);
	const auto e3 = evaluate_expression(interpreter, e);

	ut_compare_jsons(expression_to_json(e3.second), expression_to_json(expected));
}


QUARK_UNIT_TESTQ("evaluate_expression()", "literal 1234 == 1234") {
	test__evaluate_expression(
		expression_t::make_literal_int(1234),
		expression_t::make_literal_int(1234)
	);
}

QUARK_UNIT_TESTQ("evaluate_expression()", "1 + 2 == 3") {
	test__evaluate_expression(
		expression_t::make_simple_expression__2(
			floyd_basics::expression_type::k_arithmetic_add__2,
			expression_t::make_literal_int(1),
			expression_t::make_literal_int(2)
		),
		expression_t::make_literal_int(3)
	);
}


QUARK_UNIT_TESTQ("evaluate_expression()", "3 * 4 == 12") {
	test__evaluate_expression(
		expression_t::make_simple_expression__2(
			floyd_basics::expression_type::k_arithmetic_multiply__2,
			expression_t::make_literal_int(3),
			expression_t::make_literal_int(4)
		),
		expression_t::make_literal_int(12)
	);
}

QUARK_UNIT_TESTQ("evaluate_expression()", "(3 * 4) * 5 == 60") {
	test__evaluate_expression(
		expression_t::make_simple_expression__2(
			floyd_basics::expression_type::k_arithmetic_multiply__2,
			expression_t::make_simple_expression__2(
				floyd_basics::expression_type::k_arithmetic_multiply__2,
				expression_t::make_literal_int(3),
				expression_t::make_literal_int(4)
			),
			expression_t::make_literal_int(5)
		),
		expression_t::make_literal_int(60)
	);
}




//////////////////////////		environment_t



std::shared_ptr<environment_t> environment_t::make_environment(
	const interpreter_t& vm,
	std::shared_ptr<floyd_interpreter::environment_t>& parent_env
)
{
	QUARK_ASSERT(vm.check_invariant());

	auto f = environment_t{ parent_env, {} };
	return make_shared<environment_t>(f);
}

bool environment_t::check_invariant() const {
	return true;
}



//////////////////////////		interpreter_t



enum host_functions {
	k_print = 1,
	k_to_string,
	k_get_time_of_day_ms
};

//	Records all output to interpreter
std::pair<interpreter_t, value_t> host__print(const interpreter_t& vm, const std::vector<value_t>& args){
	auto vm2 = vm;
	const auto& value = args[0];
	const auto s = value.plain_value_to_string();
	printf("%s\n", s.c_str());

	vm2._print_output.push_back(s);
	return {vm2, value_t() };
}

//	string to_string(value_t)
std::pair<interpreter_t, value_t> host__to_string(const interpreter_t& vm, const std::vector<value_t>& args){
	const auto& value = args[0];
	const auto a = value.plain_value_to_string();
	return {vm, value_t(a) };
}



/*
    std::chrono::time_point<std::chrono::system_clock> start, end;
    start = std::chrono::system_clock::now();
    std::cout << "f(42) = " << fibonacci(42) << '\n';
    end = std::chrono::system_clock::now();
 
    std::chrono::duration<double> elapsed_seconds = end-start;
    std::time_t end_time = std::chrono::system_clock::to_time_t(end);
 
    std::cout << "finished computation at " << std::ctime(&end_time)
              << "elapsed time: " << elapsed_seconds.count() << "s\n";
*/



#if 0
	uint64_t get_time_of_day_ms(){
		std::chrono::time_point<std::chrono::high_resolution_clock> t = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed_seconds = t - start;
		std::time_t sec = std::chrono::high_resolution_clock::to_time_t(t);
		return sec * 1000.0;
/*
		timeval time;
		gettimeofday(&time, NULL);


	//	QUARK_ASSERT(sizeof(int) == sizeof(int64_t));
		int64_t ms = (time.tv_sec * 1000) + (time.tv_usec / 1000);
		return ms;
*/
	}
#endif

std::pair<interpreter_t, value_t> host__get_time_of_day(const interpreter_t& vm, const std::vector<value_t>& args){
	std::chrono::time_point<std::chrono::high_resolution_clock> t = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed_seconds = t - vm._start_time;
	const auto ms = elapsed_seconds.count() * 1000.0;
	return {vm, value_t(int(ms)) };
}

QUARK_UNIT_TESTQ("sizeof(int)", ""){
	QUARK_TRACE(std::to_string(sizeof(int)));
	QUARK_TRACE(std::to_string(sizeof(int64_t)));
}

QUARK_UNIT_TESTQ("get_time_of_day_ms()", ""){
	const auto a = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(7));
	const auto b = std::chrono::high_resolution_clock::now();

	std::chrono::duration<double> elapsed_seconds = b - a;
	const int ms = static_cast<int>((static_cast<double>(elapsed_seconds.count()) * 1000.0));

	QUARK_UT_VERIFY(ms >= 7)
}







std::pair<interpreter_t, floyd_ast::value_t> interpreter_t::call_host_function(int function_id, const std::vector<floyd_ast::value_t> args) const{
	if(function_id == static_cast<int>(host_functions::k_print)){
		return host__print(*this, args);
	}
	else if(function_id == static_cast<int>(host_functions::k_to_string)){
		return host__to_string(*this, args);
	}
	else if(function_id == static_cast<int>(host_functions::k_get_time_of_day_ms)){
		return host__get_time_of_day(*this, args);
	}
	else{
		QUARK_ASSERT(false);
	}
}


interpreter_t::interpreter_t(const ast_t& ast){
	QUARK_ASSERT(ast.check_invariant());

	std::vector<std::shared_ptr<statement_t>> init_statements;

	//	Insert built-in functions into AST.

	init_statements.push_back(make_shared<statement_t>(make_function_statement(
		"print",
		function_definition_t(
			{ member_t{ floyd_basics::typeid_t::make_string(), "s" } },
			host_functions::k_print,
			floyd_basics::typeid_t::make_null()
		)
	)));

	init_statements.push_back(make_shared<statement_t>(make_function_statement(
		"to_string",
		function_definition_t(
			//??? Supports arg of any type.
			{ },
			host_functions::k_to_string,
			floyd_basics::typeid_t::make_string()
		)
	)));

	init_statements.push_back(make_shared<statement_t>(make_function_statement(
		"get_time_of_day",
		function_definition_t(
			{},
			host_functions::k_get_time_of_day_ms,
			floyd_basics::typeid_t::make_int()
		)
	)));


	_ast = ast_t(init_statements + ast._statements);


	//	Make the top-level environment = global scope.
	shared_ptr<environment_t> empty_env;
	auto global_env = environment_t::make_environment(*this, empty_env);

	_call_stack.push_back(global_env);

	_start_time = std::chrono::high_resolution_clock::now();

	//	Run static intialization (basically run global statements before calling main()).
	const auto r = execute_statements(*this, _ast._statements);
	if(r.second){
		throw std::runtime_error("Return statement illegal in global scope.");
	}

	_call_stack[0]->_values = r.first._call_stack[0]->_values;
	_print_output = r.first._print_output;
	QUARK_ASSERT(check_invariant());
}

interpreter_t::interpreter_t(const interpreter_t& other) :
	_start_time(other._start_time),
	_ast(other._ast),
	_call_stack(other._call_stack),
	_print_output(other._print_output)
{
	QUARK_ASSERT(other.check_invariant());
	QUARK_ASSERT(check_invariant());
}


	//??? make proper operator=(). Exception safety etc.
const interpreter_t& interpreter_t::operator=(const interpreter_t& other){
	_start_time = other._start_time;
	_ast = other._ast;
	_call_stack = other._call_stack;
	_print_output = other._print_output;
	return *this;
}


bool interpreter_t::check_invariant() const {
	QUARK_ASSERT(_ast.check_invariant());
	return true;
}





//////////////////////////		run_main()


ast_t program_to_ast2(const string& program){
	const auto pass1 = floyd_parser::parse_program2(program);
	const auto pass2 = run_pass2(pass1);
	trace(pass2);
	return pass2;
}


interpreter_t run_global(const string& source){
	auto ast = program_to_ast2(source);
	auto vm = interpreter_t(ast);

	QUARK_TRACE(json_to_pretty_string(interpreter_to_json(vm)));
	return vm;
}

std::pair<interpreter_t, floyd_ast::value_t> run_main(const string& source, const vector<floyd_ast::value_t>& args){
	auto ast = program_to_ast2(source);
	auto vm = interpreter_t(ast);
	const auto f = find_global_symbol(vm, "main");
	const auto r = call_function(vm, f, args);
	return { r.first, *r.second };
}





///////////////////////////////////////////////////////////////////////////////////////
//	FLOYD LANGUAGE TEST SUITE
///////////////////////////////////////////////////////////////////////////////////////



void test__run_init__check_result(const std::string& program, const value_t& expected_result){
	const auto result = run_global(program);
	const auto result_value = result._call_stack[0]->_values["result"];
	ut_compare_jsons(
		expression_to_json(expression_t::make_literal(result_value)),
		expression_to_json(expression_t::make_literal(expected_result))
	);
}
void test__run_init2(const std::string& program){
	const auto result = run_global(program);
}


void test__run_main(const std::string& program, const vector<floyd_ast::value_t>& args, const value_t& expected_return){
	const auto result = run_main(program, args);
	ut_compare_jsons(
		expression_to_json(expression_t::make_literal(result.second)),
		expression_to_json(expression_t::make_literal(expected_return))
	);
}


//??? test all errors too!

//////////////////////////		TEST GLOBAL CONSTANTS


QUARK_UNIT_TESTQ("Floyd test suite", "Global int variable"){
	test__run_init__check_result("int result = 123;", value_t(123));
}


//////////////////////////		TEST CONSTANT EXPRESSIONS

/*
QUARK_UNIT_TESTQ("Floyd test suite", "null constant expression"){
	test__run_init__check_result("int result = null;", value_t());
}
*/

QUARK_UNIT_TESTQ("Floyd test suite", "bool constant expression"){
	test__run_init__check_result("bool result = true;", value_t(true));
}
QUARK_UNIT_TESTQ("Floyd test suite", "bool constant expression"){
	test__run_init__check_result("bool result = false;", value_t(false));
}

QUARK_UNIT_TESTQ("Floyd test suite", "int constant expression"){
	test__run_init__check_result("int result = 123;", value_t(123));
}

QUARK_UNIT_TESTQ("Floyd test suite", "float constant expression"){
	test__run_init__check_result("float result = 3.5;", value_t(float(3.5)));
}

QUARK_UNIT_TESTQ("Floyd test suite", "string constant expression"){
	test__run_init__check_result("string result = \"xyz\";", value_t("xyz"));
}

//??? struct
//??? vector
//??? function







//////////////////////////		TEST EXPRESSIONS


QUARK_UNIT_TESTQ("Floyd test suite", "+") {
	test__run_init__check_result("int result = 1 + 2;", value_t(3));
}
QUARK_UNIT_TESTQ("Floyd test suite", "+") {
	test__run_init__check_result("int result = 1 + 2 + 3;", value_t(6));
}
QUARK_UNIT_TESTQ("Floyd test suite", "*") {
	test__run_init__check_result("int result = 3 * 4;", value_t(12));
}

QUARK_UNIT_TESTQ("Floyd test suite", "parant") {
	test__run_init__check_result("int result = (3 * 4) * 5;", value_t(60));
}





//////////////////////////		TEST conditional expression


QUARK_UNIT_TESTQ("run_main()", "conditional expression"){
	test__run_init__check_result("int result = true ? 1 : 2;", value_t(1));
}
QUARK_UNIT_TESTQ("run_main()", "conditional expression"){
	test__run_init__check_result("int result = false ? 1 : 2;", value_t(2));
}

//??? Test truthness off all variable types: strings, floats

QUARK_UNIT_TESTQ("run_main()", "conditional expression"){
	test__run_init__check_result("string result = true ? \"yes\" : \"no\";", value_t("yes"));
}
QUARK_UNIT_TESTQ("run_main()", "conditional expression"){
	test__run_init__check_result("string result = false ? \"yes\" : \"no\";", value_t("no"));
}





QUARK_UNIT_TESTQ("evaluate_expression()", "Parenthesis") {
	test__run_init__check_result("int result = 5*(4+4+1);", value_t(45));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "Parenthesis") {
	test__run_init__check_result("int result = 5*(2*(1+3)+1);", value_t(45));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "Parenthesis") {
	test__run_init__check_result("int result = 5*((1+3)*2+1);", value_t(45));
}



QUARK_UNIT_TESTQ("evaluate_expression()", "Spaces") {
	test__run_init__check_result("int result = 5 * ((1 + 3) * 2 + 1) ;", value_t(45));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "Spaces") {
	test__run_init__check_result("int result = 5 - 2 * ( 3 ) ;", value_t(-1));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "Spaces") {
	test__run_init__check_result("int result =  5 - 2 * ( ( 4 )  - 1 ) ;", value_t(-1));
}

QUARK_UNIT_TESTQ("evaluate_expression()", "Sign before parenthesis") {
	test__run_init__check_result("int result = -(2+1)*4;", value_t(-12));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "Sign before parenthesis") {
	test__run_init__check_result("int result = -4*(2+1);", value_t(-12));
}


QUARK_UNIT_TESTQ("evaluate_expression()", "Fractional numbers") {
	test__run_init__check_result("float result = 5.5/5.0;", value_t(1.1f));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "Fractional numbers") {
//	test__run_init__check_result("int result = 1/5e10") == 2e-11);
}
QUARK_UNIT_TESTQ("evaluate_expression()", "Fractional numbers") {
	test__run_init__check_result("float result = (4.0-3.0)/(4.0*4.0);", value_t(0.0625f));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "Fractional numbers") {
	test__run_init__check_result("float result = 1.0/2.0/2.0;", value_t(0.25f));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "Fractional numbers") {
	test__run_init__check_result("float result = 0.25 * .5 * 0.5;", value_t(0.0625f));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "Fractional numbers") {
	test__run_init__check_result("float result = .25 / 2.0 * .5;", value_t(0.0625f));
}

QUARK_UNIT_TESTQ("evaluate_expression()", "Repeated operators") {
	test__run_init__check_result("int result = 1+-2;", value_t(-1));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "Repeated operators") {
	test__run_init__check_result("int result = --2;", value_t(2));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "Repeated operators") {
	test__run_init__check_result("int result = 2---2;", value_t(0));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "Repeated operators") {
	test__run_init__check_result("int result = 2-+-2;", value_t(4));
}


QUARK_UNIT_TESTQ("evaluate_expression()", "Bool") {
	test__run_init__check_result("bool result = true;", value_t(true));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "Bool") {
	test__run_init__check_result("bool result = false;", value_t(false));
}


//??? add tests  for pass2()
QUARK_UNIT_TESTQ("evaluate_expression()", "?:") {
	test__run_init__check_result("int result = true ? 4 : 6;", value_t(4));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "?:") {
	test__run_init__check_result("int result = false ? 4 : 6;", value_t(6));
}

QUARK_UNIT_TESTQ("evaluate_expression()", "?:") {
	test__run_init__check_result("int result = 1==3 ? 4 : 6;", value_t(6));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "?:") {
	test__run_init__check_result("int result = 3==3 ? 4 : 6;", value_t(4));
}

QUARK_UNIT_TESTQ("evaluate_expression()", "?:") {
	test__run_init__check_result("int result = 3==3 ? 2 + 2 : 2 * 3;", value_t(4));
}

QUARK_UNIT_TESTQ("evaluate_expression()", "?:") {
	test__run_init__check_result("int result = 3==1+2 ? 2 + 2 : 2 * 3;", value_t(4));
}




QUARK_UNIT_TESTQ("evaluate_expression()", "==") {
	test__run_init__check_result("bool result = 1 == 1;", value_t(true));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "==") {
	test__run_init__check_result("bool result = 1 == 2;", value_t(false));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "==") {
	test__run_init__check_result("bool result = 1.3 == 1.3;", value_t(true));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "==") {
	test__run_init__check_result("bool result = \"hello\" == \"hello\";", value_t(true));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "==") {
	test__run_init__check_result("bool result = \"hello\" == \"bye\";", value_t(false));
}


QUARK_UNIT_TESTQ("evaluate_expression()", "<") {
	test__run_init__check_result("bool result = 1 < 2;", value_t(true));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "<") {
	test__run_init__check_result("bool result = 5 < 2;", value_t(false));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "<") {
	test__run_init__check_result("bool result = 0.3 < 0.4;", value_t(true));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "<") {
	test__run_init__check_result("bool result = 1.5 < 0.4;", value_t(false));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "<") {
	test__run_init__check_result("bool result = \"adwark\" < \"boat\";", value_t(true));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "<") {
	test__run_init__check_result("bool result = \"boat\" < \"adwark\";", value_t(false));
}


QUARK_UNIT_TESTQ("evaluate_expression()", "&&") {
	test__run_init__check_result("bool result = false && false;", value_t(false));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "&&") {
	test__run_init__check_result("bool result = false && true;", value_t(false));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "&&") {
	test__run_init__check_result("bool result = true && false;", value_t(false));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "&&") {
	test__run_init__check_result("bool result = true && true;", value_t(true));
}

QUARK_UNIT_TESTQ("evaluate_expression()", "&&") {
	test__run_init__check_result("bool result = false && false && false;", value_t(false));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "&&") {
	test__run_init__check_result("bool result = false && false && true;", value_t(false));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "&&") {
	test__run_init__check_result("bool result = false && true && false;", value_t(false));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "&&") {
	test__run_init__check_result("bool result = false && true && true;", value_t(false));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "&&") {
	test__run_init__check_result("bool result = true && false && false;", value_t(false));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "&&") {
	test__run_init__check_result("bool result = true && true && false;", value_t(false));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "&&") {
	test__run_init__check_result("bool result = true && true && true;", value_t(true));
}


QUARK_UNIT_TESTQ("evaluate_expression()", "||") {
	test__run_init__check_result("bool result = false || false;", value_t(false));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "||") {
	test__run_init__check_result("bool result = false || true;", value_t(true));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "||") {
	test__run_init__check_result("bool result = true || false;", value_t(true));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "||") {
	test__run_init__check_result("bool result = true || true;", value_t(true));
}


QUARK_UNIT_TESTQ("evaluate_expression()", "||") {
	test__run_init__check_result("bool result = false || false || false;", value_t(false));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "||") {
	test__run_init__check_result("bool result = false || false || true;", value_t(true));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "||") {
	test__run_init__check_result("bool result = false || true || false;", value_t(true));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "||") {
	test__run_init__check_result("bool result = false || true || true;", value_t(true));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "||") {
	test__run_init__check_result("bool result = true || false || false;", value_t(true));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "||") {
	test__run_init__check_result("bool result = true || false || true;", value_t(true));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "||") {
	test__run_init__check_result("bool result = true || true || false;", value_t(true));
}
QUARK_UNIT_TESTQ("evaluate_expression()", "||") {
	test__run_init__check_result("bool result = true || true || true;", value_t(true));
}

QUARK_UNIT_TESTQ("evaluate_expression()", "Type mismatch") {
	try{
		test__run_init__check_result("int result = true;", value_t(1));
		QUARK_TEST_VERIFY(false);
	}
	catch(const std::runtime_error& e){
		QUARK_TEST_VERIFY(string(e.what()) == "Types not compatible in bind.");
	}
}


QUARK_UNIT_TESTQ("evaluate_expression()", "Division by zero") {
	try{
		test__run_init__check_result("int result = 2/0;", value_t());
		QUARK_TEST_VERIFY(false);
	}
	catch(const std::runtime_error& e){
		QUARK_TEST_VERIFY(string(e.what()) == "EEE_DIVIDE_BY_ZERO");
	}
}

QUARK_UNIT_TESTQ("evaluate_expression()", "Division by zero"){
	try{
		test__run_init__check_result("int result = 3+1/(5-5)+4;", value_t());
		QUARK_TEST_VERIFY(false);
	}
	catch(const std::runtime_error& e){
		QUARK_TEST_VERIFY(string(e.what()) == "EEE_DIVIDE_BY_ZERO");
	}
}

#if false

QUARK_UNIT_TESTQ("evaluate_expression()", "Errors") {
		//	Multiple errors not possible/relevant now that we use exceptions for errors.
/*
	//////////////////////////		Only one error will be detected (in this case, the last one)
	QUARK_TEST_VERIFY(test__evaluate_expression("3+1/0+4$") == EEE_WRONG_CHAR);

	QUARK_TEST_VERIFY(test__evaluate_expression("3+1/0+4") == EEE_DIVIDE_BY_ZERO);

	// ...or the first one
	QUARK_TEST_VERIFY(test__evaluate_expression("q+1/0)") == EEE_WRONG_CHAR);
	QUARK_TEST_VERIFY(test__evaluate_expression("+1/0)") == EEE_PARENTHESIS);
	QUARK_TEST_VERIFY(test__evaluate_expression("+1/0") == EEE_DIVIDE_BY_ZERO);
*/
}

#endif








//////////////////////////		TEST FUNCTIONS



QUARK_UNIT_TESTQ("run_main", "Can make and read global int"){
	test__run_main(
		"int test = 123;"
		"string main(){\n"
		"	return test;"
		"}\n",
		{},
		value_t(123)
	);
}


QUARK_UNIT_TESTQ("run_main()", "minimal program 2"){
	test__run_main(
		"string main(string args){\n"
		"	return \"123\" + \"456\";\n"
		"}\n",
		vector<floyd_ast::value_t>{floyd_ast::value_t("program_name 1 2 3 4")},
		floyd_ast::value_t("123456")
	);
}

QUARK_UNIT_TESTQ("run_main()", ""){
	test__run_main("bool main(){ return 4 < 5; }", {}, floyd_ast::value_t(true));
}
QUARK_UNIT_TESTQ("run_main()", ""){
	test__run_main("bool main(){ return 5 < 4; }", {}, floyd_ast::value_t(false));
}
QUARK_UNIT_TESTQ("run_main()", ""){
	test__run_main("bool main(){ return 4 <= 4; }", {}, floyd_ast::value_t(true));
}

QUARK_UNIT_TESTQ("call_function()", "minimal program"){
	auto ast = program_to_ast2(
		"int main(string args){\n"
		"	return 3 + 4;\n"
		"}\n"
	);
	auto vm = interpreter_t(ast);
	const auto f = find_global_symbol(vm, "main");
	const auto result = call_function(vm, f, vector<floyd_ast::value_t>{ floyd_ast::value_t("program_name 1 2 3") });
	QUARK_TEST_VERIFY(*result.second == floyd_ast::value_t(7));
}


QUARK_UNIT_TESTQ("call_function()", "minimal program 2"){
	auto ast = program_to_ast2(
		"string main(string args){\n"
		"	return \"123\" + \"456\";\n"
		"}\n"
	);
	auto vm = interpreter_t(ast);
	const auto f = find_global_symbol(vm, "main");
	const auto result = call_function(vm, f, vector<floyd_ast::value_t>{ floyd_ast::value_t("program_name 1 2 3") });
	QUARK_TEST_VERIFY(*result.second == floyd_ast::value_t("123456"));
}

QUARK_UNIT_TESTQ("call_function()", "define additional function, call it several times"){
	auto ast = program_to_ast2(
		"int myfunc(){ return 5; }\n"
		"int main(string args){\n"
		"	return myfunc() + myfunc() * 2;\n"
		"}\n"
	);
	auto vm = interpreter_t(ast);
	const auto f = find_global_symbol(vm, "main");
	const auto result = call_function(vm, f, vector<floyd_ast::value_t>{ floyd_ast::value_t("program_name 1 2 3") });
	QUARK_TEST_VERIFY(*result.second == floyd_ast::value_t(15));
}

QUARK_UNIT_TESTQ("call_function()", "use function inputs"){
	auto ast = program_to_ast2(
		"string main(string args){\n"
		"	return \"-\" + args + \"-\";\n"
		"}\n"
	);
	auto vm = interpreter_t(ast);
	const auto f = find_global_symbol(vm, "main");
	const auto result = call_function(vm, f, vector<floyd_ast::value_t>{ floyd_ast::value_t("xyz") });
	QUARK_TEST_VERIFY(*result.second == floyd_ast::value_t("-xyz-"));

	const auto result2 = call_function(vm, f, vector<floyd_ast::value_t>{ floyd_ast::value_t("Hello, world!") });
	QUARK_TEST_VERIFY(*result2.second == floyd_ast::value_t("-Hello, world!-"));
}


QUARK_UNIT_TESTQ("call_function()", "use local variables"){
	auto ast = program_to_ast2(
		"string myfunc(string t){ return \"<\" + t + \">\"; }\n"
		"string main(string args){\n"
		"	 string a = \"--\"; string b = myfunc(args) ; return a + args + b + a;\n"
		"}\n"
	);
	auto vm = interpreter_t(ast);
	const auto f = find_global_symbol(vm, "main");
	const auto result = call_function(vm, f, vector<floyd_ast::value_t>{ floyd_ast::value_t("xyz") });
	QUARK_TEST_VERIFY(*result.second == floyd_ast::value_t("--xyz<xyz>--"));

	const auto result2 = call_function(vm, f, vector<floyd_ast::value_t>{ floyd_ast::value_t("123") });
	QUARK_TEST_VERIFY(*result2.second == floyd_ast::value_t("--123<123>--"));
}



QUARK_UNIT_TESTQ("call_function()", "return from middle of function"){
	auto r = run_global(
		"string f(){"
		"	int dummy = print(\"A\");"
		"	return \"B\";"
		"	int dummy = print(\"C\");"
		"	return \"D\";"
		"}"
		"string x = f();"
		"int dummy = print(x);"
	);
	QUARK_UT_VERIFY((r._print_output == vector<string>{ "A", "B" }));
}

QUARK_UNIT_TESTQ("call_function()", "return from within IF block"){
	auto r = run_global(
		"string f(){"
		"	if(true){"
		"		int dummy = print(\"A\");"
		"		return \"B\";"
		"		int dummy = print(\"C\");"
		"	}"
		"	int dummy = print(\"D\");"
		"	return \"E\";"
		"}"
		"string x = f();"
		"int dummy = print(x);"
	);
	QUARK_UT_VERIFY((r._print_output == vector<string>{ "A", "B" }));
}

QUARK_UNIT_TESTQ("call_function()", "return from within FOR block"){
	auto r = run_global(
		"string f(){"
		"	for(e in 0...3){"
		"		int dummy = print(\"A\");"
		"		return \"B\";"
		"		int dummy2 = print(\"C\");"
		"	}"
		"	int dummy = print(\"D\");"
		"	return \"E\";"
		"}"
		"string x = f();"
		"int dummy = print(x);"
	);
	QUARK_UT_VERIFY((r._print_output == vector<string>{ "A", "B" }));
}

// ??? add test for: return from ELSE

QUARK_UNIT_TESTQ("call_function()", "return from within BLOCK"){
	auto r = run_global(
		"string f(){"
		"	{"
		"		int dummy = print(\"A\");"
		"		return \"B\";"
		"		int dummy2 = print(\"C\");"
		"	}"
		"	int dummy = print(\"D\");"
		"	return \"E\";"
		"}"
		"string x = f();"
		"int dummy = print(x);"
	);
	QUARK_UT_VERIFY((r._print_output == vector<string>{ "A", "B" }));
}






//////////////////////////		Host: to_string()



QUARK_UNIT_TESTQ("run_init()", ""){
	test__run_init__check_result(
		R"(
			string result = to_string(145);
		)"
		,
		value_t("145")
	);
}
QUARK_UNIT_TESTQ("run_init()", ""){
	test__run_init__check_result(
		R"(
			string result = to_string(3.1);
		)"
		,
		value_t("3.100000")
	);
}


//////////////////////////		Host: Print



QUARK_UNIT_TESTQ("run_global()", "Print Hello, world!"){
	const auto r = run_global(
		R"(
			int result = print("Hello, World!");
		)"
	);
	QUARK_UT_VERIFY((r._print_output == vector<string>{ "Hello, World!" }));
}


QUARK_UNIT_TESTQ("run_global()", "Test that VM state (print-log) escapes block!"){
	const auto r = run_global(
		R"(
			{
				int result = print("Hello, World!");
			}
		)"
	);
	QUARK_UT_VERIFY((r._print_output == vector<string>{ "Hello, World!" }));
}

QUARK_UNIT_TESTQ("run_global()", "Test that VM state (print-log) escapes IF!"){
	const auto r = run_global(
		R"(
			if(true){
				int result = print("Hello, World!");
			}
		)"
	);
	QUARK_UT_VERIFY((r._print_output == vector<string>{ "Hello, World!" }));
}



//////////////////////////		Host: get_time_of_day()



QUARK_UNIT_TESTQ("run_init()", "get_time_of_day()"){
	test__run_init2(
		R"(
			int a = get_time_of_day();
			int b = get_time_of_day();
			int c = b - a;
			int result = print("Delta time:" + to_string(a));
		)"
	);
}


//////////////////////////		Blocks and scoping



QUARK_UNIT_TESTQ("run_init()", "Empty block"){
	test__run_init2(
		"{}"
	);
}

QUARK_UNIT_TESTQ("run_init()", "Block with local variable, no shadowing"){
	test__run_init2(
		"{ int x = 4; }"
	);
}

QUARK_UNIT_TESTQ("run_init()", "Block with local variable, no shadowing"){
//			int dummy_a = print("A:" + to_string(x));
	const auto r = run_global(
		R"(
			int x = 3;
			int dummy_b = print("B:" + to_string(x));
			{
				int dummy_c = print("C:" + to_string(x));
				int x = 4;
				int dummy_d = print("D:" + to_string(x));
			}
			int dummy_e = print("E:" + to_string(x));
		)"
	);
	QUARK_UT_VERIFY((r._print_output == vector<string>{ "B:3", "C:3", "D:4", "E:3" }));
}


//////////////////////////		if-statement



QUARK_UNIT_TESTQ("run_init()", "if(true){}"){
	const auto r = run_global(
		R"(
			if(true){
				int dummy_1 = print("Hello!");
			}
			int dummy_2 = print("Goodbye!");
		)"
	);
	QUARK_UT_VERIFY((r._print_output == vector<string>{ "Hello!", "Goodbye!" }));
}
QUARK_UNIT_TESTQ("run_init()", "if(false){}"){
	const auto r = run_global(
		R"(
			if(false){
				int dummy_1 = print("Hello!");
			}
			int dummy_2 = print("Goodbye!");
		)"
	);
	QUARK_UT_VERIFY((r._print_output == vector<string>{ "Goodbye!" }));
}



QUARK_UNIT_TESTQ("run_init()", "if(true){}else{}"){
	const auto r = run_global(
		R"(
			if(true){
				int dummy_1 = print("Hello!");
			}
			else{
				int dummy_2 = print("Goodbye!");
			}
		)"
	);
	QUARK_UT_VERIFY((r._print_output == vector<string>{ "Hello!" }));
}
QUARK_UNIT_TESTQ("run_init()", "if(false){}else{}"){
	const auto r = run_global(
		R"(
			if(false){
				int dummy_1 = print("Hello!");
			}
			else{
				int dummy_2 = print("Goodbye!");
			}
		)"
	);
	QUARK_UT_VERIFY((r._print_output == vector<string>{ "Goodbye!" }));
}




QUARK_UNIT_TESTQ("run_init()", ""){
	const auto r = run_global(
		R"(
			if(1 == 1){
				int dummy = print("one");
			}
			else if(2 == 0){
				int dummy = print("two");
			}
			else if(3 == 0){
				int dummy = print("three");
			}
			else{
				int dummy = print("four");
			}
		)"
	);
	QUARK_UT_VERIFY((r._print_output == vector<string>{ "one" }));
}

QUARK_UNIT_TESTQ("run_init()", ""){
	const auto r = run_global(
		R"(
			if(1 == 0){
				int dummy = print("one");
			}
			else if(2 == 2){
				int dummy = print("two");
			}
			else if(3 == 0){
				int dummy = print("three");
			}
			else{
				int dummy = print("four");
			}
		)"
	);
	QUARK_UT_VERIFY((r._print_output == vector<string>{ "two" }));
}

QUARK_UNIT_TESTQ("run_init()", ""){
	const auto r = run_global(
		R"(
			if(1 == 0){
				int dummy = print("one");
			}
			else if(2 == 0){
				int dummy = print("two");
			}
			else if(3 == 3){
				int dummy = print("three");
			}
			else{
				int dummy = print("four");
			}
		)"
	);
	QUARK_UT_VERIFY((r._print_output == vector<string>{ "three" }));
}

QUARK_UNIT_TESTQ("run_init()", ""){
	const auto r = run_global(
		R"(
			if(1 == 0){
				int dummy = print("one");
			}
			else if(2 == 0){
				int dummy = print("two");
			}
			else if(3 == 0){
				int dummy = print("three");
			}
			else{
				int dummy = print("four");
			}
		)"
	);
	QUARK_UT_VERIFY((r._print_output == vector<string>{ "four" }));
}




//////////////////////////		for-statement


QUARK_UNIT_TESTQ("run_init()", "for"){
	const auto r = run_global(
		R"(
			for (i in 0...2) {
				int dummy = print("Iteration: " + to_string(i));
			}
		)"
	);
	QUARK_UT_VERIFY((r._print_output == vector<string>{ "Iteration: 0", "Iteration: 1", "Iteration: 2" }));
}



//////////////////////////		fibonacci


QUARK_UNIT_TESTQ("run_init()", "fibonacci"){
	const auto vm = run_global(
		"int fibonacci(int n) {"
		"	if (n <= 1){"
		"		return n;"
		"	}"
		"	return fibonacci(n - 2) + fibonacci(n - 1);"
		"}"

		"for (i in 0...10) {"
		"	int dummy = print(fibonacci(i));"
		"}"
	);

	QUARK_UT_VERIFY((
		vm._print_output == vector<string>{
			"0", "1", "1", "2", "3", "5", "8", "13", "21", "34",
			"55" //, "89", "144", "233", "377", "610", "987", "1597", "2584", "4181"
		})
	);
}





//////////////////////////		TEST STRUCT SUPPORT



#if false
QUARK_UNIT_TESTQ("run_main()", "struct"){
	QUARK_UT_VERIFY(test__run_main("struct t { int a;} bool main(){ t b = t_constructor(); return b == b; }", floyd_ast::value_t(true)));
}

QUARK_UNIT_1("run_main()", "", test__run_main(
	"struct t { int a;} bool main(){ t a = t_constructor(); t b = t_constructor(); return a == b; }",
	floyd_ast::value_t(true)
));


QUARK_UNIT_TESTQ("run_main()", ""){
	QUARK_TEST_VERIFY(test__run_main(
		"struct t { int a;} bool main(){ t a = t_constructor(); t b = t_constructor(); return a == b; }",
		floyd_ast::value_t(true)
	));
}

QUARK_UNIT_TESTQ("run_main()", ""){
	try {
		test__run_main(
			"struct t { int a;} bool main(){ t a = t_constructor(); int b = 1055; return a == b; }",
			floyd_ast::value_t(true)
		);
		QUARK_UT_VERIFY(false);
	}
	catch(...){
	}
}


QUARK_UNIT_TESTQ("struct", "Can define struct, instantiate it and read member data"){
	const auto a = run_main(
		"struct pixel { string s; }"
		"string main(){\n"
		"	pixel p = pixel_constructor();"
		"	return p.s;"
		"}\n",
		{}
	);
	QUARK_TEST_VERIFY(a.second == value_t(""));
}

QUARK_UNIT_TESTQ("struct", "Struct member default value"){
	const auto a = run_main(
		"struct pixel { string s = \"one\"; }"
		"string main(){\n"
		"	pixel p = pixel_constructor();"
		"	return p.s;"
		"}\n",
		{}
	);
	QUARK_TEST_VERIFY(a.second == value_t("one"));
}

QUARK_UNIT_TESTQ("struct", "Nesting structs"){
	const auto a = run_main(
		"struct pixel { string s = \"two\"; }"
		"struct image { pixel background_color; int width; int height; }"
		"string main(){\n"
		"	image i = image_constructor();"
		"	return i.background_color.s;"
		"}\n",
		{}
	);
	QUARK_TEST_VERIFY(a.second == value_t("two"));
}

QUARK_UNIT_TESTQ("struct", "Can use struct as argument"){
	const auto a = run_main(
		"string get_s(pixel p){ return p.s; }"
		"struct pixel { string s = \"two\"; }"
		"string main(){\n"
		"	pixel p = pixel_constructor();"
		"	return get_s(p);"
		"}\n",
		{}
	);
	QUARK_TEST_VERIFY(a.second == value_t("two"));
}

QUARK_UNIT_TESTQ("struct", "Can return struct"){
	const auto a = run_main(
		"struct pixel { string s = \"three\"; }"
		"pixel test(){ return pixel_constructor(); }"
		"string main(){\n"
		"	pixel p = test();"
		"	return p.s;"
		"}\n",
		{}
	);
	QUARK_TEST_VERIFY(a.second == value_t("three"));
}
#endif



}	//	floyd_interpreter

