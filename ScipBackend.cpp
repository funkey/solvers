#include <boost/timer/timer.hpp>
#include <boost/chrono.hpp>
#include <config.h>

#ifdef HAVE_SCIP

#include <sstream>

#include <scip/scipdefplugins.h>
#include <scip/cons_linear.h>

#include <util/Logger.h>
#include "ScipBackend.h"

using namespace logger;

LogChannel sciplog("sciplog", "[ScipBackend] ");

ScipBackend::ScipBackend() :
		_scip(0) {

	SCIP_CALL_ABORT(SCIPcreate(&_scip));
	SCIP_CALL_ABORT(SCIPincludeDefaultPlugins(_scip));
	SCIP_CALL_ABORT(SCIPcreateProbBasic(_scip, "problem"));
}

ScipBackend::~ScipBackend() {

	LOG_DEBUG(sciplog) << "destructing scip solver..." << std::endl;

	freeVariables();
	freeConstraints();

	if (_scip != 0)
		SCIP_CALL_ABORT(SCIPfree(&_scip));
}

void
ScipBackend::initialize(
		unsigned int numVariables,
		VariableType variableType) {

	initialize(numVariables, variableType, std::map<unsigned int, VariableType>());
}

void
ScipBackend::initialize(
		unsigned int                                numVariables,
		VariableType                                defaultVariableType,
		const std::map<unsigned int, VariableType>& specialVariableTypes) {

	if (sciplog.getLogLevel() >= Debug)
		setVerbose(true);
	else
		setVerbose(false);

	_numVariables = numVariables;

	// delete previous variables
	freeVariables();

	LOG_DEBUG(sciplog) << "creating " << _numVariables << " variables" << std::endl;

	for (int i = 0; i < _numVariables; i++) {

		SCIP_VAR* v;
		std::string name("x");
		name += boost::lexical_cast<std::string>(i);

		double lb, ub;
		SCIP_VARTYPE type = scipVarType(
				specialVariableTypes.count(i) ? specialVariableTypes.at(i) : defaultVariableType,
				lb, ub);

		SCIP_CALL_ABORT(SCIPcreateVarBasic(_scip, &v, name.c_str(), lb, ub, 0 /* obj */, type));
		SCIP_CALL_ABORT(SCIPaddVar(_scip, v));

		_variables.push_back(v);
	}

	for (SCIP_VAR* v : _variables)
		SCIP_CALL_ABORT(SCIPreleaseVar(_scip, &v));
}

void
ScipBackend::setObjective(const LinearObjective& objective) {

	setObjective((QuadraticObjective)objective);
}

void
ScipBackend::setObjective(const QuadraticObjective& objective) {

	LOG_ALL(sciplog) << "setting objective sense" << std::endl;

	// set sense of objective
	if (objective.getSense() == Minimize)
		SCIP_CALL_ABORT(SCIPsetObjsense(_scip, SCIP_OBJSENSE_MINIMIZE));
	else
		SCIP_CALL_ABORT(SCIPsetObjsense(_scip, SCIP_OBJSENSE_MAXIMIZE));

	// set the constant value of the objective

	LOG_ALL(sciplog) << "setting objective offset to " << objective.getConstant() << std::endl;

	double offset = SCIPgetOrigObjoffset(_scip);
	SCIP_CALL_ABORT(SCIPaddOrigObjoffset(_scip, objective.getConstant() - offset));

	LOG_DEBUG(sciplog) << "setting linear coefficients" << std::endl;

	for (unsigned int i = 0; i < _numVariables; i++) {

		LOG_ALL(sciplog) << "setting objective value of var " << i << " to " << objective.getCoefficients()[i] << std::endl;
		SCIP_CALL_ABORT(SCIPchgVarObj(_scip, _variables[i], objective.getCoefficients()[i]));
	}

	if (objective.getQuadraticCoefficients().size() > 0)
		UTIL_THROW_EXCEPTION(
				NotYetImplemented,
				"Quadratic objectives are not yet implemented for SCIP. "
				"You can do so by converting min xQx into min z s.t. z >= xQx");
}

void
ScipBackend::setConstraints(const LinearConstraints& constraints) {

	// remove previous constraints
	freeConstraints();

	// allocate memory for new constraints
	_constraints.reserve(constraints.size());

	LOG_DEBUG(sciplog) << "setting " << constraints.size() << " constraints" << std::endl;

	unsigned int j = 0;
	for (const LinearConstraint& constraint : constraints) {

		if (j > 0)
			if (j % 1000 == 0)
				LOG_ALL(sciplog) << "" << j << " constraints set so far" << std::endl;

		addConstraint(constraint);

		j++;
	}
}

void
ScipBackend::addConstraint(const LinearConstraint& constraint) {

	// create a list of variables and their coefficients
	std::vector<SCIP_VAR*> vars;
	std::vector<SCIP_Real> coefs;
	for (auto& p : constraint.getCoefficients()) {
		vars.push_back(_variables[p.first]);
		coefs.push_back(p.second);
	}

	// create the SCIP constraint lhs <= linear expr <= rhs
	SCIP_CONS* c;
	std::string name("c");
	name += boost::lexical_cast<std::string>(_constraints.size());

	// set lhs and rhs according to constraint relation
	SCIP_Real lhs = constraint.getValue();
	SCIP_Real rhs = constraint.getValue();
	if (constraint.getRelation() == LessEqual)
		lhs = -SCIPinfinity(_scip);
	if (constraint.getRelation() == GreaterEqual)
		rhs = SCIPinfinity(_scip);

	SCIP_CALL_ABORT(SCIPcreateConsBasicLinear(
			_scip,
			&c,
			name.c_str(),
			vars.size(),
			&vars[0],
			&coefs[0],
			lhs,
			rhs));

	_constraints.push_back(c);

	SCIP_CALL_ABORT(SCIPaddCons(_scip, c));
	SCIP_CALL_ABORT(SCIPreleaseCons(_scip, &c));
}

void
ScipBackend::setTimeout(double timeout) {

	SCIP_CALL_ABORT(SCIPsetRealParam(_scip, "limits/time", timeout));
}

void
ScipBackend::setOptimalityGap(double gap, bool absolute) {

	if (absolute)
		SCIP_CALL_ABORT(SCIPsetRealParam(_scip, "limits/absgap", gap));
	else
		SCIP_CALL_ABORT(SCIPsetRealParam(_scip, "limits/gap", gap));
}

void
ScipBackend::setNumThreads(unsigned int numThreads) {

	SCIP_CALL_ABORT(SCIPsetIntParam(_scip, "lp/threads", numThreads));
}

bool
ScipBackend::solve(Solution& x, std::string& msg) {

	LOG_ALL(sciplog) << "solving model" << std::endl;

	boost::timer::cpu_timer timer;
	timer.start();

	SCIP_CALL_ABORT(SCIPpresolve(_scip));
	SCIP_CALL_ABORT(SCIPsolve(_scip));

	boost::chrono::nanoseconds ns(timer.elapsed().system + timer.elapsed().user);
	double seconds = boost::chrono::duration<double>(ns).count();
	x.setTime(seconds);

	if (SCIPgetNSols(_scip) == 0) {

		msg = "Optimal solution *NOT* found";
		return false;
	}

	// extract solution
	SCIP_SOL* sol = SCIPgetBestSol(_scip);

	x.resize(_numVariables);
	for (unsigned int i = 0; i < _numVariables; i++)
		x[i] = SCIPgetSolVal(_scip, sol, _variables[i]);

	// get current value of the objective
	x.setValue(SCIPgetSolOrigObj(_scip, sol));

	SCIP_CALL_ABORT(SCIPfreeTransform(_scip));

	return true;
}

void
ScipBackend::setVerbose(bool verbose) {

	// setup GRB environment
	//if (verbose)
		//_model.getEnv().set(GRB_IntParam_OutputFlag, 1);
	//else
		//_model.getEnv().set(GRB_IntParam_OutputFlag, 0);
}

void
ScipBackend::freeVariables() {

	// SCIPfree should free the variables for us
	_variables.clear();
}

void
ScipBackend::freeConstraints() {

	// SCIPfree should free the constraints for us
	_constraints.clear();
}

SCIP_VARTYPE
ScipBackend::scipVarType(VariableType type, double& lb, double& ub) {

	if (type == Binary) {

		lb = 0;
		ub = 1;
		return SCIP_VARTYPE_INTEGER;
	}

	if (type == Integer) {

		lb = -SCIPinfinity(_scip);
		ub =  SCIPinfinity(_scip);
		return SCIP_VARTYPE_INTEGER;
	}

	if (type == Continuous) {

		lb = -SCIPinfinity(_scip);
		ub =  SCIPinfinity(_scip);
		return SCIP_VARTYPE_CONTINUOUS;
	}

	assert(false);
}

#endif // HAVE_SCIP

