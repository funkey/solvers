#include "SolverFactory.h"

#include <config.h>

#ifdef HAVE_GUROBI
#include "GurobiBackend.h"
#endif

#ifdef HAVE_CPLEX
#include "CplexBackend.h"
#endif

#ifdef HAVE_SCIP
#include "ScipBackend.h"
#endif

LinearSolverBackend*
SolverFactory::createLinearSolverBackend(Preference preference) const {

// by default, create a gurobi backend
#ifdef HAVE_GUROBI

	if (preference == Any || preference == Gurobi)
		return new GurobiBackend();

#endif

// if this is not available, create a CPLEX backend
#ifdef HAVE_CPLEX

	if (preference == Any || preference == Cplex)
		return new CplexBackend();

#endif

// if this is not available, create a SCIP backend
#ifdef HAVE_SCIP

	if (preference == Any || preference == Scip)
		return new ScipBackend();

#endif

// if this is not available as well, throw an exception

	BOOST_THROW_EXCEPTION(NoSolverException() << error_message("No linear solver available."));
}

QuadraticSolverBackend*
SolverFactory::createQuadraticSolverBackend(Preference preference) const {

// by default, create a gurobi backend
#ifdef HAVE_GUROBI

	if (preference == Any || preference == Gurobi)
			return new GurobiBackend();

#endif

// if this is not available, create a CPLEX backend
#ifdef HAVE_CPLEX

	if (preference == Any || preference == Cplex)
		return new CplexBackend();

#endif

// if this is not available as well, throw an exception

	BOOST_THROW_EXCEPTION(NoSolverException() << error_message("No quadratic solver available."));
}
