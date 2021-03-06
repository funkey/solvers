#ifndef INFERENCE_DEFAULT_FACTORY_H__
#define INFERENCE_DEFAULT_FACTORY_H__

#include <memory>
#include <util/exceptions.h>
#include "LinearSolverBackendFactory.h"
#include "QuadraticSolverBackendFactory.h"

struct NoSolverException : virtual Exception {};

class SolverFactory :
		public LinearSolverBackendFactory,
		public QuadraticSolverBackendFactory {

public:

	std::shared_ptr<LinearSolverBackend> createLinearSolverBackend(Preference preference = Any) const;

	std::shared_ptr<QuadraticSolverBackend> createQuadraticSolverBackend(Preference preference = Any) const;
};

#endif // INFERENCE_DEFAULT_FACTORY_H__

