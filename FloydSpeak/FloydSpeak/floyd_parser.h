//
//  floyd_parser.h
//  FloydSpeak
//
//  Created by Marcus Zetterquist on 10/04/16.
//  Copyright © 2016 Marcus Zetterquist. All rights reserved.
//

#ifndef floyd_parser_h
#define floyd_parser_h

#include "quark.h"
#include <string>

struct json_t;
struct seq_t;

namespace floyd_parser {


const std::string k_test_program_0_source = "int main(){ return 3; }";
const std::string k_test_program_0_parserout = R"(
	[
		[
			"def-func",
			{
				"args": [],
				"name": "main",
				"return_type": "<int>",
				"statements": [
					[ "return", [ "k", 3, "<int>" ] ]
				]
			}
		]
	]
)";
const std::string k_test_program_0_pass2output = R"(
	{
		"global": {
			"args": [],
			"locals": [],
			"members": [],
			"name": "global",
			"return_type": "",
			"statements": [],
			"type": "global"
		},
		"lookup": {
			"$1000": {
				"base_type": "function",
				"path": "global/main",
				"scope_def": {
					"args": [],
					"locals": [],
					"members": [],
					"name": "main",
					"return_type": "$int",
					"statements": [
						[ "return", [ "k", 3, "$int" ] ]
					],
					"type": "function",
					"types": {}
				}
			}
		}
	}
)";




const std::string k_test_program_1_source =
	"int main(string args){\n"
	"	return 3;\n"
	"}\n";
const std::string k_test_program_1_parserout = R"(
	[
		[
			"def-func",
			{
				"args": [
					{ "name": "args", "type": "<string>" }
				],
				"name": "main",
				"return_type": "<int>",
				"statements": [
					[ "return", [ "k", 3, "<int>" ] ]
				]
			}
		]
	]
)";


const char k_test_program_100_source[] = R"(
	struct pixel { float red; float green; float blue; }
	float get_grey(pixel p){ return (p.red + p.green + p.blue) / 3.0; }

	float main(){
		pixel p = pixel(1, 0, 0);
		return get_grey(p);
	}
)";
const char k_test_program_100_parserout[] = R"(
	[
		[
			"def-struct",
			{
				"members": [
					{ "name": "red", "type": "<float>" },
					{ "name": "green", "type": "<float>" },
					{ "name": "blue", "type": "<float>" }
				],
				"name": "pixel"
			}
		],
		[
			"def-func",
			{
				"args": [{ "name": "p", "type": "<pixel>" }],
				"name": "get_grey",
				"return_type": "<float>",
				"statements": [
					[
						"return",
						[
							"/",
							[
								"+",
								["+", ["->", ["@", "p"], "red"], ["->", ["@", "p"], "green"]],
								["->", ["@", "p"], "blue"]
							],
							["k", 3.0, "<float>"]
						]
					]
				]
			}
		],
		[
			"def-func",
			{
				"args": [],
				"name": "main",
				"return_type": "<float>",
				"statements": [
					[
						"bind",
						"<pixel>",
						"p",
						["call", ["@", "pixel"], [["k", 1, "<int>"], ["k", 0, "<int>"], ["k", 0, "<int>"]]]
					],
					["return", ["call", ["@", "get_grey"], [["@", "p"]]]]
				]
			}
		]
	]
)";



	/*
		OUTPUT
			json_t statement_array;
			std::string _rest;
	*/
	std::pair<json_t, seq_t> read_statements2(const seq_t& s);

	/*
	{
		"name": "global", "type": "global",
		"statements": [
			[ "return", EXPRESSION ],
			[ "bind", "<float>", "x", EXPRESSION ],
			[
				"def-struct",
				{
					"name": "pixel",
					"members": [
						{ "expr": [ "k", "two", "<string>" ], "name": "s", "type": "<string>" }
					],
				}
			],
			[
				"def-func",
				{
					"args": [],
					"locals": [],
					"name": "main",
					"return_type": "<int>",
					"statements": [
						[ "return", [ "k", 3, "<int>" ]]
					],
					"type": "function",
					"types": {}
				}
			]
		]
	}
	*/
	json_t parse_program2(const std::string& program);

}	//	floyd_parser


#endif /* floyd_parser_h */
