// Copyright 2021-2026 Nikita Fediuchin. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/***********************************************************************************************************************
 * @file
 * @brief Common ECSM optimization functions. Helps compiler to better optimize the code.
 */

#ifdef __has_builtin
#define ECSM_HAVE_BUILTIN(x) __has_builtin(x)
#else
#define ECSM_HAVE_BUILTIN(x) 0
#endif

#ifndef EXPECT
#if ECSM_HAVE_BUILTIN(__builtin_expect)
	/**
	 * @brief Indicate that an expression is likely to evaluate to a specified value.
	 *
	 * @param expr integral-type expression to be evaluated
	 * @param value expected value of the expression
	 */
	#define EXPECT(expr, value) __builtin_expect(expr, value)
#else
	/**
	 * @brief No support in current compiler.
	 */
	#define EXPECT(expr, value) (expr)
#endif
#endif

#ifndef LIKELY
/**
 * @brief Indicates compiler that this expression is most likely true.
 * @param expr target expression to be evaluated
 */
#define LIKELY(expr) EXPECT(!!(expr), 1)
#endif
#ifndef UNLIKELY
/**
 * @brief Indicates compiler that this expression is unlikely true.
 * @param expr target expression to be evaluated
 */
#define UNLIKELY(expr) EXPECT(!!(expr), 0)
#endif

#ifndef ASSUME
#if !defined(NDEBUG)
	/**
	 * @brief Indicates compiler that this expression is always true.
	 * @param expr target expression to be evaluated
	 */
	#define ASSUME(expr) LIKELY(expr) ? void() : assert(false && #expr)
#elif ECSM_HAVE_BUILTIN(__builtin_assume)
	/**
	 * @brief Indicates compiler that this expression is always true.
	 * @param expr target expression to be evaluated
	 */
	#define ASSUME(expr) __builtin_assume(expr)
#elif defined(_MSC_VER)
	/**
	 * @brief Indicates compiler that this expression is always true.
	 * @param expr target expression to be evaluated
	 */
	#define ASSUME(expr) __assume(expr)
#else
	/**
	 * @brief Not supported by the used compiler.
	 */
	#define ASSUME(expr) void()
#endif
#endif