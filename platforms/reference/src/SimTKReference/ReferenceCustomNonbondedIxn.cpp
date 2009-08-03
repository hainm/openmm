
/* Portions copyright (c) 2009 Stanford University and Simbios.
 * Contributors: Peter Eastman
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS, CONTRIBUTORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <string.h>
#include <sstream>

#include "../SimTKUtilities/SimTKOpenMMCommon.h"
#include "../SimTKUtilities/SimTKOpenMMLog.h"
#include "../SimTKUtilities/SimTKOpenMMUtilities.h"
#include "ReferenceForce.h"
#include "ReferenceCustomNonbondedIxn.h"

using std::map;
using std::string;
using std::stringstream;
using std::vector;

/**---------------------------------------------------------------------------------------

   ReferenceCustomNonbondedIxn constructor

   --------------------------------------------------------------------------------------- */

ReferenceCustomNonbondedIxn::ReferenceCustomNonbondedIxn(const Lepton::ExpressionProgram& energyExpression,
        const Lepton::ExpressionProgram& forceExpression, const vector<string>& parameterNames, const vector<Lepton::ExpressionProgram>& combiningRules) :
            cutoff(false), periodic(false), energyExpression(energyExpression), forceExpression(forceExpression),
            paramNames(parameterNames), combiningRules(combiningRules) {

   // ---------------------------------------------------------------------------------------

   // static const char* methodName = "\nReferenceCustomNonbondedIxn::ReferenceCustomNonbondedIxn";

   // ---------------------------------------------------------------------------------------

    for (int i = 0; i < paramNames.size(); i++) {
        for (int j = 1; j < 3; j++) {
            stringstream name;
            name << paramNames[i] << j;
            particleParamNames.push_back(name.str());
        }
    }
}

/**---------------------------------------------------------------------------------------

   ReferenceCustomNonbondedIxn destructor

   --------------------------------------------------------------------------------------- */

ReferenceCustomNonbondedIxn::~ReferenceCustomNonbondedIxn( ){

   // ---------------------------------------------------------------------------------------

   // static const char* methodName = "\nReferenceCustomNonbondedIxn::~ReferenceCustomNonbondedIxn";

   // ---------------------------------------------------------------------------------------

}

  /**---------------------------------------------------------------------------------------

     Set the force to use a cutoff.

     @param distance            the cutoff distance
     @param neighbors           the neighbor list to use

     @return ReferenceForce::DefaultReturn

     --------------------------------------------------------------------------------------- */

  int ReferenceCustomNonbondedIxn::setUseCutoff( RealOpenMM distance, const OpenMM::NeighborList& neighbors ) {

    cutoff = true;
    cutoffDistance = distance;
    neighborList = &neighbors;

    return ReferenceForce::DefaultReturn;
  }

  /**---------------------------------------------------------------------------------------

     Set the force to use periodic boundary conditions.  This requires that a cutoff has
     also been set, and the smallest side of the periodic box is at least twice the cutoff
     distance.

     @param boxSize             the X, Y, and Z widths of the periodic box

     @return ReferenceForce::DefaultReturn

     --------------------------------------------------------------------------------------- */

  int ReferenceCustomNonbondedIxn::setPeriodic( RealOpenMM* boxSize ) {

    assert(cutoff);
    assert(boxSize[0] >= 2.0*cutoffDistance);
    assert(boxSize[1] >= 2.0*cutoffDistance);
    assert(boxSize[2] >= 2.0*cutoffDistance);
    periodic = true;
    periodicBoxSize[0] = boxSize[0];
    periodicBoxSize[1] = boxSize[1];
    periodicBoxSize[2] = boxSize[2];
    return ReferenceForce::DefaultReturn;

  }


/**---------------------------------------------------------------------------------------

   Calculate the custom pair ixn

   @param numberOfAtoms    number of atoms
   @param atomCoordinates  atom coordinates
   @param atomParameters   atom parameters                             atomParameters[atomIndex][paramterIndex]
   @param exclusions       atom exclusion indices                      exclusions[atomIndex][atomToExcludeIndex]
                           exclusions[atomIndex][0] = number of exclusions
                           exclusions[atomIndex][1-no.] = atom indices of atoms to excluded from
                           interacting w/ atom atomIndex
   @param fixedParameters  non atom parameters (not currently used)
   @param globalParameters the values of global parameters
   @param forces           force array (forces added)
   @param energyByAtom     atom energy
   @param totalEnergy      total energy

   @return ReferenceForce::DefaultReturn

   --------------------------------------------------------------------------------------- */

int ReferenceCustomNonbondedIxn::calculatePairIxn( int numberOfAtoms, RealOpenMM** atomCoordinates,
                                             RealOpenMM** atomParameters, int** exclusions,
                                             RealOpenMM* fixedParameters, map<string, double> globalParameters, RealOpenMM** forces,
                                             RealOpenMM* energyByAtom, RealOpenMM* totalEnergy ) const {

   map<string, double> variablesForParams = globalParameters;
   map<string, double> variablesForForce = globalParameters;
   if (cutoff) {
       for (int i = 0; i < (int) neighborList->size(); i++) {
           OpenMM::AtomPair pair = (*neighborList)[i];

           // Apply the combining rules to compute the force field parameters.

           for (int j = 0; j < combiningRules.size(); j++) {
               variablesForParams[particleParamNames[j*2]] = atomParameters[pair.first][j];
               variablesForParams[particleParamNames[j*2+1]] = atomParameters[pair.second][j];
           }
           for (int j = 0; j < combiningRules.size(); j++)
               variablesForForce[paramNames[j]] = combiningRules[j].evaluate(variablesForParams);
           calculateOneIxn(pair.first, pair.second, atomCoordinates, variablesForForce, forces, energyByAtom, totalEnergy);
       }
   }
   else {
       // allocate and initialize exclusion array

       int* exclusionIndices = new int[numberOfAtoms];
       for( int ii = 0; ii < numberOfAtoms; ii++ ){
          exclusionIndices[ii] = -1;
       }

       for( int ii = 0; ii < numberOfAtoms; ii++ ){

          // set exclusions

          for( int jj = 1; jj <= exclusions[ii][0]; jj++ ){
             exclusionIndices[exclusions[ii][jj]] = ii;
          }

          // loop over atom pairs

          for( int jj = ii+1; jj < numberOfAtoms; jj++ ){

             if( exclusionIndices[jj] != ii ){

                 // Apply the combining rules to compute the force field parameters.

                 for (int j = 0; j < combiningRules.size(); j++) {
                     variablesForParams[particleParamNames[j*2]] = atomParameters[ii][j];
                     variablesForParams[particleParamNames[j*2+1]] = atomParameters[jj][j];
                 }
                 for (int j = 0; j < combiningRules.size(); j++)
                     variablesForForce[paramNames[j]] = combiningRules[j].evaluate(variablesForParams);
                 calculateOneIxn(ii, jj, atomCoordinates, variablesForForce, forces, energyByAtom, totalEnergy);
             }
          }
       }

       delete[] exclusionIndices;
   }

   return ReferenceForce::DefaultReturn;
}
/**---------------------------------------------------------------------------------------

   Calculate custom pair ixn for exceptions

   @param numberOfExceptions   number of exceptions
   @param atomIndices          indices of atoms participating in exception ixn: atomIndices[exceptionIndex][indices]
   @param atomCoordinates      atom coordinates: atomCoordinates[atomIndex][3]
   @param parameters           parameters: parameters[exceptionIndex][*]; contents of array
                               depend on ixn
   @param globalParameters     the values of global parameters
   @param forces               force array (forces added to current values): forces[atomIndex][3]
   @param totalEnergy          totalEnergy: sum over { energies[atomIndex] }

   --------------------------------------------------------------------------------------- */

void ReferenceCustomNonbondedIxn::calculateExceptionIxn( int numberOfExceptions, int** atomIndices, RealOpenMM** atomCoordinates,
                            RealOpenMM** parameters, map<string, double> globalParameters, RealOpenMM** forces,
                            RealOpenMM* energyByAtom, RealOpenMM* totalEnergy ) const {

    map<string, double> variables = globalParameters;
    for (int i = 0; i < numberOfExceptions; i++) {
        for (int j = 0; j < combiningRules.size(); j++)
            variables[paramNames[j]] = parameters[i][j];
        calculateOneIxn(atomIndices[i][0], atomIndices[i][1], atomCoordinates, variables, forces, energyByAtom, totalEnergy);
    }
}

  /**---------------------------------------------------------------------------------------

     Calculate one pair ixn between two atoms

     @param ii               the index of the first atom
     @param jj               the index of the second atom
     @param atomCoordinates  atom coordinates
     @param atomParameters   atom parameters (charges, c6, c12, ...)     atomParameters[atomIndex][paramterIndex]
     @param forces           force array (forces added)
     @param energyByAtom     atom energy
     @param totalEnergy      total energy

     --------------------------------------------------------------------------------------- */

void ReferenceCustomNonbondedIxn::calculateOneIxn( int ii, int jj, RealOpenMM** atomCoordinates,
                        map<string, double>& variables, RealOpenMM** forces,
                        RealOpenMM* energyByAtom, RealOpenMM* totalEnergy ) const {

    // ---------------------------------------------------------------------------------------

    static const std::string methodName = "\nReferenceCustomNonbondedIxn::calculateOneIxn";

    // ---------------------------------------------------------------------------------------

    // constants -- reduce Visual Studio warnings regarding conversions between float & double

    static const RealOpenMM zero        =  0.0;
    static const RealOpenMM one         =  1.0;
    static const RealOpenMM two         =  2.0;
    static const RealOpenMM three       =  3.0;
    static const RealOpenMM six         =  6.0;
    static const RealOpenMM twelve      = 12.0;
    static const RealOpenMM oneM        = -1.0;

    // get deltaR, R2, and R between 2 atoms

    RealOpenMM deltaR[ReferenceForce::LastDeltaRIndex];
    if (periodic)
        ReferenceForce::getDeltaRPeriodic( atomCoordinates[jj], atomCoordinates[ii], periodicBoxSize, deltaR );
    else
        ReferenceForce::getDeltaR( atomCoordinates[jj], atomCoordinates[ii], deltaR );
    if (cutoff && deltaR[ReferenceForce::RIndex] >= cutoffDistance)
        return;

    // accumulate forces

    variables["r"] = deltaR[ReferenceForce::RIndex];
    RealOpenMM dEdR = forceExpression.evaluate(variables)/(deltaR[ReferenceForce::RIndex]);
    for( int kk = 0; kk < 3; kk++ ){
       RealOpenMM force  = -dEdR*deltaR[kk];
       forces[ii][kk]   += force;
       forces[jj][kk]   -= force;
    }

    // accumulate energies

    if( totalEnergy || energyByAtom ) {
        RealOpenMM energy = energyExpression.evaluate(variables);
        if( totalEnergy )
           *totalEnergy += energy;
        if( energyByAtom ){
           energyByAtom[ii] += energy;
           energyByAtom[jj] += energy;
        }
    }
  }

