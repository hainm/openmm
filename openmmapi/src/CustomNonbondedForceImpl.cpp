/* -------------------------------------------------------------------------- *
 *                                   OpenMM                                   *
 * -------------------------------------------------------------------------- *
 * This is part of the OpenMM molecular simulation toolkit originating from   *
 * Simbios, the NIH National Center for Physics-Based Simulation of           *
 * Biological Structures at Stanford, funded under the NIH Roadmap for        *
 * Medical Research, grant U54 GM072970. See https://simtk.org.               *
 *                                                                            *
 * Portions copyright (c) 2008-2009 Stanford University and the Authors.      *
 * Authors: Peter Eastman                                                     *
 * Contributors:                                                              *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person obtaining a    *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction, including without limitation  *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
 * and/or sell copies of the Software, and to permit persons to whom the      *
 * Software is furnished to do so, subject to the following conditions:       *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
 * THE AUTHORS, CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,    *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR      *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE  *
 * USE OR OTHER DEALINGS IN THE SOFTWARE.                                     *
 * -------------------------------------------------------------------------- */

#include "openmm/OpenMMException.h"
#include "openmm/internal/ContextImpl.h"
#include "openmm/internal/CustomNonbondedForceImpl.h"
#include "openmm/kernels.h"
#include <sstream>

using namespace OpenMM;
using std::map;
using std::pair;
using std::vector;
using std::set;
using std::string;
using std::stringstream;

CustomNonbondedForceImpl::CustomNonbondedForceImpl(CustomNonbondedForce& owner) : owner(owner) {
}

CustomNonbondedForceImpl::~CustomNonbondedForceImpl() {
}

void CustomNonbondedForceImpl::initialize(ContextImpl& context) {
    kernel = context.getPlatform().createKernel(CalcCustomNonbondedForceKernel::Name(), context);

    // Check for errors in the specification of parameters and exceptions.

    System& system = context.getSystem();
    if (owner.getNumParticles() != system.getNumParticles())
        throw OpenMMException("CustomNonbondedForce must have exactly as many particles as the System it belongs to.");
    vector<set<int> > exceptions(owner.getNumParticles());
    vector<double> parameters;
    int numParameters = owner.getNumParameters();
    for (int i = 0; i < owner.getNumParticles(); i++) {
        owner.getParticleParameters(i, parameters);
        if (parameters.size() != numParameters) {
            stringstream msg;
            msg << "CustomNonbondedForce: Wrong number of parameters for particle ";
            msg << i;
            throw OpenMMException(msg.str());
        }
    }
    for (int i = 0; i < owner.getNumExceptions(); i++) {
        int particle1, particle2;
        owner.getExceptionParameters(i, particle1, particle2, parameters);
        if (particle1 < 0 || particle1 >= owner.getNumParticles()) {
            stringstream msg;
            msg << "CustomNonbondedForce: Illegal particle index for an exception: ";
            msg << particle1;
            throw OpenMMException(msg.str());
        }
        if (particle2 < 0 || particle2 >= owner.getNumParticles()) {
            stringstream msg;
            msg << "CustomNonbondedForce: Illegal particle index for an exception: ";
            msg << particle2;
            throw OpenMMException(msg.str());
        }
        if (exceptions[particle1].count(particle2) > 0 || exceptions[particle2].count(particle1) > 0) {
            stringstream msg;
            msg << "CustomNonbondedForce: Multiple exceptions are specified for particles ";
            msg << particle1;
            msg << " and ";
            msg << particle2;
            throw OpenMMException(msg.str());
        }
        if (parameters.size() != 0 && parameters.size() != numParameters) {
            stringstream msg;
            msg << "CustomNonbondedForce: Wrong number of parameters for exception ";
            msg << i;
            throw OpenMMException(msg.str());
        }
        exceptions[particle1].insert(particle2);
        exceptions[particle2].insert(particle1);
    }
    dynamic_cast<CalcCustomNonbondedForceKernel&>(kernel.getImpl()).initialize(context.getSystem(), owner);
}

void CustomNonbondedForceImpl::calcForces(ContextImpl& context, Stream& forces) {
    dynamic_cast<CalcCustomNonbondedForceKernel&>(kernel.getImpl()).executeForces(context);
}

double CustomNonbondedForceImpl::calcEnergy(ContextImpl& context) {
    return dynamic_cast<CalcCustomNonbondedForceKernel&>(kernel.getImpl()).executeEnergy(context);
}

vector<string> CustomNonbondedForceImpl::getKernelNames() {
    vector<string> names;
    names.push_back(CalcCustomNonbondedForceKernel::Name());
    return names;
}

map<string, double> CustomNonbondedForceImpl::getDefaultParameters() {
    map<string, double> parameters;
    for (int i = 0; i < owner.getNumGlobalParameters(); i++)
        parameters[owner.getGlobalParameterName(i)] = 0.0;
    return parameters;
}