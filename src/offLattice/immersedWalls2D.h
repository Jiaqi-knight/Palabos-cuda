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

#ifndef IMMERSED_WALLS_2D_H
#define IMMERSED_WALLS_2D_H

#include "core/globalDefs.h"
#include "core/array.h"
#include "atomicBlock/dataProcessingFunctional2D.h"
#include "atomicBlock/dataField2D.h"

namespace plb {

/* ******** InamuroDeltaFunction ************************************ */

template<typename T>
class InamuroDeltaFunction {
public:
    InamuroDeltaFunction(int N_)
        : N(N_),
          dx(4./(T)N),
          invDx(1./dx)
    {
        sampleFunction();
    }
    T rawValue(T r) const {
        T rabs = fabs(r);
        T rsqr = r*r;
        if (rabs<1.) {
            return 0.125*(3.-2.*rabs+sqrt(1.+4.*rabs-4.*rsqr));
        }
        else if (rabs<2.) {
            return 0.125*(5.-2.*rabs-sqrt(-7.+12.*rabs-4.*rsqr));
        }
        else {
            return 0.;
        }
    }
    T w(T r) const {
        int position = (int)((r+2.0)*invDx+0.5);
        if (position<=0) {
            return 0.;
        }
        if (position>=N) {
            return 0.;
        }
        return samples[position];
    }
    T W(Array<T,2> const& r) const {
        return w(r[0])*w(r[1])*w(r[2]);
    }
private:
    void sampleFunction() {
        samples.resize(N+1);
        for(int i=0; i<=N; ++i) {
            samples[i] = rawValue(-2.+dx*i);
        }
    }
private:
    int N;
    T dx, invDx;
    std::vector<T> samples;
};

template<typename T>
InamuroDeltaFunction<T> const& inamuroDeltaFunction() {
    static InamuroDeltaFunction<T> deltaFunction(1000);
    return deltaFunction;
}

/* ******** ImmersedWallData2D ************************************ */

template<typename T>
struct ImmersedWallData2D : public ContainerBlockData
{
    Array<T,2> offset; // To convert vertices from local to absolute units.
    std::vector< Array<T,2> > vertices;
    std::vector<T> areas;
    std::vector< Array<T,2> > normals;
    std::vector< Array<T,2> > g;
    std::vector<int> flags; // Flag for each vertex used to distinguish between vertices for conditional reduction operations.
    std::vector<pluint> globalVertexIds;
    virtual ImmersedWallData2D<T>* clone() const {
        return new ImmersedWallData2D<T>(*this);
    }
};

/* ******** Utility functions ************************************ */

template<typename T>
inline bool closedOpenContained(Array<T,2> const& x, Box2D const& box) { 
    return x[0]>=(box.x0-0.5) && x[0]<(box.x1+0.5) &&
           x[1]>=(box.y0-0.5) && x[1]<(box.y1+0.5);
    // in order to count correctly the particles, a 0.5 must be added
}

/* ******** ReduceAxialTorqueImmersed2D ************************************ */

// The reduced quantity is computed only for the vertices which have a flag
// equal to "reductionFlag".
template<typename T>
class ReduceAxialTorqueImmersed2D : public PlainReductiveBoxProcessingFunctional2D
{
public:
    ReduceAxialTorqueImmersed2D(Array<T,2> const& center_, Array<T,2> const& unitaryAxis_, int reductionFlag_ = 0);
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> fields);
    virtual ReduceAxialTorqueImmersed2D<T>* clone() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
    virtual BlockDomain::DomainT appliesTo() const;
    Array<T,2> getSumTorque() const;
private:
    Array<T,2> center, unitaryAxis;
    Array<plint,2> sum_torque_ids;
    int reductionFlag;
};

template<typename T>
Array<T,2> reduceAxialTorqueImmersed(MultiContainerBlock2D& container, Array<T,2> center, Array<T,2> unitaryAxis,
        int reductionFlag = 0)
{
    std::vector<MultiBlock2D*> args;
    args.push_back(&container);
    ReduceAxialTorqueImmersed2D<T> functional(center, unitaryAxis, reductionFlag);
    applyProcessingFunctional(functional, container.getBoundingBox(), args);
    return functional.getSumTorque();
}

/* ******** ReduceImmersedForce2D ************************************ */

// The reduced quantity is computed only for the vertices which have a flag
// equal to "reductionFlag".
template<typename T>
class ReduceImmersedForce2D : public PlainReductiveBoxProcessingFunctional2D
{
public:
    ReduceImmersedForce2D(int reductionFlag_ = 0);
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> fields);
    virtual ReduceImmersedForce2D<T>* clone() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
    virtual BlockDomain::DomainT appliesTo() const;
    Array<T,2> getSumG() const;
private:
    Array<plint,2> sum_g_ids;
    int reductionFlag;
};

template<typename T>
Array<T,2> reduceImmersedForce(MultiContainerBlock2D& container, int reductionFlag = 0)
{
    std::vector<MultiBlock2D*> args;
    args.push_back(&container);
    ReduceImmersedForce2D<T> functional(reductionFlag);
    applyProcessingFunctional(functional, container.getBoundingBox(), args);
    return functional.getSumG();
}

/* ******** ReduceImmersedArea2D ************************************ */

// The reduced quantity is computed only for the vertices which have a flag
// equal to "reductionFlag".
template<typename T>
class ReduceImmersedArea2D : public PlainReductiveBoxProcessingFunctional2D
{
public:
    ReduceImmersedArea2D(int reductionFlag_ = 0);
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> fields);
    virtual ReduceImmersedArea2D<T>* clone() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
    virtual BlockDomain::DomainT appliesTo() const;
    T getSumArea() const;
private:
    plint sum_area_id;
    int reductionFlag;
};

template<typename T>
T reduceImmersedArea(MultiContainerBlock2D& container, int reductionFlag = 0)
{
    std::vector<MultiBlock2D*> args;
    args.push_back(&container);
    ReduceImmersedArea2D<T> functional(reductionFlag);
    applyProcessingFunctional(functional, container.getBoundingBox(), args);
    return functional.getSumArea();
}

/* ******** InamuroIteration2D ************************************ */

template<typename T, class VelFunction>
class InamuroIteration2D : public BoxProcessingFunctional2D
{
public:
    InamuroIteration2D(VelFunction velFunction_, T tau_);
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> fields);
    virtual InamuroIteration2D<T,VelFunction>* clone() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
    virtual BlockDomain::DomainT appliesTo() const;
private:
    VelFunction velFunction;
    T tau;
};

template<typename T, class VelFunction>
void inamuroIteration (
    VelFunction velFunction,
    MultiScalarField2D<T>& rhoBar,
    MultiTensorField2D<T,2>& j,
    MultiContainerBlock2D& container, T tau )
{
    std::vector<MultiBlock2D*> args;
    args.push_back(&rhoBar);
    args.push_back(&j);
    args.push_back(&container);
    applyProcessingFunctional (
        new InamuroIteration2D<T,VelFunction>(velFunction, tau), rhoBar.getBoundingBox(), args );
}

/* ******** IndexedInamuroIteration2D ************************************ */

// This is the same as InamuroIteration2D, with the difference that
// the VelFunction accepts as argument a global vertex index instead of
// a 2D position in space.
template<typename T, class VelFunction>
class IndexedInamuroIteration2D : public BoxProcessingFunctional2D
{
public:
    IndexedInamuroIteration2D(VelFunction velFunction_, T tau_);
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> fields);
    virtual IndexedInamuroIteration2D<T,VelFunction>* clone() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
    virtual BlockDomain::DomainT appliesTo() const;
private:
    VelFunction velFunction;
    T tau;
};

template<typename T, class VelFunction>
void indexedInamuroIteration (
    VelFunction velFunction,
    MultiScalarField2D<T>& rhoBar,
    MultiTensorField2D<T,2>& j,
    MultiContainerBlock2D& container, T tau )
{
    std::vector<MultiBlock2D*> args;
    args.push_back(&rhoBar);
    args.push_back(&j);
    args.push_back(&container);
    applyProcessingFunctional (
        new IndexedInamuroIteration2D<T,VelFunction>(velFunction, tau), rhoBar.getBoundingBox(), args );
}

/* ******** ConstVelInamuroIteration2D ************************************ */

template<typename T>
class ConstVelInamuroIteration2D : public BoxProcessingFunctional2D
{
public:
    ConstVelInamuroIteration2D(Array<T,2> const& wallVelocity_, T tau_);
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> fields);
    virtual ConstVelInamuroIteration2D<T>* clone() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
    virtual BlockDomain::DomainT appliesTo() const;
private:
    Array<T,2> wallVelocity;
    T tau;
};

template<typename T>
void constVelInamuroIteration (
    Array<T,2> const& wallVelocity,
    MultiScalarField2D<T>& rhoBar,
    MultiTensorField2D<T,2>& j,
    MultiContainerBlock2D& container, T tau )
{
    std::vector<MultiBlock2D*> args;
    args.push_back(&rhoBar);
    args.push_back(&j);
    args.push_back(&container);
    applyProcessingFunctional (
        new ConstVelInamuroIteration2D<T>(wallVelocity, tau), rhoBar.getBoundingBox(), args );
}



/* ******** ComputeImmersedBoundaryForce2D ************************************ */

// This functional computes the immersed boundary force on the lattice and
// stores it in a provided tensor field. This data processor must be called
// after all the immersed boundary iteretions have completed.
template<typename T>
class ComputeImmersedBoundaryForce2D : public BoxProcessingFunctional2D
{
public:
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> fields);
    virtual ComputeImmersedBoundaryForce2D<T>* clone() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
    virtual BlockDomain::DomainT appliesTo() const;
};

template<typename T>
void computeImmersedBoundaryForce2D(MultiTensorField2D<T,2>& force, MultiContainerBlock2D& container)
{
    std::vector<MultiBlock2D*> args;
    args.push_back(&force);
    args.push_back(&container);
    applyProcessingFunctional (
        new ComputeImmersedBoundaryForce2D<T>, force.getBoundingBox(), args );
}


/* ******** InstantiateImmersedWallData2D ************************************ */

template<typename T>
class InstantiateImmersedWallData2D : public BoxProcessingFunctional2D
{
public:
    InstantiateImmersedWallData2D (
            std::vector< Array<T,2> > const& vertices_,
            std::vector<T> const& areas_,
            std::vector< Array<T,2> > const& normals_);
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> fields);
    virtual InstantiateImmersedWallData2D<T>* clone() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
    virtual BlockDomain::DomainT appliesTo() const;
private:
    std::vector< Array<T,2> > const& vertices;
    std::vector<T> const& areas;
    std::vector< Array<T,2> > const& normals;
};

template<typename T>
void instantiateImmersedWallData (
            std::vector< Array<T,2> > const& vertices, std::vector<T> const& areas,
            MultiContainerBlock2D& container )
{ 
    static std::vector< Array<T,2> > dummyNormals;
    std::vector<MultiBlock2D*> args;
    args.push_back(&container);
    applyProcessingFunctional (
            new InstantiateImmersedWallData2D<T>(vertices,areas,dummyNormals), container.getBoundingBox(), args );
}

template<typename T>
void instantiateImmersedWallData (
            std::vector< Array<T,2> > const& vertices, std::vector<T> const& areas,
            std::vector< Array<T,2> > const& normals, MultiContainerBlock2D& container )
{ 
    std::vector<MultiBlock2D*> args;
    args.push_back(&container);
    applyProcessingFunctional (
            new InstantiateImmersedWallData2D<T>(vertices,areas,normals), container.getBoundingBox(), args );
}

/* ******** InstantiateImmersedWallDataWithTagging2D ************************************ */

template<typename T>
class InstantiateImmersedWallDataWithTagging2D : public BoxProcessingFunctional2D
{
public:
    InstantiateImmersedWallDataWithTagging2D (
            std::vector< Array<T,2> > const& vertices_,
            std::vector<T> const& areas_, int fluidFlag_ );
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> fields);
    virtual InstantiateImmersedWallDataWithTagging2D<T>* clone() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
    virtual BlockDomain::DomainT appliesTo() const;
private:
    std::vector< Array<T,2> > const& vertices;
    std::vector<T> const& areas;
    int fluidFlag;
};

template<typename T>
void instantiateImmersedWallDataWithTagging (
            std::vector< Array<T,2> > const& vertices, std::vector<T> const& areas,
            MultiContainerBlock2D& container, MultiScalarField2D<int>& flags, int fluidFlag )
{
    std::vector<MultiBlock2D*> args;
    args.push_back(&container);
    args.push_back(&flags);
    applyProcessingFunctional (
            new InstantiateImmersedWallDataWithTagging2D<T>(vertices,areas,fluidFlag), container.getBoundingBox(), args );
}

/* ******** InstantiateImmersedWallDataWithIndexedTagging2D ************************************ */

// This is the same as InstantiateImmersedWallDataWithTagging2D, but instead of a MultiScalarField2D
// of flags given to compute the flags of the ImmersedWallData2D, a vector of flags on every
// vertex of the immersed walls is provided.
template<typename T>
class InstantiateImmersedWallDataWithIndexedTagging2D : public BoxProcessingFunctional2D
{
public:
    InstantiateImmersedWallDataWithIndexedTagging2D (
            std::vector< Array<T,2> > const& vertices_,
            std::vector<T> const& areas_,
            std::vector<int> const& flags_ );
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> fields);
    virtual InstantiateImmersedWallDataWithIndexedTagging2D<T>* clone() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
    virtual BlockDomain::DomainT appliesTo() const;
private:
    std::vector< Array<T,2> > const& vertices;
    std::vector<T> const& areas;
    std::vector<int> const& flags;
};

template<typename T>
void instantiateImmersedWallDataWithIndexedTagging (
            std::vector< Array<T,2> > const& vertices, std::vector<T> const& areas,
            std::vector<int> const& flags, MultiContainerBlock2D& container )
{
    std::vector<MultiBlock2D*> args;
    args.push_back(&container);
    applyProcessingFunctional (
            new InstantiateImmersedWallDataWithIndexedTagging2D<T>(vertices, areas, flags),
            container.getBoundingBox(), args );
}

/* ******** TwoPhaseInamuroParam2D ************************************ */

template<typename T>
class TwoPhaseInamuroParam2D
{
public:
    TwoPhaseInamuroParam2D(std::vector<AtomicBlock2D*>& blocks, T tau_, T tau2_);
    pluint getNumVertices() const { return numVertices; }
    T area(plint i) const;
    Array<T,2>& g(plint i);
    Array<T,2> vertex(plint i) const;
    Array<T,2> absoluteVertex(plint i) const;
    Array<plint,2> intVertex(plint i) const;
    T rhoBar(plint iX, plint iY, plint iZ) const;
    Array<T,2> j(plint iX, plint iY, plint iZ) const;
    void addToJ(plint iX, plint iY, plint iZ, Array<T,2> deltaJ);
    T getTau(plint iX, plint iY, plint iZ) const;
private:
    int getFlag(plint iX, plint iY, plint iZ) const;
    pluint getGlobalVertexId(plint i) const;
private:
    ScalarField2D<T> *rhoBar_, *rhoBar2_;
    TensorField2D<T,2> *j_, *j2_;
    ScalarField2D<int>* flag_;
    ScalarField2D<T>* volumeFraction_;
    AtomicContainerBlock2D* container;
    ImmersedWallData2D<T>* wallData;
    Dot2D ofsRhoBar2, ofsJ, ofsJ2, ofsFlag, ofsVF;
    Array<T,2> absOffset;
    pluint numVertices;
    T tau, tau2;
};

/* ******** TwoPhaseInamuroIteration2D ************************************ */

template<typename T, class VelFunction>
class TwoPhaseInamuroIteration2D : public BoxProcessingFunctional2D
{
public:
    TwoPhaseInamuroIteration2D(VelFunction velFunction_, T tau_, T tau2_);
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> fields);
    virtual TwoPhaseInamuroIteration2D<T,VelFunction>* clone() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
    virtual BlockDomain::DomainT appliesTo() const;
private:
    VelFunction velFunction;
    T tau, tau2;
};

template<typename T, class VelFunction>
void twoPhaseInamuroIteration (
    VelFunction velFunction,
    MultiScalarField2D<T>& rhoBar,
    MultiScalarField2D<T>& rhoBar2,
    MultiTensorField2D<T,2>& j,
    MultiTensorField2D<T,2>& j2,
    MultiScalarField2D<int>& flag,
    MultiScalarField2D<T>& volumeFraction,
    MultiContainerBlock2D& container, T tau, T tau2 )
{
    std::vector<MultiBlock2D*> args;
    args.push_back(&rhoBar);
    args.push_back(&rhoBar2);
    args.push_back(&j);
    args.push_back(&j2);
    args.push_back(&flag);
    args.push_back(&volumeFraction);
    args.push_back(&container);
    applyProcessingFunctional (
        new TwoPhaseInamuroIteration2D<T,VelFunction>(velFunction, tau, tau2), rhoBar.getBoundingBox(), args );
}

/* ******** TwoPhaseIndexedInamuroIteration2D ************************************ */

// This is the same as TwoPhaseInamuroIteration2D, with the difference that
// the VelFunction accepts as argument a global vertex index instead of
// a 2D position in space.
template<typename T, class VelFunction>
class TwoPhaseIndexedInamuroIteration2D : public BoxProcessingFunctional2D
{
public:
    TwoPhaseIndexedInamuroIteration2D(VelFunction velFunction_, T tau_, T tau2_);
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> fields);
    virtual TwoPhaseIndexedInamuroIteration2D<T,VelFunction>* clone() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
    virtual BlockDomain::DomainT appliesTo() const;
private:
    VelFunction velFunction;
    T tau, tau2;
};

template<typename T, class VelFunction>
void twoPhaseIndexedInamuroIteration (
    VelFunction velFunction,
    MultiScalarField2D<T>& rhoBar,
    MultiScalarField2D<T>& rhoBar2,
    MultiTensorField2D<T,2>& j,
    MultiTensorField2D<T,2>& j2,
    MultiScalarField2D<int>& flag,
    MultiScalarField2D<T>& volumeFraction,
    MultiContainerBlock2D& container, T tau, T tau2 )
{
    std::vector<MultiBlock2D*> args;
    args.push_back(&rhoBar);
    args.push_back(&rhoBar2);
    args.push_back(&j);
    args.push_back(&j2);
    args.push_back(&flag);
    args.push_back(&volumeFraction);
    args.push_back(&container);
    applyProcessingFunctional (
        new TwoPhaseIndexedInamuroIteration2D<T,VelFunction>(velFunction, tau, tau2), rhoBar.getBoundingBox(), args );
}

/* ******** TwoPhaseConstVelInamuroIteration2D ************************************ */

template<typename T>
class TwoPhaseConstVelInamuroIteration2D : public BoxProcessingFunctional2D
{
public:
    TwoPhaseConstVelInamuroIteration2D(Array<T,2> const& wallVelocity_, T tau_, T tau2_);
    virtual void processGenericBlocks(Box2D domain, std::vector<AtomicBlock2D*> fields);
    virtual TwoPhaseConstVelInamuroIteration2D<T>* clone() const;
    virtual void getTypeOfModification(std::vector<modif::ModifT>& modified) const;
    virtual BlockDomain::DomainT appliesTo() const;
private:
    Array<T,2> wallVelocity;
    T tau, tau2;
};

template<typename T>
void twoPhaseConstVelInamuroIteration (
    Array<T,2> const& wallVelocity,
    MultiScalarField2D<T>& rhoBar,
    MultiScalarField2D<T>& rhoBar2,
    MultiTensorField2D<T,2>& j,
    MultiTensorField2D<T,2>& j2,
    MultiScalarField2D<int>& flag,
    MultiScalarField2D<T>& volumeFraction,
    MultiContainerBlock2D& container, T tau, T tau2 )
{
    std::vector<MultiBlock2D*> args;
    args.push_back(&rhoBar);
    args.push_back(&rhoBar2);
    args.push_back(&j);
    args.push_back(&j2);
    args.push_back(&flag);
    args.push_back(&volumeFraction);
    args.push_back(&container);
    applyProcessingFunctional (
        new TwoPhaseConstVelInamuroIteration2D<T>(wallVelocity, tau, tau2), rhoBar.getBoundingBox(), args );
}

}  // namespace plb

#endif  // IMMERSED_WALLS_2D_H

