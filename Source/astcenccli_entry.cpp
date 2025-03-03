// SPDX-License-Identifier: Apache-2.0
// ----------------------------------------------------------------------------
// Copyright 2020-2023 Arm Limited
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy
// of the License at:
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.
// ----------------------------------------------------------------------------

/**
 * @brief Platform-specific function implementations.
 *
 * This module contains the CLI entry point which also performs the role of
 * validating the host extended ISA support meets the needs of the tools.
 */

#include "astcenccli_internal.h"

#if (ASTCENC_SSE > 0)    || (ASTCENC_AVX > 0) || \
    (ASTCENC_POPCNT > 0) || (ASTCENC_F16C > 0)

static bool g_init { false };

/** Does this CPU support SSE 4.1? Set to -1 if not yet initialized. */
static bool g_cpu_has_sse41 { false };

/** Does this CPU support AVX2? Set to -1 if not yet initialized. */
static bool g_cpu_has_avx2 { false };

/** Does this CPU support POPCNT? Set to -1 if not yet initialized. */
static bool g_cpu_has_popcnt { false };

/** Does this CPU support F16C? Set to -1 if not yet initialized. */
static bool g_cpu_has_f16c { false };

/* ============================================================================
   Platform code for Visual Studio
============================================================================ */
#if !defined(__clang__) && defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>

/**
 * @brief Detect platform CPU ISA support and update global trackers.
 */
static void detect_cpu_isa()
{
	int data[4];

	__cpuid(data, 0);
	int num_id = data[0];

	if (num_id >= 1)
	{
		__cpuidex(data, 1, 0);
		// SSE41 = Bank 1, ECX, bit 19
		g_cpu_has_sse41 = data[2] & (1 << 19) ? true : false;
		// POPCNT = Bank 1, ECX, bit 23
		g_cpu_has_popcnt = data[2] & (1 << 23) ? true : false;
		// F16C = Bank 1, ECX, bit 29
		g_cpu_has_f16c = data[2] & (1 << 29) ? true : false;
	}

	if (num_id >= 7)
	{
		__cpuidex(data, 7, 0);
		// AVX2 = Bank 7, EBX, bit 5
		g_cpu_has_avx2 = data[1] & (1 << 5) ? true : false;
	}

	// Ensure state bits are updated before init flag is updated
	MemoryBarrier();
	g_init = true;
}

/* ============================================================================
   Platform code for GCC and Clang
============================================================================ */
#else
#include <cpuid.h>

/**
 * @brief Detect platform CPU ISA support and update global trackers.
 */
static void detect_cpu_isa()
{
	unsigned int data[4];

	if (__get_cpuid_count(1, 0, &data[0], &data[1], &data[2], &data[3]))
	{
		// SSE41 = Bank 1, ECX, bit 19
		g_cpu_has_sse41 = data[2] & (1 << 19) ? true : false;
		// POPCNT = Bank 1, ECX, bit 23
		g_cpu_has_popcnt = data[2] & (1 << 23) ? true : false;
		// F16C = Bank 1, ECX, bit 29
		g_cpu_has_f16c = data[2] & (1 << 29) ? true : false;
	}

	g_cpu_has_avx2 = 0;
	if (__get_cpuid_count(7, 0, &data[0], &data[1], &data[2], &data[3]))
	{
		// AVX2 = Bank 7, EBX, bit 5
		g_cpu_has_avx2 = data[1] & (1 << 5) ? true : false;
	}

	// Ensure state bits are updated before init flag is updated
	__sync_synchronize();
	g_init = true;
}
#endif

/* See header for documentation. */
bool cpu_supports_popcnt()
{
	if (!g_init)
	{
		detect_cpu_isa();
	}

	return g_cpu_has_popcnt;
}

/* See header for documentation. */
bool cpu_supports_f16c()
{
	if (!g_init)
	{
		detect_cpu_isa();
	}

	return g_cpu_has_f16c;
}

/* See header for documentation. */
bool cpu_supports_sse41()
{
	if (!g_init)
	{
		detect_cpu_isa();
	}

	return g_cpu_has_sse41;
}

/* See header for documentation. */
bool cpu_supports_avx2()
{
	if (!g_init)
	{
		detect_cpu_isa();
	}

	return g_cpu_has_avx2;
}

/**
 * @brief Validate CPU ISA support meets the requirements of this build of the library.
 *
 * Each library build is statically compiled for a particular set of CPU ISA features, such as the
 * SIMD support or other ISA extensions such as POPCNT. This function checks that the host CPU
 * actually supports everything this build needs.
 *
 * @return Return @c true if validated, @c false otherwise.
 */
static bool validate_cpu_isa()
{
	#if ASTCENC_AVX >= 2
		if (!cpu_supports_avx2())
		{
			print_error("ERROR: Host does not support AVX2 ISA extension\n");
			return false;
		}
	#endif

	#if ASTCENC_F16C >= 1
		if (!cpu_supports_f16c())
		{
			print_error("ERROR: Host does not support F16C ISA extension\n");
			return false;
		}
	#endif

	#if ASTCENC_SSE >= 41
		if (!cpu_supports_sse41())
		{
			print_error("ERROR: Host does not support SSE4.1 ISA extension\n");
			return false;
		}
	#endif

	#if ASTCENC_POPCNT >= 1
		if (!cpu_supports_popcnt())
		{
			print_error("ERROR: Host does not support POPCNT ISA extension\n");
			return false;
		}
	#endif

	return true;
}

#else

// Fallback for cases with no dynamic ISA availability
bool validate_cpu_isa()
{
	return true;
}

#endif

int main(
	int argc,
	char **argv
) {
	if (!validate_cpu_isa())
	{
		return 1;
	}

	return astcenc_main(argc, argv);
}
