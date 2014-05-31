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

#ifndef BUBBLE_HISTORY_2D_HH
#define BUBBLE_HISTORY_2D_HH

#include "multiPhysics/bubbleHistory2D.h"
#include "offLattice/makeSparse2D.h"
#include "parallelism/mpiManager.h"
#include "atomicBlock/reductiveDataProcessingFunctional2D.h"
#include "atomicBlock/atomicContainerBlock2D.h"
#include "atomicBlock/dataProcessingFunctional2D.h"
#include "multiPhysics/freeSurfaceUtil2D.h"
#include <limits>


namespace plb
{

/* ************** class BubbleHistory2D ********************************** */

template<typename T>
BubbleHistory2D<T>::BubbleHistory2D ( MultiBlock2D& templ )
    : bubbleAnalysisContainer ( createContainerBlock ( templ, new BubbleAnalysisData2D() ) ),
      bubbleCorrelationContainer ( createContainerBlock ( templ, new BubbleCorrelationData2D() ) ),
      bubbleRemapContainer ( createContainerBlock ( templ, new BubbleRemapData2D() ) ),
      mpiData ( templ ),
      oldTagMatrix ( new MultiScalarField2D<plint> ( templ ) ),
      nextBubbleID ( 0 )
{
    setToConstant ( *oldTagMatrix, oldTagMatrix->getBoundingBox(), ( plint )-1 );
}

template<typename T>
BubbleHistory2D<T>::~BubbleHistory2D()
{
    delete bubbleAnalysisContainer;
    delete bubbleCorrelationContainer;
    delete bubbleRemapContainer;
    delete oldTagMatrix;
}

template<typename T>
void BubbleHistory2D<T>::transition ( BubbleMatch2D& bubbleMatch, plint iterationStep, T newBubbleVolumeCorrection )
{

    matchAndRemapBubbles ( bubbleMatch, *oldTagMatrix, *bubbleMatch.getTagMatrix(), newBubbleVolumeCorrection, iterationStep );

    // Get the tag-matrix from bubbleMatch, and store it as internal oldTagMatrix for next iteration.
    MultiScalarField2D<plint>* newTagMatrix = bubbleMatch.getTagMatrix();
    bubbleMatch.setTagMatrix ( oldTagMatrix );
    oldTagMatrix = newTagMatrix;
}

template<typename T>
void BubbleHistory2D<T>::updateBubblePressure ( MultiScalarField2D<T>& outsideDensity, T rhoEmpty )
{
    applyProcessingFunctional (
        new UpdateBubblePressure2D<T> ( bubbles, rhoEmpty ),
        oldTagMatrix->getBoundingBox(), *oldTagMatrix, outsideDensity );
}

template<typename T>
void BubbleHistory2D<T>::freeze()
{
    typename std::map<plint,BubbleInfo2D>::iterator it = bubbles.begin();
    for ( ; it!=bubbles.end(); ++it )
    {
        plint bubbleID = it->first;
        it->second.freeze();
        PLB_ASSERT ( ( plint ) fullBubbleRecord.size() >bubbleID );
        fullBubbleRecord[bubbleID].frozen = true;
    }
}

template<typename T>
void BubbleHistory2D<T>::timeHistoryLog ( std::string fName )
{
    plb_ofstream ofile ( fName.c_str() );
    std::map<plint, std::pair<std::vector<plint>,std::vector<plint> > >::const_iterator
    it = timeHistory.begin();
    for ( ; it!=timeHistory.end(); ++it )
    {
        plint timeStep = it->first;
        std::pair<std::vector<plint>,std::vector<plint> > log = it->second;
        ofile << "Iteration " << std::setw ( 8 ) << timeStep << ".  ";
        if ( !log.first.empty() )
        {
            ofile << "Created bubble(s) with ID";
            for ( pluint i=0; i<log.first.size(); ++i )
            {
                ofile << " " << log.first[i];
            }
            ofile << ".";
        }
        if ( !log.second.empty() )
        {
            ofile << " Removed bubble(s) with ID";
            for ( pluint i=0; i<log.second.size(); ++i )
            {
                ofile << " " << log.second[i];
            }
            ofile << ".";
        }
        ofile << std::endl;
    }
}

template<typename T>
void BubbleHistory2D<T>::fullBubbleLog ( std::string fName )
{
    plb_ofstream ofile ( fName.c_str() );
    for ( pluint i=0; i<fullBubbleRecord.size(); ++i )
    {
        ofile << fullBubbleRecord[i].description ( i );
        ofile << std::endl;
    }
}

template<typename T>
void BubbleHistory2D<T>::matchAndRemapBubbles (
    BubbleMatch2D& bubbleMatch, MultiScalarField2D<plint>& tagMatrix1,
    MultiScalarField2D<plint>& tagMatrix2, T newBubbleVolumeCorrection, plint iterationStep )
{
    pluint numBubbles = bubbleMatch.numBubbles();
    // The following data processor creates a correspondance between new bubbles and all overlapping old bubbles.
    // This is the basis of the upcoming contrived algorithm.
    std::vector<std::vector<plint> > newToAllOldMap;
    correlateBubbleIds ( tagMatrix1, tagMatrix2, newToAllOldMap, numBubbles );

    // First, the map is inverted, to get a correspondence between old bubbles and all overlapping new bubbles.
    std::map<plint,std::vector<plint> > oldToAllNewMap;
    typename std::map<plint,BubbleInfo2D>::const_iterator itOld = bubbles.begin();
    for ( ; itOld!=bubbles.end(); ++itOld )
    {
        plint oldID = itOld->first;
        oldToAllNewMap.insert (
            std::pair<plint,std::vector<plint> > ( oldID, std::vector<plint>() ) );
    }
    for ( plint newID=0; newID< ( plint ) newToAllOldMap.size(); ++newID )
    {
        for ( pluint i=0; i<newToAllOldMap[newID].size(); ++i )
        {
            plint oldID = newToAllOldMap[newID][i];
            typename std::map<plint,std::vector<plint> >::iterator it = oldToAllNewMap.find ( oldID );
            PLB_ASSERT ( it!=oldToAllNewMap.end() );
            it->second.push_back ( newID );
        }
    }

    // Then, a transition pattern between groups of old and new bubbles is determined.
    std::vector<BubbleTransition2D> bubbleTransitions;
    computeBubbleTransitions ( newToAllOldMap, oldToAllNewMap, bubbleTransitions );


    std::vector<plint> bubbleFinalRemap ( newToAllOldMap.size() );
    plint maxInt = std::numeric_limits<plint>::max();
    for ( pluint i=0; i<bubbleFinalRemap.size(); ++i )
    {
        plint minTag = maxInt;
        for ( pluint j=0; j<newToAllOldMap[i].size(); ++j )
        {
            if ( newToAllOldMap[i][j] < minTag )
            {
                minTag = newToAllOldMap[i][j];
            }
        }
        bubbleFinalRemap[i] = minTag;
    }

    updateBubbleInformation ( bubbleTransitions, bubbleMatch.getBubbleVolume(), bubbleMatch.getBubbleCenter(),
                              newBubbleVolumeCorrection, iterationStep );

    std::vector<MultiBlock2D*> args2;
    args2.push_back ( &tagMatrix2 );
    args2.push_back ( bubbleRemapContainer );
    applyProcessingFunctional (
        new ApplyTagRemap2D(), bubbleRemapContainer->getBoundingBox(), args2 );
}

template<typename T>
void BubbleHistory2D<T>::updateBubbleInformation (
    std::vector<BubbleTransition2D>& bubbleTransitions,
    std::vector<double> const& bubbleVolume, std::vector<Array<double,2> > const& bubbleCenter,
    T newBubbleVolumeCorrection, plint iterationStep )
{
    std::map<plint,BubbleInfo2D> newBubbles;
    std::map<plint,plint> newToFinal;
    for ( pluint i=0; i<bubbleTransitions.size(); ++i )
    {
        std::set<plint>& oldIDs = bubbleTransitions[i].oldIDs;
        std::set<plint>& newIDs = bubbleTransitions[i].newIDs;
        computeNewBubbles ( oldIDs, newIDs, bubbleVolume, bubbleCenter, newBubbleVolumeCorrection, newBubbles, newToFinal );
        updateBubbleLog ( bubbleTransitions[i], bubbleVolume, iterationStep, newBubbles, newToFinal );
    }

    // Prepare the data for remapping the bubbles.
    std::vector<plint> const& localIds = mpiData.getLocalIds();
    for ( pluint i=0; i<localIds.size(); ++i )
    {
        plint id = localIds[i];
        AtomicContainerBlock2D& atomicDataContainer = bubbleRemapContainer->getComponent ( id );
        BubbleRemapData2D* pData = dynamic_cast<BubbleRemapData2D*> ( atomicDataContainer.getData() );
        PLB_ASSERT ( pData );
        BubbleRemapData2D& data = *pData;
        data.getTagRemap() =newToFinal;
    }
    bubbles.swap ( newBubbles );
}

template<typename T>
void BubbleHistory2D<T>::computeNewBubbles (
    std::set<plint>& oldIDs, std::set<plint>& newIDs,
    std::vector<double> const& bubbleVolume, std::vector<Array<double,2> > const& bubbleCenter,
    T newBubbleVolumeCorrection, std::map<plint,BubbleInfo2D>& newBubbles, std::map<plint,plint>& newToFinal )
{
    PLB_ASSERT ( bubbleVolume.size() ==bubbleCenter.size() );
    // A bubble is created from nothing.
    if ( oldIDs.empty() )
    {
        // Create a new bubble.
        PLB_ASSERT ( newIDs.size() ==1 );
        plint newID = *newIDs.begin();
        newBubbles[nextBubbleID] = BubbleInfo2D ( bubbleVolume[newID]*newBubbleVolumeCorrection, bubbleCenter[newID] );
        newToFinal[newID] = nextBubbleID;
        ++nextBubbleID;
    }
    // A bubble vanishes into nothing.
    else if ( newIDs.empty() )
    {
        PLB_ASSERT ( oldIDs.size() ==1 );
    }
    // All other cases: normal transition, splitting, merging.
    else
    {
        PLB_ASSERT ( oldIDs.size() >=1 && newIDs.size() >=1 );

        // First check the old bubbles: their total initial volume will be distributed
        // as initial volume of the new bubbles. If one of the old bubbles is frozen,
        // then so are the new bubbles.
        // And while we're at it, also close the history record for these old bubbles.
        T totalReferenceVolume = T();
        bool isFrozen = false;
        std::set<plint>::const_iterator itOld = oldIDs.begin();
        for ( ; itOld!=oldIDs.end(); ++itOld )
        {
            plint oldID = *itOld;
            typename std::map<plint,BubbleInfo2D>::const_iterator it = bubbles.find ( oldID );
            PLB_ASSERT ( it!=bubbles.end() );
            totalReferenceVolume += it->second.getReferenceVolume();
            isFrozen = isFrozen || it->second.isFrozen();
        }

        // Then, compute the total volume of the new bubbles.
        T newTotalVolume = T();
        std::set<plint>::const_iterator itNew = newIDs.begin();
        for ( ; itNew!=newIDs.end(); ++itNew )
        {
            plint newID = *itNew;
            PLB_ASSERT ( newID<= ( plint ) bubbleVolume.size() );
            newTotalVolume += bubbleVolume[newID];
        }

        // Finally, put into place all information of the new bubbles.
        static const T epsilon = std::numeric_limits<T>::epsilon() *1.e4;
        itNew = newIDs.begin();
        for ( ; itNew!=newIDs.end(); ++itNew )
        {
            plint newID = *itNew;
            PLB_ASSERT ( newID<= ( plint ) bubbleVolume.size() );
            T newInitialVolume = bubbleVolume[newID];
            if ( fabs ( newTotalVolume ) >epsilon )
            {
                newInitialVolume *= totalReferenceVolume/newTotalVolume;
            }

            plint finalID = newID;
            // In case of normal transition (no splitting nor merging), the
            // ID is inherited from the previous bubble.
            if ( newIDs.size() ==1 && oldIDs.size() ==1 )
            {
                finalID = *oldIDs.begin();
            }
            // Otherwise, a new ID is generated.
            else
            {
                finalID = nextBubbleID;
                ++nextBubbleID;
            }

            BubbleInfo2D nextBubble ( newInitialVolume, bubbleCenter[newID] );
            nextBubble.setVolume ( bubbleVolume[newID] );
            if ( isFrozen )
            {
                nextBubble.freeze();
            }

            newToFinal[newID] = finalID;
            newBubbles[finalID] = nextBubble;
        }
    }
}

template<typename T>
void BubbleHistory2D<T>::updateBubbleLog (
    BubbleTransition2D& bubbleTransition, std::vector<double> const& bubbleVolume, plint iterationStep,
    std::map<plint,BubbleInfo2D>& newBubbles, std::map<plint,plint>& newToFinal )
{
    std::set<plint>& oldIDs = bubbleTransition.oldIDs;
    std::set<plint>& newIDs = bubbleTransition.newIDs;
    // Update the bubble-transition to point to the final ID (instead of
    // newID), wo it is properly written into the log.
    std::set<plint> save_newIDs;
    for ( std::set<plint>::iterator it=newIDs.begin(); it!=newIDs.end(); ++it )
    {
        save_newIDs.insert ( newToFinal[*it] );
    }
    save_newIDs.swap ( newIDs );

    // For purposes of history, keep a full record of the bubble (start off the record).
    // Furthermore, keep a time-record of the fact that a bubble was created.
    //
    // A bubble is created from nothing.
    if ( oldIDs.empty() )
    {
        PLB_ASSERT ( newIDs.size() ==1 );
        plint newID = *save_newIDs.begin();
        FullBubbleRecord nextBubbleRecord ( bubbleVolume[newID], iterationStep );
        nextBubbleRecord.beginTransition = bubbleTransition;
        fullBubbleRecord.push_back ( nextBubbleRecord );
        timeHistory[iterationStep].first.push_back ( newToFinal[newID] );
    }
    // A bubble vanishes into nothing.
    else if ( newIDs.empty() )
    {
        PLB_ASSERT ( oldIDs.size() ==1 );
        plint vanishedID = *oldIDs.begin();
        PLB_ASSERT ( ( plint ) fullBubbleRecord.size() >vanishedID );
        fullBubbleRecord[vanishedID].endIteration = iterationStep;
        fullBubbleRecord[vanishedID].finalVolume = 0.;
        fullBubbleRecord[vanishedID].endTransition = bubbleTransition;
        timeHistory[iterationStep].second.push_back ( vanishedID );
    }
    // Splitting or merging. Exclude the case of normal transitions.
    else if ( ! ( newIDs.size() ==1 && oldIDs.size() ==1 ) )
    {
        std::set<plint>::const_iterator itOld = oldIDs.begin();
        // Finish the record for all old bubbles that now vanish.
        for ( ; itOld!=oldIDs.end(); ++itOld )
        {
            plint oldID = *itOld;
            typename std::map<plint,BubbleInfo2D>::const_iterator it = bubbles.find ( oldID );
            PLB_ASSERT ( it!=bubbles.end() );
            PLB_ASSERT ( ( plint ) fullBubbleRecord.size() >oldID );
            fullBubbleRecord[oldID].endIteration = iterationStep;
            fullBubbleRecord[oldID].finalVolume = it->second.getVolume();
            fullBubbleRecord[oldID].endTransition = bubbleTransition;
            timeHistory[iterationStep].second.push_back ( oldID );
        }
        // Introduce a record for all newly created bubbles.
        std::set<plint>::const_iterator itNew = save_newIDs.begin();
        for ( ; itNew!=save_newIDs.end(); ++itNew )
        {
            plint newID = *itNew;
            FullBubbleRecord nextBubbleRecord ( bubbleVolume[newID], iterationStep );
            nextBubbleRecord.beginTransition = bubbleTransition;
            nextBubbleRecord.frozen = newBubbles[newToFinal[newID]].isFrozen();
            fullBubbleRecord.push_back ( nextBubbleRecord );
            timeHistory[iterationStep].first.push_back ( newToFinal[newID] );
        }
    }
}

template<typename T>
void BubbleHistory2D<T>::correlateBubbleIds (
    MultiScalarField2D<plint>& tagMatrix1, MultiScalarField2D<plint>& tagMatrix2,
    std::vector<std::vector<plint> >& newToAllOldMap, pluint numBubbles )
{
    std::vector<plint> const& localIds = mpiData.getLocalIds();
    plint maxInt = std::numeric_limits<plint>::max();

    // Reset the vector for the return value.
    newToAllOldMap = std::vector<std::vector<plint> > ( numBubbles );

    // Initialization: prepare the new-to-old maps in the containers of the
    // local MPI threads.
    for ( pluint i=0; i<localIds.size(); ++i )
    {
        plint id = localIds[i];
        AtomicContainerBlock2D& atomicDataContainer = bubbleCorrelationContainer->getComponent ( id );
        BubbleCorrelationData2D* pData = dynamic_cast<BubbleCorrelationData2D*> ( atomicDataContainer.getData() );
        PLB_ASSERT ( pData );
        BubbleCorrelationData2D& data = *pData;

        data.newToOldMap0.resize ( numBubbles );
        data.newToOldMap1.resize ( numBubbles );
        // newToOldMap0 holds the "previous iteration": initialize it to -1
        // to make sure it is smaller than any actually occurring tag.
        std::fill ( data.newToOldMap0.begin(), data.newToOldMap0.end(), -1 );
        // newToOldMap1 will take the tags from the first iteration. Initialize
        // it to maxInt in view of the coming MIN reduction.
        std::fill ( data.newToOldMap1.begin(), data.newToOldMap1.end(), maxInt );
    }

    // The number of iterations in the coming loop will be N+1, where N is the maximum
    // number of bubbles from the physical time step t-1 that coalesced to form a bubble
    // at time t. Very often, no coalescence occurs and N is just 1.
    bool hasConverged = false;
    while ( !hasConverged )
    {
        std::vector<MultiBlock2D*> args;
        args.push_back ( &tagMatrix1 );
        args.push_back ( &tagMatrix2 );
        args.push_back ( bubbleCorrelationContainer );
        // Correlate the new bubbles with the next group of old bubbles.
        applyProcessingFunctional (
            new CorrelateBubbleIds2D<T>, bubbleCorrelationContainer->getBoundingBox(), args );

        std::vector<plint> newToOldMap ( numBubbles, maxInt );

        // Apply the MIN reduction to all atomic-blocks that are on the current MPI thread.
        for ( pluint i=0; i<localIds.size(); ++i )
        {
            plint id = localIds[i];
            AtomicContainerBlock2D& atomicDataContainer = bubbleCorrelationContainer->getComponent ( id );
            BubbleCorrelationData2D* pData = dynamic_cast<BubbleCorrelationData2D*> ( atomicDataContainer.getData() );
            PLB_ASSERT ( pData );
            BubbleCorrelationData2D& data = *pData;

            PLB_ASSERT ( data.newToOldMap0.size() == numBubbles );
            PLB_ASSERT ( data.newToOldMap1.size() == numBubbles );

            for ( pluint i=0; i<numBubbles; ++i )
            {
                newToOldMap[i] = std::min ( newToOldMap[i], data.newToOldMap1[i] );
            }
        }

        // Apply the MIN reduction globally.
#ifdef PLB_MPI_PARALLEL
        global::mpi().allReduceVect ( newToOldMap, MPI_MIN );
#endif

        // If new bubbles where found, add them to the return value. If not, abort the
        // iterations.
        bool newBubbleDiscovered = false;
        for ( pluint i=0; i<numBubbles; ++i )
        {
            if ( newToOldMap[i] < maxInt )
            {
                newBubbleDiscovered = true;
                newToAllOldMap[i].push_back ( newToOldMap[i] );
            }
        }
        hasConverged = !newBubbleDiscovered;

        // Assign the result of the global reduction to all local atomic-blocks, to be
        // used for the next iteration.
        for ( pluint i=0; i<localIds.size(); ++i )
        {
            plint id = localIds[i];
            AtomicContainerBlock2D& atomicDataContainer = bubbleCorrelationContainer->getComponent ( id );
            BubbleCorrelationData2D* pData = dynamic_cast<BubbleCorrelationData2D*> ( atomicDataContainer.getData() );
            PLB_ASSERT ( pData );
            BubbleCorrelationData2D& data = *pData;

            // Advance by one iteration: assign the globally reduced data to newToOldMap0
            // ("the previous iteration").
            data.newToOldMap0.assign ( newToOldMap.begin(), newToOldMap.end() );
            // Prepare newToOldMap1 for the next iteration, and especially for the coming
            // MIN reduction, by initializing to maxInt.
            std::fill ( data.newToOldMap1.begin(), data.newToOldMap1.end(), maxInt );
        }
    }
}


/* *************** Class CorrelateBubbleIds2D ******************************** */

template<typename T>
CorrelateBubbleIds2D<T>* CorrelateBubbleIds2D<T>::clone() const
{
    return new CorrelateBubbleIds2D<T> ( *this );
}


template<typename T>
void CorrelateBubbleIds2D<T>::processGenericBlocks ( Box2D domain,std::vector<AtomicBlock2D*> atomicBlocks )
{
    PLB_ASSERT ( atomicBlocks.size() ==3 );

    ScalarField2D<plint>* pTagMatrix1 = dynamic_cast<ScalarField2D<plint>*> ( atomicBlocks[0] );
    PLB_ASSERT ( pTagMatrix1 );
    ScalarField2D<plint>& tagMatrix1 = *pTagMatrix1;

    ScalarField2D<plint>* pTagMatrix2 = dynamic_cast<ScalarField2D<plint>*> ( atomicBlocks[1] );
    PLB_ASSERT ( pTagMatrix2 );
    ScalarField2D<plint>& tagMatrix2 = *pTagMatrix2;

    AtomicContainerBlock2D* pDataBlock = dynamic_cast<AtomicContainerBlock2D*> ( atomicBlocks[2] );
    PLB_ASSERT ( pDataBlock );
    AtomicContainerBlock2D& dataBlock = *pDataBlock;
    BubbleCorrelationData2D* pData = dynamic_cast<BubbleCorrelationData2D*> ( dataBlock.getData() );
    PLB_ASSERT ( pData );
    BubbleCorrelationData2D& data = *pData;

    Dot2D tag2Ofs = computeRelativeDisplacement ( tagMatrix1, tagMatrix2 );

    pluint numNewBubbles = data.newToOldMap0.size();
    PLB_ASSERT ( data.newToOldMap1.size() ==numNewBubbles );

    for ( plint iX=domain.x0; iX<=domain.x1; ++iX )
    {
        for ( plint iY=domain.y0; iY<=domain.y1; ++iY )
        {
            plint newBubbleTag = tagMatrix2.get ( iX+tag2Ofs.x,iY+tag2Ofs.y );
            if ( newBubbleTag>=0 )
            {
                plint oldBubbleTag = tagMatrix1.get ( iX,iY );
                if ( oldBubbleTag>=0 )
                {
                    PLB_ASSERT ( newBubbleTag< ( plint ) numNewBubbles );
                    // At this step of the iteration (corresponding to a call to the present
                    // data processor), we're only targeting tags that are larger than the
                    // ones from the previous step.
                    if ( oldBubbleTag>data.newToOldMap0[newBubbleTag] )
                    {
                        // Searching the minimum among all tags that match the condition.
                        data.newToOldMap1[newBubbleTag] = std::min ( data.newToOldMap1[newBubbleTag],oldBubbleTag );
                    }
                }
            }
        }
    }
}

/* *************** Class UpdateBubblePressure2D ******************************** */

template<typename T>
UpdateBubblePressure2D<T>::UpdateBubblePressure2D ( std::map<plint,BubbleInfo2D> const& bubbles_, T rho0_ )
    : bubbles ( bubbles_ ),
      rho0 ( rho0_ )
{ }

template<typename T>
UpdateBubblePressure2D<T>* UpdateBubblePressure2D<T>::clone() const
{
    return new UpdateBubblePressure2D<T> ( *this );
}

template<typename T>
void UpdateBubblePressure2D<T>::process ( Box2D domain, ScalarField2D<plint>& tags, ScalarField2D<T>& density )
{
    Dot2D ofs = computeRelativeDisplacement ( tags, density );
    for ( plint iX=domain.x0; iX<=domain.x1; ++iX )
    {
        for ( plint iY=domain.y0; iY<=domain.y1; ++iY )
        {
            plint tag = tags.get ( iX,iY );
            if ( tag>=0 )
            {
                typename std::map<plint,BubbleInfo2D>::const_iterator it = bubbles.find ( tag );
                PLB_ASSERT ( it!=bubbles.end() );
                BubbleInfo2D const& info = it->second;
                T rho = rho0* ( T ) info.getVolumeRatio();
                if ( rho>1.2*rho0 )
                {
                    rho = 1.2*rho0;
                }
                density.get ( iX+ofs.x,iY+ofs.y ) = rho;
            }
            else
            {
                density.get ( iX+ofs.x,iY+ofs.y ) = rho0;
            }
        }
    }
}

template<typename T>
void BubbleHistory2D<T>::computeBubbleTransitions (
    std::vector<std::vector<plint> > const& newToAllOldMap,
    std::map<plint,std::vector<plint> > const& oldToAllNewMap, std::vector<BubbleTransition2D>& bubbleTransitions )
{
    std::vector<bool> checked ( newToAllOldMap.size() );
    std::fill ( checked.begin(), checked.end(), false );
    for ( pluint newId=0; newId<newToAllOldMap.size(); ++newId )
    {
        std::set<plint> newCandidates, oldCandidates;
        BubbleTransition2D transition;
        if ( !checked[newId] )
        {
            newCandidates.insert ( newId );
        }
        while ( ! ( newCandidates.empty() && oldCandidates.empty() ) )
        {
            if ( !newCandidates.empty() )
            {
                std::set<plint>::iterator it1 = newCandidates.begin();
                plint newCandidate = *it1;
                newCandidates.erase ( it1 );

                std::set<plint>::iterator it2 = transition.newIDs.find ( newCandidate );
                // Treat this ID only if it is has not already been treated.
                if ( it2==transition.newIDs.end() &&!checked[newCandidate] )
                {
                    transition.newIDs.insert ( newCandidate );
                    checked[newCandidate]=true;

                    PLB_ASSERT ( newCandidate< ( plint ) newToAllOldMap.size() );
                    std::vector<plint> oldIds = newToAllOldMap[newCandidate];
                    oldCandidates.insert ( oldIds.begin(), oldIds.end() );
                }
            }
            else if ( !oldCandidates.empty() )
            {
                std::set<plint>::iterator it1 = oldCandidates.begin();
                plint oldCandidate = *it1;
                oldCandidates.erase ( it1 );

                std::set<plint>::iterator it2 = transition.oldIDs.find ( oldCandidate );
                // Treat this ID only if it is has not already been treated.
                if ( it2==transition.oldIDs.end() )
                {
                    transition.oldIDs.insert ( oldCandidate );

                    std::map<plint,std::vector<plint> >::const_iterator it2 = oldToAllNewMap.find ( oldCandidate );
                    PLB_ASSERT ( it2!=oldToAllNewMap.end() );
                    std::vector<plint> newIds = it2->second;
                    newCandidates.insert ( newIds.begin(), newIds.end() );
                }
            }
        }
        if ( !transition.empty() )
        {
            bubbleTransitions.push_back ( transition );
        }
    }

    std::map<plint,std::vector<plint> >::const_iterator it = oldToAllNewMap.begin();
    for ( ; it!=oldToAllNewMap.end(); ++it )
    {
        if ( it->second.empty() )
        {
            BubbleTransition2D transition;
            transition.oldIDs.insert ( it->first );
            bubbleTransitions.push_back ( transition );
        }
    }
}

}  // namespace plb

#endif  // BUBBLE_HISTORY_2D_HH

