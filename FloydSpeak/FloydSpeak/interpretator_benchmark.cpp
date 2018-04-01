//
//  floyd_test_suite.cpp
//  FloydSpeak
//
//  Created by Marcus Zetterquist on 2018-01-21.
//  Copyright © 2018 Marcus Zetterquist. All rights reserved.
//

#include "interpretator_benchmark.h"

#include "floyd_interpreter.h"

#include <string>
#include <vector>
#include <iostream>

using std::vector;
using std::string;

using namespace floyd;

#if 1

//////////////////////////////////////////		HELPERS

interpreter_context_t make_test_context(){
	const auto t = quark::trace_context_t(false, quark::get_trace());
	interpreter_context_t context{ t };
	return context;
}


//	Returns time in nanoseconds
std::int64_t measure_execution_time_ns(const std::string& test_name, std::function<void (void)> func){
	func();

	auto t0 = std::chrono::system_clock::now();
	func();
	auto t1 = std::chrono::system_clock::now();
	func();
	auto t2 = std::chrono::system_clock::now();
	func();
	auto t3 = std::chrono::system_clock::now();
	func();
	auto t4 = std::chrono::system_clock::now();
	func();
	auto t5 = std::chrono::system_clock::now();

	auto d0 = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0);
	auto d1 = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1);
	auto d2 = std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t2);
	auto d3 = std::chrono::duration_cast<std::chrono::nanoseconds>(t4 - t3);
	auto d4 = std::chrono::duration_cast<std::chrono::nanoseconds>(t5 - t4);

	const auto average = (d0 + d1 + d2 + d3 + d4) / 5.0;
	auto duration1 = std::chrono::duration_cast<std::chrono::nanoseconds>(average);

	int64_t duration = duration1.count();
	std::cout << test_name << " execution time (ns): " << duration << std::endl;
	return duration;
}

OFF_QUARK_UNIT_TEST("", "measure_execution_time_ns()", "", ""){
	const auto t = measure_execution_time_ns(
		"test",
		[] { std::cout << "Hello, my Greek friends"; }
	);
	std::cout << "duration2..." << t << std::endl;
}



//////////////////////////////////////////		TESTS

	const int64_t cpp_iterations = (50LL * 1000LL * 1000LL);
	const int64_t floyd_iterations = (50LL * 1000LL * 1000LL);

void copy_test(){
			volatile int64_t count = 0;
			for (int64_t i = 0 ; i < cpp_iterations ; i++) {
				count = count + 1;
			}
			assert(count > 0);
//			std::cout << "C++ count:" << count << std::endl;
}


OFF_QUARK_UNIT_TEST_VIP("Basic performance", "for-loop", "", ""){

	const auto cpp_ns = measure_execution_time_ns(
		"C++: For-loop",
		[&] {
			copy_test();
		}
	);

	interpreter_context_t context = make_test_context();
	const auto ast = program_to_ast2(
		context,
		R"(
			int f(){
				mutable int count = 0;
				for (index in 1...50000000) {
					count = count + 1;
				}
				return 13;
			}
	//				print("Floyd count:" + to_string(count));
		)"
	);
	interpreter_t vm(ast);
	const auto f = find_global_symbol2(vm, "f");
	QUARK_ASSERT(f != nullptr);

	const auto floyd_ns = measure_execution_time_ns(
		"Floyd: For-loop",
		[&] {
			const auto result = call_function(vm, bc_to_value(f->_value, f->_symbol._value_type), {});
		}
	);

	double cpp_iteration_time = (double)cpp_ns / (double)cpp_iterations;
	double floyd_iteration_time = (double)floyd_ns / (double)floyd_iterations;
	double k = cpp_iteration_time / floyd_iteration_time;
	std::cout << "Floyd performance: " << k << std::endl;
}




const int fib_cpp_iterations = 32;

int fibonacci(int n) {
	if (n <= 1){
		return n;
	}
	return fibonacci(n - 2) + fibonacci(n - 1);
}

void fib_test(){
	volatile int sum = 0;
	for (auto i = 0 ; i < fib_cpp_iterations ; i++) {
		const auto a = fibonacci(i);
		sum = sum + a;
	}
}

QUARK_UNIT_TEST_VIP("Basic performance", "fibonacci", "", ""){
	const auto cpp_ns = measure_execution_time_ns(
		"C++: Fibonacci",
		[&] {
			fib_test();
		}
	);

	interpreter_context_t context = make_test_context();
	const auto ast = program_to_ast2(
		context,
		R"(
			int fibonacci(int n) {
				if (n <= 1){
					return n;
				}
				return fibonacci(n - 2) + fibonacci(n - 1);
			}

			int f(){
				for (i in 0..<32) {
					a = fibonacci(i);
				}
				return 8;
			}
		)"
	);
	interpreter_t vm(ast);
	const auto f = find_global_symbol2(vm, "f");
	QUARK_ASSERT(f != nullptr);

	const auto floyd_ns = measure_execution_time_ns(
		"Floyd: Fibonacci",
		[&] {
			const auto result = call_function(vm, bc_to_value(f->_value, f->_symbol._value_type), {});
		}
	);

	double k = (double)cpp_ns / (double)floyd_ns;
	std::cout << "Floyd performance percentage of C++: " << (k * 100.0) << std::endl;
}

#endif
