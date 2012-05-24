/*

   Copyright (c) 2011, The Chinese University of Hong Kong

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

*/

#pragma once
#ifndef IDOCK_SEED_HPP
#define IDOCK_SEED_HPP

#include <ctime>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace idock
{
#ifdef _WIN32
	/// Returns current process ID.
	inline unsigned int pid()
	{
		return GetCurrentProcessId();
	}
#else
	/// Returns current process ID.
	inline unsigned int pid()
	{
		return getpid();
	}
#endif

	/// Generates a random seed from current process ID and current time.
	inline size_t random_seed()
	{
		return static_cast<size_t>(pid() * (time(0))); // The return type of time(0) is size_t.
	}
}

#endif
