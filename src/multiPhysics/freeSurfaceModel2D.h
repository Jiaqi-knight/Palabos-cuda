/* This file is part of the Palabos library.
 *
 * Copyright (C) 2011-2013 FlowKit Sarl
 * Route d'Oron 2
 * 1010 Lausanne, Switzerland
 * E-mail contact: contact@flowkit.com
 *
 * The most recent release of Palabos can be downloaded at 
 * <http://www.palabos.org/>
 *
 * The library Palabos is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * The library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FREE_SURFACE_MODEL_2D_H
#define FREE_SURFACE_MODEL_2D_H

#include <algorithm>
#include "core/globalDefs.h"
#include "atomicBlock/dataProcessingFunctional2D.h"
#include "multiBlock/defaultMultiBlockPolicy2D.h"
#include "multiPhysics/freeSurfaceUtil2D.h"
#include "multiPhysics/freeSurfaceInitializer2D.h"
#include "dataProcessors/dataInitializerWrapper2D.h"
#include "basicDynamics/dynamicsProcessor2D.h"

namespace plb {

template<typename T, template<typename U> class Descriptor>
class TwoPhaseComputeNormals2D : public BoxProcessingFunctional2D {
public:
    TwoPhaseComputeNormals2D()
    {
        if (sizeof(T) == sizeof(float))
            precision = FLT;
        else if (sizeof(T) == sizeof(double))
            precision = DBL;
        else if (sizeof(T) == sizeof(long double))
            precision = LDBL;
        else
            PLB_ASSERT(false);
    }
    virtual TwoPhaseComputeNormals2D<T,Descriptor>* clone() const {
        return new TwoPhaseComputeNormals2D<T,Descriptor>(*this);
    }
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> atomicBlocks);
    virtual void getTypeOfModification (std::vector<modif::ModifT>& modified) const {
        std::fill(modified.begin(), modified.end(), modif::nothing);
        modified[0] = modif::nothing;         // Fluid.
        modified[1] = modif::nothing;         // rhoBar.
        modified[2] = modif::nothing;         // j.
        modified[3] = modif::nothing;         // Mass.
        modified[4] = modif::nothing;         // Volume fraction.
        modified[5] = modif::nothing;         // Flag-status.
        modified[6] = modif::staticVariables; // Normal.
        modified[7] = modif::nothing;         // Interface-lists.
        modified[8] = modif::nothing;         // Curvature.
        modified[9] = modif::nothing;         // Outside density.
    }
private:
    Precision precision;
};

template<typename T, template<typename U> class Descriptor>
class FreeSurfaceGeometry2D : public BoxProcessingFunctional2D {
public:
    FreeSurfaceGeometry2D(T contactAngle_)
        : contactAngle(contactAngle_)
    {
        Precision precision;
        if (sizeof(T) == sizeof(float))
            precision = FLT;
        else if (sizeof(T) == sizeof(double))
            precision = DBL;
        else if (sizeof(T) == sizeof(long double))
            precision = LDBL;
        else
            PLB_ASSERT(false);

        eps = getEpsilon<T>(precision);

        // The contact angle must take values between 0 and 180 degrees. If it is negative,
        // this means that contact angle effects will not be modeled.
        PLB_ASSERT(contactAngle < (T) 180.0 || fabs(contactAngle - (T) 180.0) <= eps);

        if (contactAngle < (T) 0.0 && fabs(contactAngle) > eps) {
            useContactAngle = 0;
        } else {
            useContactAngle = 1;
        }
    }
    virtual FreeSurfaceGeometry2D<T,Descriptor>* clone() const {
        return new FreeSurfaceGeometry2D<T,Descriptor>(*this);
    }
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> atomicBlocks);
    virtual void getTypeOfModification (std::vector<modif::ModifT>& modified) const {
        std::fill(modified.begin(), modified.end(), modif::nothing);
        modified[0] = modif::nothing;             // Fluid.
        modified[1] = modif::nothing;             // rhoBar.
        modified[2] = modif::nothing;             // j.
        modified[3] = modif::nothing;             // Mass.
        modified[4] = modif::nothing;             // Volume fraction.
        modified[5] = modif::nothing;             // Flag-status.
        modified[6] = modif::staticVariables;     // Normal.
        modified[7] = modif::nothing;             // Interface-lists.
        modified[8] = modif::staticVariables;     // Curvature.
        modified[9] = modif::nothing;             // Outside density.
    }
private:
    ScalarField2D<int> *getInterfaceFlags(Box2D domain, FreeSurfaceProcessorParam2D<T,Descriptor>& param);
    void computeHeights2D(FreeSurfaceProcessorParam2D<T,Descriptor>& param, int integrationDirection,
            plint iX, plint iY, T h[3][3]);
    void computeHeights2D(FreeSurfaceProcessorParam2D<T,Descriptor>& param, Array<int,2>& wallTangent0,
            Array<int,2>& wallTangent1, int integrationDirection, plint iX, plint iY, T h[3]);
private:
    enum { unTagged = 000, notInterface = 001, regular = 002, contactLine = 004, adjacent = 010 };
private:
    T contactAngle;
    int useContactAngle;
    T eps;
};

template<typename T, template<typename U> class Descriptor>
class TwoPhaseComputeCurvature2D : public BoxProcessingFunctional2D {
public:
    TwoPhaseComputeCurvature2D(T contactAngle_, Box2D globalBoundingBox_)
        : contactAngle(contactAngle_),
          globalBoundingBox(globalBoundingBox_)
    {
        if (sizeof(T) == sizeof(float))
            precision = FLT;
        else if (sizeof(T) == sizeof(double))
            precision = DBL;
        else if (sizeof(T) == sizeof(long double))
            precision = LDBL;
        else
            PLB_ASSERT(false);

        T eps = getEpsilon<T>(precision);

        // The contact angle must take values between 0 and 180 degrees. If it is negative,
        // this means that contact angle effects will not be modeled.
        PLB_ASSERT(contactAngle < (T) 180.0 || fabs(contactAngle - (T) 180.0) <= eps);

        if (contactAngle < (T) 0.0 && fabs(contactAngle) > eps) {
            useContactAngle = 0;
        } else {
            useContactAngle = 1;
        }

        if (useContactAngle) {
            T pi = 3.14159265358979323844;
            contactAngle *= pi / (T) 180.0;
        }
    }
    virtual TwoPhaseComputeCurvature2D<T,Descriptor>* clone() const {
        return new TwoPhaseComputeCurvature2D<T,Descriptor>(*this);
    }
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> atomicBlocks);
    virtual void getTypeOfModification (std::vector<modif::ModifT>& modified) const {
        std::fill(modified.begin(), modified.end(), modif::nothing);
        modified[0] = modif::nothing;             // Fluid.
        modified[1] = modif::nothing;             // rhoBar.
        modified[2] = modif::nothing;             // j.
        modified[3] = modif::nothing;             // Mass.
        modified[4] = modif::nothing;             // Volume fraction.
        modified[5] = modif::nothing;             // Flag-status.
        modified[6] = modif::nothing;             // Normal.
        modified[7] = modif::nothing;             // Interface-lists.
        modified[8] = modif::staticVariables;     // Curvature.
        modified[9] = modif::nothing;             // Outside density.
    }
private:
    T contactAngle;
    int useContactAngle;
    Box2D globalBoundingBox;
    Precision precision;
};

/// Compute the mass balance on every node in the domain, and store in mass matrix.
/** Input:
  *   - Flag-status:   needed in bulk+1
  *   - Mass:          needed in bulk
  *   - Volume fraction: needed in bulk
  *   - Populations:   needed in bulk+1
  * Output:
  *   - mass.
  **/
template< typename T,template<typename U> class Descriptor>
class FreeSurfaceMassChange2D : public BoxProcessingFunctional2D {
public:
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> atomicBlocks);
    virtual FreeSurfaceMassChange2D<T,Descriptor>* clone() const {
        return new FreeSurfaceMassChange2D<T,Descriptor>(*this);
    }
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const {
        std::fill(modified.begin(), modified.end(), modif::nothing);
        modified[0] = modif::nothing;         // Fluid.
        modified[1] = modif::nothing;         // rhoBar.
        modified[2] = modif::nothing;         // j.
        modified[3] = modif::staticVariables; // Mass.
        modified[4] = modif::nothing;         // Volume fraction.
        modified[5] = modif::nothing;         // Flag-status.
        modified[6] = modif::nothing;         // Normal.
        modified[7] = modif::nothing;         // Interface-lists.
        modified[8] = modif::nothing;         // Curvature.
        modified[9] = modif::nothing;         // Outside density.
   }
};

/// Completion scheme on the post-collide populations on interface cells.
/** Input:
  *   - Flag-status:   needed in bulk+1
  *   - Volume fraction: needed in bulk+1
  *   - Populations:   needed in bulk+1
  *   - Momentum:      needed in bulk+1
  *   - Density:       needed in bulk+1
  * Output:
  *   - Populations.
  **/
// ASK: This data processor loops over the whole volume. Is this really
//      necessary, or could one of the lists be used instead?
template<typename T, template<typename U> class Descriptor>
class FreeSurfaceCompletion2D : public BoxProcessingFunctional2D {
public:
    virtual FreeSurfaceCompletion2D<T,Descriptor>* clone() const {
        return new FreeSurfaceCompletion2D<T,Descriptor>(*this);
    }
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> atomicBlocks);
    virtual void getTypeOfModification (std::vector<modif::ModifT>& modified) const {
        std::fill(modified.begin(), modified.end(), modif::nothing);
        modified[0] = modif::nothing;         // Fluid. Should be: staticVariables.
        modified[1] = modif::nothing;         // rhoBar.
        modified[2] = modif::nothing;         // j.
        modified[3] = modif::nothing;         // Mass.
        modified[4] = modif::nothing;         // Volume fraction.
        modified[5] = modif::nothing;         // Flag-status.
        modified[6] = modif::nothing;         // Normal.
        modified[7] = modif::nothing;         // Interface-lists.
        modified[8] = modif::nothing;         // Curvature.
        modified[9] = modif::nothing;         // Outside density.
    }
};

/// Compute and store mass-fraction and macroscopic variables.
/** Input:
  *   - Flag-status:   needed in bulk
  *   - Mass:          needed in bulk
  *   - Populations:   needed in bulk
  * Output:
  *   - mass-fraction, density, momentum, flag (because setting bounce-back).
  **/
template<typename T, template<typename U> class Descriptor>
class FreeSurfaceMacroscopic2D : public BoxProcessingFunctional2D {
public:
    FreeSurfaceMacroscopic2D(T rhoDefault_)
        : rhoDefault(rhoDefault_)
    { }
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> atomicBlocks);
    virtual FreeSurfaceMacroscopic2D<T,Descriptor>* clone() const {
        return new FreeSurfaceMacroscopic2D<T,Descriptor>(*this);
    }
    virtual void getTypeOfModification (std::vector<modif::ModifT>& modified) const {
        std::fill(modified.begin(), modified.end(), modif::nothing);
        modified[0] = modif::nothing;         // Fluid. Should be: staticVariables.
        modified[1] = modif::staticVariables; // rhoBar.
        modified[2] = modif::staticVariables; // j.
        modified[3] = modif::staticVariables; // Mass. Should be: staticVariables.
        modified[4] = modif::staticVariables; // Volume fraction.
        modified[5] = modif::staticVariables; // Flag-status.
        modified[6] = modif::nothing;         // Normal.
        modified[7] = modif::nothing;         // Interface-lists.
        modified[8] = modif::nothing;         // Curvature.
        modified[9] = modif::nothing;         // Outside density.
    }
private:
    T rhoDefault;
};

/// Add the surface tension contribution.
/** Input:
  *   - Flag-status:   needed in bulk
  *   - Mass:          needed in bulk
  *   - Populations:   needed in bulk
  * Output:
  *   - mass-fraction, density, momentum, flag (because setting bounce-back).
  **/
template<typename T, template<typename U> class Descriptor>
class TwoPhaseAddSurfaceTension2D : public BoxProcessingFunctional2D {
public:
    TwoPhaseAddSurfaceTension2D(T surfaceTension_)
        : surfaceTension(surfaceTension_)
    { }
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> atomicBlocks);
    virtual TwoPhaseAddSurfaceTension2D<T,Descriptor>* clone() const {
        return new TwoPhaseAddSurfaceTension2D<T,Descriptor>(*this);
    }
    virtual void getTypeOfModification (std::vector<modif::ModifT>& modified) const {
        std::fill(modified.begin(), modified.end(), modif::nothing);
        modified[0] = modif::nothing;         // Fluid. Should be: staticVariables.
        modified[1] = modif::staticVariables; // rhoBar.
        modified[2] = modif::staticVariables; // j.
        modified[3] = modif::staticVariables; // Mass.
        modified[4] = modif::staticVariables; // Volume fraction.
        modified[5] = modif::staticVariables; // Flag-status.
        modified[6] = modif::nothing;         // Normal.
        modified[7] = modif::nothing;         // Interface-lists.
        modified[8] = modif::nothing;         // Curvature.
        modified[9] = modif::nothing;         // Outside density.
    }
private:
    T surfaceTension;
};

/// Based on the current flag status, decide, upon the value of mass fraction, which nodes shall
///   switch state.
/** Input:
  *   - Volume fraction: needed in bulk+2
  *   - Flag-status:   needed in bulk+2
  * Output:
  *   - interface-to-fluid list: defined in bulk+2
  *   - interface-to-empty list: defined in bulk+1
  *   - empty-to-interface list: defined in bulk+1
  **/
template<typename T,template<typename U> class Descriptor>
class FreeSurfaceComputeInterfaceLists2D : public BoxProcessingFunctional2D
{
public:
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> atomicBlocks);
    virtual FreeSurfaceComputeInterfaceLists2D<T,Descriptor>* clone() const {
        return new FreeSurfaceComputeInterfaceLists2D<T,Descriptor>(*this);
    }
    virtual void getTypeOfModification (std::vector<modif::ModifT>& modified) const {
        std::fill(modified.begin(), modified.end(), modif::nothing);
        modified[0] = modif::nothing;         // Fluid (not used in this processor).
        modified[1] = modif::nothing;         // rhoBar.
        modified[2] = modif::nothing;         // j.
        modified[3] = modif::nothing;         // Mass (not used in this processor).
        modified[4] = modif::nothing;         // Volume fraction, read-only.
        modified[5] = modif::nothing;         // Flag-status, read-only.
        modified[6] = modif::nothing;         // Normal.
        modified[7] = modif::staticVariables; // Interface-lists; all lists are created here.
        modified[8] = modif::nothing;         // Curvature.
        modified[9] = modif::nothing;         // Outside density.
    }
private:
    static T kappa; // Safety threshold for state-change, to prevent back-and-forth oscillations.
};

/** Input:
  *   - interface-to-fluid list: needed in bulk+1
  *   - interface-to-empty list: needed in bulk+1
  *   - density: needed in bulk+1
  *   - mass:    needed in bulk+1
  *   - flag:    needed in bulk+1
  * Output:
  *   - flag, dynamics, mass, volumeFraction, density, force, momentum
  *   - mass-excess-list: defined in bulk+1
  **/
template<typename T,template<typename U> class Descriptor>
class FreeSurfaceIniInterfaceToAnyNodes2D : public BoxProcessingFunctional2D {
public:
    FreeSurfaceIniInterfaceToAnyNodes2D(T rhoDefault_);
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> atomicBlocks);

    virtual FreeSurfaceIniInterfaceToAnyNodes2D<T,Descriptor>* clone() const {
        return new FreeSurfaceIniInterfaceToAnyNodes2D<T,Descriptor>(*this);
    }
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const {
        std::fill(modified.begin(), modified.end(), modif::nothing);
        modified[0] = modif::nothing;           // Fluid. Gets assigned new dynamics. Should be: dataStructure
        modified[1] = modif::staticVariables;   // rhoBar.
        modified[2] = modif::nothing;           // j. Should be: staticVariables.
        modified[3] = modif::staticVariables;   // Mass. Is redistributed and initialized from neighborying density.
        modified[4] = modif::nothing;           // Volume fraction. Is default-initialized. Should be: staticVariables.
        modified[5] = modif::staticVariables;   // Flag-status. Is adapted according to cell-change lists.
        modified[6] = modif::nothing;           // Normal.
        modified[7] = modif::nothing;           // Interface-lists. Read-only.
        modified[8] = modif::nothing;           // Curvature.
        modified[9] = modif::nothing;           // Outside density.
    }
private:
    T rhoDefault; 
};

/// Based on the previously computed empty->interface list, initialize flow variables for
///   new interface cells.
/** Input:
  *   - Populations: needed in bulk+0
  *   - Momentum:    needed in bulk+1
  *   - Density:     needed in bulk+1
  *   - Flag-status: needed in bulk+0
  * Output:
  *   - flag-status:   initialized to "interface" on corresponding cells.
  *   - lattice:       initialized from neighbor averages on new interface cells.
  *   - mass:          initialized to zero on new interface cells.
  *   - mass-fraction: initialized to zero on new interface cells.
  *   - momentum
  **/
template<typename T,template<typename U> class Descriptor>
class FreeSurfaceIniEmptyToInterfaceNodes2D: public BoxProcessingFunctional2D {
public:
    FreeSurfaceIniEmptyToInterfaceNodes2D(Dynamics<T,Descriptor>* dynamicsTemplate_, Array<T,Descriptor<T>::d> force_)
        : dynamicsTemplate(dynamicsTemplate_), force(force_)
    { }
    FreeSurfaceIniEmptyToInterfaceNodes2D(FreeSurfaceIniEmptyToInterfaceNodes2D<T,Descriptor> const& rhs)
        : dynamicsTemplate(rhs.dynamicsTemplate->clone()),
          force(rhs.force)
    { }
    FreeSurfaceIniEmptyToInterfaceNodes2D<T,Descriptor>* operator=(
            FreeSurfaceIniEmptyToInterfaceNodes2D<T,Descriptor> const& rhs)
    { 
        FreeSurfaceIniEmptyToInterfaceNodes2D<T,Descriptor>(rhs).swap(*this);
        return *this;
    }
    void swap(FreeSurfaceIniEmptyToInterfaceNodes2D<T,Descriptor>& rhs) {
        std::swap(dynamicsTemplate, rhs.dynamicsTemplate);
        std::swap(force, rhs.force);
    }
    ~FreeSurfaceIniEmptyToInterfaceNodes2D() {
        delete dynamicsTemplate;
    }
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> atomicBlocks);
    virtual FreeSurfaceIniEmptyToInterfaceNodes2D<T,Descriptor>* clone() const {
        return new FreeSurfaceIniEmptyToInterfaceNodes2D<T,Descriptor>(*this);
    }
    virtual void getTypeOfModification (std::vector<modif::ModifT>& modified) const {
        std::fill(modified.begin(), modified.end(), modif::nothing);
        modified[0] = modif::nothing;         // Fluid. Should be: dataStructure
        modified[1] = modif::staticVariables; // rhoBar.
        modified[2] = modif::nothing;         // j. Should be: staticVariables.
        modified[3] = modif::staticVariables; // Mass.
        modified[4] = modif::nothing;         // Volume fraction, read-only. Should be: staticVariables
        modified[5] = modif::staticVariables; // Flag-status, read-only.
        modified[6] = modif::nothing;         // Normal.
        modified[7] = modif::nothing;         // Interface-lists. Read access to gasCellToInitializeData.
        modified[8] = modif::nothing;         // Curvature.
        modified[9] = modif::nothing;         // Outside density.
    }
private:
    Dynamics<T,Descriptor>* dynamicsTemplate;
    Array<T,Descriptor<T>::d> force; // Body force, for initialization of the new interface cell.
};

/// Isolated cells cannot be part of the interface. This data processor spots and
/// removes them.
/** Input:
  *   - Flag-status: needed in bulk+2
  *   - mass:        needed in bulk+1
  *   - density:     needed in bulk+1
  * Output:
  *   - interfaceToFluidNodes:   initialized in bulk+1
  *   - interfaceToEmptyNodes:   initialized in bulk+1
  *   - massExcess list:         initialized in bulk+1
  *   - mass, density, mass-fraction, dynamics, force, momentum, flag: in bulk+1
  **/
template<typename T,template<typename U> class Descriptor>
class FreeSurfaceRemoveFalseInterfaceCells2D : public BoxProcessingFunctional2D {
public:
    FreeSurfaceRemoveFalseInterfaceCells2D(T rhoDefault_)
        : rhoDefault(rhoDefault_)
    { }
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> atomicBlocks);
    virtual FreeSurfaceRemoveFalseInterfaceCells2D<T,Descriptor>* clone() const {
        return new FreeSurfaceRemoveFalseInterfaceCells2D<T,Descriptor>(*this);
    }
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const {
        std::fill(modified.begin(), modified.end(), modif::nothing);
        modified[0] = modif::nothing;          // Fluid: Gets NoDynamics when node changes to empty. Should be: dataStructure.
        modified[1] = modif::staticVariables;  // rhoBar.
        modified[2] = modif::nothing;          // j. Should be: staticVariables.
        modified[3] = modif::staticVariables;  // Mass.
        modified[4] = modif::nothing;          // Volume fraction. Should be: staticVariables.
        modified[5] = modif::staticVariables;  // Flag-status.
        modified[6] = modif::nothing;          // Normal.
        modified[7] = modif::nothing;          // Interface lists.
        modified[8] = modif::nothing;          // Curvature.
        modified[9] = modif::nothing;          // Outside density.
    }
private:
    T rhoDefault;  
};

/// Enforce exact mass balance when interface cells become fluid or empty.
/** Input:
  *   - mass-excess list: needed in bulk+1
  *   - Flag-status: needed in bulk+2
  *   - mass:        needed in bulk+2
  *   - density:     needed in bulk+2
  * Output:
  *   - mass, mass-fraction
  **/
template<typename T,template<typename U> class Descriptor>
class FreeSurfaceEqualMassExcessReDistribution2D : public BoxProcessingFunctional2D {
public:
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> atomicBlocks);
    virtual FreeSurfaceEqualMassExcessReDistribution2D<T,Descriptor>* clone() const {
        return new FreeSurfaceEqualMassExcessReDistribution2D(*this);
    }
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const {
        std::fill(modified.begin(), modified.end(), modif::nothing);
        modified[0] = modif::dataStructure;    // Fluid.
        modified[1] = modif::staticVariables;  // rhoBar.
        modified[2] = modif::staticVariables;  // j.
        modified[3] = modif::staticVariables;  // Mass.
        modified[4] = modif::staticVariables;  // Volume fraction.
        modified[5] = modif::nothing;          // Flag-status.
        modified[6] = modif::nothing;          // Normal.
        modified[7] = modif::nothing;          // Interface lists.
        modified[8] = modif::nothing;          // Curvature.
        modified[9] = modif::nothing;          // Outside density.
    }
};

template<typename T,template<typename U> class Descriptor>
class TwoPhaseComputeStatistics2D : public BoxProcessingFunctional2D {
public:
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> atomicBlocks);
    virtual TwoPhaseComputeStatistics2D<T,Descriptor>* clone() const {
        return new TwoPhaseComputeStatistics2D(*this);
    }
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const {
        std::fill(modified.begin(), modified.end(), modif::nothing);
        modified[0] = modif::nothing;         // Fluid.
        modified[1] = modif::nothing;         // rhoBar.
        modified[2] = modif::nothing;         // j.
        modified[3] = modif::nothing;         // Mass.
        modified[4] = modif::nothing;         // Volume fraction.
        modified[5] = modif::nothing;         // Flag-status.
        modified[6] = modif::nothing;         // Normal.
        modified[7] = modif::nothing;         // Interface lists.
        modified[8] = modif::nothing;         // Curvature.
        modified[9] = modif::nothing;         // Outside density.
    }
};

template< typename T,template<typename U> class Descriptor>
class FreeSurfaceInterfaceFilter2D : public BoxProcessingFunctional2D {
public:
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> atomicBlocks);
    virtual FreeSurfaceInterfaceFilter2D<T,Descriptor>* clone() const {
        return new FreeSurfaceInterfaceFilter2D<T,Descriptor>(*this);
    }
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const {
        std::fill(modified.begin(), modified.end(), modif::nothing);
        modified[0] = modif::nothing;         // Fluid.
        modified[1] = modif::staticVariables; // rhoBar.
        modified[2] = modif::staticVariables; // j.
        modified[3] = modif::nothing;         // Mass.
        modified[4] = modif::nothing;         // Volume fraction.
        modified[5] = modif::nothing;         // Flag-status.
        modified[6] = modif::nothing;         // Normal.
        modified[7] = modif::nothing;         // Interface-lists.
        modified[8] = modif::nothing;         // Curvature.
        modified[9] = modif::nothing;         // Outside density.
   }
};

template<typename T,template<typename U> class Descriptor>
class InitializeInterfaceLists2D : public BoxProcessingFunctional2D {
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> atomicBlocks)
    {
        PLB_ASSERT(atomicBlocks.size()==1);
        
        AtomicContainerBlock2D* containerInterfaceLists = dynamic_cast<AtomicContainerBlock2D*>(atomicBlocks[0]);
        PLB_ASSERT(containerInterfaceLists);
        InterfaceLists<T,Descriptor>* interfaceLists = new InterfaceLists<T,Descriptor>;
        containerInterfaceLists->setData(interfaceLists);

    }
    virtual InitializeInterfaceLists2D<T,Descriptor>* clone() const {
        return new InitializeInterfaceLists2D<T,Descriptor>(*this);
    }
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const {
        // Default-assign potential other parameters present in a multi-fluid system.
        std::fill(modified.begin(), modified.end(), modif::nothing);
        modified[0] = modif::staticVariables;
    }
};

/// Wrapper for execution of InitializeInterfaceLists2D.
template<typename T,template<typename U> class Descriptor>
void initializeInterfaceLists2D(MultiContainerBlock2D& interfaceListBlock) {
    std::vector<MultiBlock2D*> arg;
    arg.push_back(&interfaceListBlock);
    applyProcessingFunctional (
            new InitializeInterfaceLists2D<T,Descriptor>,
            interfaceListBlock.getBoundingBox(), arg );
}

template<typename T, template<typename U> class Descriptor>
struct FreeSurfaceFields2D {
    //static const int envelopeWidth = 2;
    static const int envelopeWidth = 3; // Necessary when we use height functions to compute the curvature,
                                        // or when double smoothing is used at the data processor that computes the
                                        // normals from the volume fraction.
    //static const int envelopeWidth = 4; // Necessary when we use height functions to compute the curvature and
                                        //   use the old contact angle algorithm.
    static const int smallEnvelopeWidth = 1;

    static const int envelopeWidthForImmersedWalls = 4;

    FreeSurfaceFields2D(SparseBlockStructure2D const& blockStructure,
                        Dynamics<T,Descriptor>* dynamics_,
                        T rhoDefault_, T surfaceTension_, T contactAngle_, Array<T,2> force_,
                        bool useImmersedWalls = false)
        : dynamics(dynamics_),
          rhoDefault(rhoDefault_), surfaceTension(surfaceTension_), contactAngle(contactAngle_), force(force_),
          lattice (
                MultiBlockManagement2D (
                        blockStructure, defaultMultiBlockPolicy2D().getThreadAttribution(),
                        smallEnvelopeWidth ),
                defaultMultiBlockPolicy2D().getBlockCommunicator(),
                defaultMultiBlockPolicy2D().getCombinedStatistics(),
                defaultMultiBlockPolicy2D().getMultiCellAccess<T,Descriptor>(), dynamics->clone() ),
          helperLists(lattice),
          mass(lattice),
          flag (
                MultiBlockManagement2D (
                        blockStructure,
                        defaultMultiBlockPolicy2D().getThreadAttribution(),
                        useImmersedWalls ? envelopeWidthForImmersedWalls : envelopeWidth ),
                defaultMultiBlockPolicy2D().getBlockCommunicator(),
                defaultMultiBlockPolicy2D().getCombinedStatistics(),
                defaultMultiBlockPolicy2D().getMultiScalarAccess<int>() ),
          volumeFraction((MultiBlock2D&) flag),
          curvature (
                MultiBlockManagement2D (
                        blockStructure,
                        defaultMultiBlockPolicy2D().getThreadAttribution(),
                        envelopeWidth ),
                defaultMultiBlockPolicy2D().getBlockCommunicator(),
                defaultMultiBlockPolicy2D().getCombinedStatistics(),
                defaultMultiBlockPolicy2D().getMultiScalarAccess<T>() ),
          outsideDensity((MultiBlock2D&) curvature),
          rhoBar (
                MultiBlockManagement2D (
                        blockStructure,
                        defaultMultiBlockPolicy2D().getThreadAttribution(),
                        useImmersedWalls ? envelopeWidthForImmersedWalls : smallEnvelopeWidth ),
                defaultMultiBlockPolicy2D().getBlockCommunicator(),
                defaultMultiBlockPolicy2D().getCombinedStatistics(),
                defaultMultiBlockPolicy2D().getMultiScalarAccess<T>() ),
          j (
                MultiBlockManagement2D (
                        blockStructure,
                        defaultMultiBlockPolicy2D().getThreadAttribution(),
                        useImmersedWalls ? envelopeWidthForImmersedWalls : smallEnvelopeWidth ),
                defaultMultiBlockPolicy2D().getBlockCommunicator(),
                defaultMultiBlockPolicy2D().getCombinedStatistics(),
                defaultMultiBlockPolicy2D().getMultiTensorAccess<T,2>() ),
          normal((MultiBlock2D&) curvature)
    {
        Precision precision;
        if (sizeof(T) == sizeof(float))
            precision = FLT;
        else if (sizeof(T) == sizeof(double))
            precision = DBL;
        else if (sizeof(T) == sizeof(long double))
            precision = LDBL;
        else
            PLB_ASSERT(false);

        T eps = getEpsilon<T>(precision);
        // The contact angle must take values between 0 and 180 degrees. If it is negative,
        // this means that contact angle effects will not be modeled.
        PLB_ASSERT(contactAngle < (T) 180.0 || fabs(contactAngle - (T) 180.0) <= eps);

        if (fabs(surfaceTension) <= eps) {
            useSurfaceTension = 0;
        } else {
            useSurfaceTension = 1;
        }

        twoPhaseArgs = aggregateFreeSurfaceParams(lattice, rhoBar, j, mass, volumeFraction,
                    flag, normal, helperLists, curvature, outsideDensity);

        initializeInterfaceLists2D<T,Descriptor>(helperLists);
        lattice.periodicity().toggleAll(true);
        mass.periodicity().toggleAll(true);
        flag.periodicity().toggleAll(true);
        volumeFraction.periodicity().toggleAll(true);
        curvature.periodicity().toggleAll(true);
        outsideDensity.periodicity().toggleAll(true);
        rhoBar.periodicity().toggleAll(true);
        j.periodicity().toggleAll(true);
        normal.periodicity().toggleAll(true);
        setToConstant(flag, flag.getBoundingBox(), (int)twoPhaseFlag::empty);
        setToConstant(outsideDensity, outsideDensity.getBoundingBox(), rhoDefault);
        rhoBarJparam.push_back(&lattice);
        rhoBarJparam.push_back(&rhoBar);
        rhoBarJparam.push_back(&j);

        lattice.internalStatSubscription().subscribeSum();     // Total mass.
        lattice.internalStatSubscription().subscribeSum();     // Lost mass.
        lattice.internalStatSubscription().subscribeIntSum();  // Num interface cells.

        freeSurfaceDataProcessors(rhoDefault, force, *dynamics);
    }

    FreeSurfaceFields2D(FreeSurfaceFields2D<T,Descriptor> const& rhs)
        : dynamics(rhs.dynamics->clone()),
          rhoDefault(rhs.rhoDefault),
          surfaceTension(rhs.surfaceTension),
          contactAngle(rhs.contactAngle),
          useSurfaceTension(rhs.useSurfaceTension),
          force(rhs.force),
          lattice(rhs.lattice),
          helperLists(rhs.helperLists),
          mass(rhs.mass),
          flag(rhs.flag),
          volumeFraction(rhs.volumeFraction),
          curvature(rhs.curvature),
          outsideDensity(rhs.outsideDensity),
          rhoBar(rhs.rhoBar),
          j(rhs.j),
          normal(rhs.normal),
          rhoBarJparam(rhs.rhoBarJparam),
          twoPhaseArgs(rhs.twoPhaseArgs)
    { }

    void swap(FreeSurfaceFields2D<T,Descriptor>& rhs)
    {
        std::swap(dynamics, rhs.dynamics);
        std::swap(rhoDefault, rhs.rhoDefault);
        std::swap(surfaceTension, rhs.surfaceTension);
        std::swap(contactAngle, rhs.contactAngle);
        std::swap(useSurfaceTension, rhs.useSurfaceTension);
        std::swap(force, rhs.force);
        std::swap(lattice, rhs.lattice);
        std::swap(helperLists, rhs.helperLists);
        std::swap(mass, rhs.mass);
        std::swap(flag, rhs.flag);
        std::swap(volumeFraction, rhs.volumeFraction);
        std::swap(curvature, rhs.curvature);
        std::swap(outsideDensity, rhs.outsideDensity);
        std::swap(rhoBar, rhs.rhoBar);
        std::swap(j, rhs.j);
        std::swap(normal, rhs.normal);
        std::swap(rhoBarJparam, rhs.rhoBarJparam);
        std::swap(twoPhaseArgs, rhs.twoPhaseArgs);
    }

    FreeSurfaceFields2D<T,Descriptor>& operator=(FreeSurfaceFields2D<T,Descriptor> const& rhs)
    {
        FreeSurfaceFields2D<T,Descriptor>(rhs).swap(*this);
        return *this;
    }

    FreeSurfaceFields2D<T,Descriptor>* clone() const
    {
        return new FreeSurfaceFields2D<T,Descriptor>(*this);
    }

    ~FreeSurfaceFields2D() {
        delete dynamics;
    }

    void periodicityToggle(plint direction, bool periodic)
    {
        PLB_ASSERT(direction == 0 || direction == 1 || direction == 2);

        lattice.periodicity().toggle(direction, periodic);
        mass.periodicity().toggle(direction, periodic);
        flag.periodicity().toggle(direction, periodic);
        volumeFraction.periodicity().toggle(direction, periodic);
        curvature.periodicity().toggle(direction, periodic);
        outsideDensity.periodicity().toggle(direction, periodic);
        rhoBar.periodicity().toggle(direction, periodic);
        j.periodicity().toggle(direction, periodic);
        normal.periodicity().toggle(direction, periodic);
    }

    void periodicityToggleAll(bool periodic)
    {
        lattice.periodicity().toggleAll(periodic);
        mass.periodicity().toggleAll(periodic);
        flag.periodicity().toggleAll(periodic);
        volumeFraction.periodicity().toggleAll(periodic);
        curvature.periodicity().toggleAll(periodic);
        outsideDensity.periodicity().toggleAll(periodic);
        rhoBar.periodicity().toggleAll(periodic);
        j.periodicity().toggleAll(periodic);
        normal.periodicity().toggleAll(periodic);
    }

    void defaultInitialize() {
        applyProcessingFunctional (
           new DefaultInitializeFreeSurface2D<T,Descriptor>(dynamics->clone(), force, rhoDefault),
                   lattice.getBoundingBox(), twoPhaseArgs );
    }

    void partiallyDefaultInitialize() {
        applyProcessingFunctional (
           new PartiallyDefaultInitializeFreeSurface2D<T,Descriptor>(dynamics->clone(), force, rhoDefault),
                   lattice.getBoundingBox(), twoPhaseArgs );
    }
    void freeSurfaceDataProcessors(T rhoDefault, Array<T,2> force, Dynamics<T,Descriptor>& dynamics)
    {
        plint pl; // Processor level.

        /***** Initial level ******/
        pl = 0;

        integrateProcessingFunctional (
                new ExternalRhoJcollideAndStream2D<T,Descriptor>,
                lattice.getBoundingBox(), rhoBarJparam, pl );

        integrateProcessingFunctional (
                new TwoPhaseComputeNormals2D<T,Descriptor>,
                lattice.getBoundingBox(), twoPhaseArgs, pl );

        /***** New level ******/
        pl++;

        if (useSurfaceTension) {
            integrateProcessingFunctional (
                    new TwoPhaseComputeCurvature2D<T,Descriptor>(contactAngle, lattice.getBoundingBox()),
                    lattice.getBoundingBox(), twoPhaseArgs, pl );
            
            // To change to the curvature calculation with height functions, uncomment the next data processor and
            // comment out the two previous ones. If only the next data processor is used and there is no
            // surface tension, the normals are not computed at all. Be careful if you intent to use
            // the normals and do not have the surface tension algorithm enabled.
            //integrateProcessingFunctional (
            //        new FreeSurfaceGeometry2D<T,Descriptor>(contactAngle),
            //        lattice.getBoundingBox(), twoPhaseArgs, pl );
        }

        integrateProcessingFunctional (
            new FreeSurfaceMassChange2D<T,Descriptor>, lattice.getBoundingBox(),
            twoPhaseArgs, pl );
       
        integrateProcessingFunctional (
            new FreeSurfaceCompletion2D<T,Descriptor>,
            lattice.getBoundingBox(), twoPhaseArgs, pl );
                                    
        integrateProcessingFunctional (
            new FreeSurfaceMacroscopic2D<T,Descriptor>(rhoDefault),
            lattice.getBoundingBox(), twoPhaseArgs, pl );
        /***** New level ******/
        //pl++;

        //integrateProcessingFunctional (
        //        new FreeSurfaceInterfaceFilter2D<T,Descriptor>(),
        //        lattice.getBoundingBox(), twoPhaseArgs, pl );

        /***** New level ******/
        //pl++;

        //integrateProcessingFunctional (
        //        new FreeSurfaceInterfaceFilter2D<T,Descriptor>(),
        //        lattice.getBoundingBox(), twoPhaseArgs, pl );

        if (useSurfaceTension) {
            integrateProcessingFunctional (
                new TwoPhaseAddSurfaceTension2D<T,Descriptor>(surfaceTension),
                lattice.getBoundingBox(), twoPhaseArgs, pl );
        }

        /***** New level ******/
        pl++;

        integrateProcessingFunctional (
            new FreeSurfaceComputeInterfaceLists2D<T,Descriptor>(),
            lattice.getBoundingBox(), twoPhaseArgs, pl );

        integrateProcessingFunctional (
            new FreeSurfaceIniInterfaceToAnyNodes2D<T,Descriptor>(rhoDefault),
            lattice.getBoundingBox(), twoPhaseArgs, pl );
            
        integrateProcessingFunctional (
            new FreeSurfaceIniEmptyToInterfaceNodes2D<T,Descriptor>(dynamics.clone(), force),
                                    lattice.getBoundingBox(),
                                    twoPhaseArgs, pl ); 

        /***** New level ******/
        pl++;

        integrateProcessingFunctional (
            new FreeSurfaceRemoveFalseInterfaceCells2D<T,Descriptor>(rhoDefault),
            lattice.getBoundingBox(), twoPhaseArgs, pl);

        /***** New level ******/
        pl++;

        integrateProcessingFunctional (
            new FreeSurfaceEqualMassExcessReDistribution2D<T,Descriptor>(),
            lattice.getBoundingBox(), twoPhaseArgs, pl );

        integrateProcessingFunctional (
            new TwoPhaseComputeStatistics2D<T,Descriptor>,
            lattice.getBoundingBox(), twoPhaseArgs, pl );
    }

    Dynamics<T,Descriptor>* dynamics;
    T rhoDefault;
    T surfaceTension;
    T contactAngle;
    int useSurfaceTension;
    Array<T,2> force;
    MultiBlockLattice2D<T, Descriptor> lattice;
    MultiContainerBlock2D helperLists;
    MultiScalarField2D<T> mass;
    MultiScalarField2D<int> flag;
    MultiScalarField2D<T> volumeFraction;
    MultiScalarField2D<T> curvature;
    MultiScalarField2D<T> outsideDensity;
    MultiScalarField2D<T> rhoBar;
    MultiTensorField2D<T,2> j;
    MultiTensorField2D<T,2> normal;
    std::vector<MultiBlock2D*> rhoBarJparam;
    std::vector<MultiBlock2D*> twoPhaseArgs;
};

}  // namespace plb

#endif  // FREE_SURFACE_MODEL_2D_H

