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

#ifndef BUBBLE_MATCH_3D_HH
#define BUBBLE_MATCH_3D_HH

#include "multiPhysics/bubbleMatch3D.h"
#include "offLattice/makeSparse3D.h"
#include "parallelism/mpiManager.h"
#include "atomicBlock/reductiveDataProcessingFunctional3D.h"
#include "multiBlock/reductiveMultiDataProcessorWrapper3D.h"
#include "atomicBlock/atomicContainerBlock3D.h"
#include "atomicBlock/dataProcessingFunctional3D.h"
#include "multiPhysics/freeSurfaceUtil3D.h"


namespace plb {


template<typename T>
void BubbleMatch3D::execute(MultiScalarField3D<int>& flag, MultiScalarField3D<T>& volumeFraction)
{
    bubbleBucketFill(flag);
    pluint numBubbles = countAndTagBubbles();
    bubbleVolume.clear();
    bubbleCenter.clear();
    bubbleAnalysis(flag, volumeFraction, numBubbles);
}

template<typename T>
void BubbleMatch3D::bubbleAnalysis( MultiScalarField3D<int>& flag,
                                    MultiScalarField3D<T>& volumeFraction, pluint numBubbles )
{
    std::vector<MultiBlock3D*> args;
    args.push_back(tagMatrix);
    args.push_back(&flag);
    args.push_back(bubbleAnalysisContainer);
    args.push_back(&volumeFraction);
    applyProcessingFunctional( new AnalyzeBubbles3D<T>(numBubbles, matchEmpty),
                               bubbleAnalysisContainer->getBoundingBox(), args );
    computeBubbleData(numBubbles);
}

/* *************** Class AnalyzeBubbles3D ******************************** */

template<typename T>
AnalyzeBubbles3D<T>::AnalyzeBubbles3D(pluint numBubbles_, bool matchEmpty_)
    : numBubbles(numBubbles_),
      matchEmpty(matchEmpty_)
{ }

template<typename T>
AnalyzeBubbles3D<T>* AnalyzeBubbles3D<T>::clone() const {
    return new AnalyzeBubbles3D<T>(*this);
}

template<typename T>
void AnalyzeBubbles3D<T>::processGenericBlocks(Box3D domain,std::vector<AtomicBlock3D*> atomicBlocks)
{
    PLB_ASSERT(atomicBlocks.size()==4);
    ScalarField3D<plint>* pTagMatrix = dynamic_cast<ScalarField3D<plint>*> (atomicBlocks[0]);
    PLB_ASSERT(pTagMatrix);
    ScalarField3D<plint>& tagMatrix = *pTagMatrix;

    ScalarField3D<int>* pFlagMatrix = dynamic_cast<ScalarField3D<int>*> (atomicBlocks[1]);
    PLB_ASSERT(pFlagMatrix);
    ScalarField3D<int>& flagMatrix = *pFlagMatrix;

    AtomicContainerBlock3D* pDataBlock = dynamic_cast<AtomicContainerBlock3D*> (atomicBlocks[2]);
    PLB_ASSERT(pDataBlock);
    AtomicContainerBlock3D& dataBlock = *pDataBlock;
    BubbleAnalysisData3D* pData = dynamic_cast<BubbleAnalysisData3D*>(dataBlock.getData());
    PLB_ASSERT(pData);
    BubbleAnalysisData3D& data = *pData;

    ScalarField3D<T>* pVolumeFraction = dynamic_cast<ScalarField3D<T>*>(atomicBlocks[3]);
    PLB_ASSERT( pVolumeFraction );
    ScalarField3D<T>& volumeFraction = *pVolumeFraction;

    Dot3D flagOffset = computeRelativeDisplacement(tagMatrix, flagMatrix);
    Dot3D vfOffset = computeRelativeDisplacement(tagMatrix, volumeFraction);
    Dot3D absOfs = tagMatrix.getLocation();

    std::vector<double> bubbleVolume(numBubbles);
    std::fill(bubbleVolume.begin(), bubbleVolume.end(), 0.);
    std::vector<Array<double,3> > bubbleCenter(numBubbles);
    std::fill(bubbleCenter.begin(), bubbleCenter.end(), Array<double,3>(0.,0.,0.));

    for (plint iX=domain.x0; iX<=domain.x1; ++iX) {
        for (plint iY=domain.y0; iY<=domain.y1; ++iY) {
            for (plint iZ=domain.z0; iZ<=domain.z1; ++iZ) {         
                plint tag = tagMatrix.get(iX,iY,iZ);
                if (tag>=0)
                {
                    if ( (matchEmpty && flagMatrix.get(iX+flagOffset.x,iY+flagOffset.y,iZ+flagOffset.z)==twoPhaseFlag::empty) ||
                         (!matchEmpty && flagMatrix.get(iX+flagOffset.x,iY+flagOffset.y,iZ+flagOffset.z)==twoPhaseFlag::fluid) )
                    {
                        PLB_ASSERT( tag < (plint)bubbleVolume.size() );
                        bubbleVolume[tag] += 1.0;
                        bubbleCenter[tag] += Array<double,3>((double)iX+absOfs.x,(double)iY+absOfs.y,(double)iZ+absOfs.z);
                    }
                    else if (flagMatrix.get(iX+flagOffset.x,iY+flagOffset.y,iZ+flagOffset.z)==twoPhaseFlag::interface) {
                        PLB_ASSERT( tag < (plint)bubbleVolume.size() );
                        double vf = (double)volumeFraction.get(iX+vfOffset.x,iY+vfOffset.y,iZ+vfOffset.z);
                        if (matchEmpty) {
                            vf = 1.0 - vf;
                        }
                        bubbleVolume[tag] += vf;
                        bubbleCenter[tag] += vf*Array<double,3>((double)iX+absOfs.x,(double)iY+absOfs.y,(double)iZ+absOfs.z);
                    }
                    else {
                        PLB_ASSERT( false );
                    }
                }
            }
        }
    }

    data.bubbleVolume = bubbleVolume;
    data.bubbleCenter = bubbleCenter;
}

}  // namespace plb

#endif  // BUBBLE_MATCH_3D_HH

