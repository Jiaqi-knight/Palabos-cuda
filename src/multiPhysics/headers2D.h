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

/** \file
 * Groups all the 2D include files in the directory multiPhysics.
 */


#include "multiPhysics/advectionDiffusion2D.h"
#include "multiPhysics/boussinesqThermalProcessor2D.h"
#include "multiPhysics/bubbleHistory2D.h"
#include "multiPhysics/bubbleMatch2D.h"
#include "multiPhysics/createBubbles2D.h"
#include "multiPhysics/freeSurfaceAnalysis2D.h"
#include "multiPhysics/freeSurfaceBoundaryCondition2D.h"
#include "multiPhysics/freeSurfaceInitializer2D.h"
#include "multiPhysics/freeSurfaceModel2D.h"
#include "multiPhysics/freeSurfaceUtil2D.h"
#include "multiPhysics/heLeeProcessor2D.h"
#include "multiPhysics/interparticlePotential.h"
#include "multiPhysics/multiFreeSurfaceModel2D.h"
#include "multiPhysics/REVHelperFunctional2D.h"
#include "multiPhysics/REVHelperWrapper2D.h"
#include "multiPhysics/REVProcessor2D.h"
#include "multiPhysics/shanChenLattices2D.h"
#include "multiPhysics/shanChenProcessor2D.h"
#include "multiPhysics/thermalDataAnalysis2D.h"
#include "multiPhysics/twoPhaseModel2D.h"
#include "multiPhysics/twoPhaseModel.h"
