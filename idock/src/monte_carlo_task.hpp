#pragma once
#ifndef IDOCK_MONTE_CARLO_TASK_HPP
#define IDOCK_MONTE_CARLO_TASK_HPP

#include <boost/random.hpp>
#include "ligand.hpp"

// Choose the appropriate Mersenne Twister engine for random number generation on 32-bit or 64-bit platform.
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(__amd64__) || defined(_M_X64) || defined(_M_AMD64)
typedef boost::random::mt19937_64 mt19937eng;
#else
typedef boost::random::mt19937 mt19937eng;
#endif

const size_t num_alphas = 5; ///< Number of alpha values for determining step size in BFGS

/// Task for running Monte Carlo Simulated Annealing algorithm to find local minimums of the scoring function.
/// A Monte Carlo task uses a seed to initialize its own random number generator.
/// It starts from a random initial conformation,
/// repeats a specified number of iterations,
/// uses precalculated alpha values for line search during BFGS local search,
/// clusters free energies and heavy atom coordinate vectors of the best conformations into results,
/// and sorts the results in the ascending order of free energies.
void monte_carlo_task(ptr_vector<result>& results, const ligand& lig, const size_t seed, const array<fl, num_alphas>& alphas, const scoring_function& sf, const box& b, const vector<array3d<fl>>& grid_maps);

#endif
