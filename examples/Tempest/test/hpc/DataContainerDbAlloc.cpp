///////////////////////////////////////////////////////////////////////////////
///
///    \file    DataContainerTest.cpp
///    \author  Paul Ullrich, Jorge Guerra
///    \version December 18, 2013
///
///    <remarks>
///        Copyright 2000-2010 Paul Ullrich
///
///        This file is distributed as part of the Tempest source code package.
///        Permission is granted to use, copy, modify and distribute this
///        source code and its documentation under the terms of the GNU General
///        Public License.  This software is provided "as is" without express
///        or implied warranty.
///    </remarks>

#include "ocxxr-main.hpp"
#include "Tempest.h"
#include "GridPatchCartesianGLL.h"

//#include <mpi.h>
#include "ocr.h"
#include "ocr-std.h"
#include "ocr_relative_ptr.hpp"
#include "ocr_db_alloc.hpp"
#include <cstring>


#define ARENA_SIZE (1<<26) // 64MB

#ifndef NDEBUG
#define DBG_PRINTF PRINTF
#define DBG_ONLY(x) (x)
#else
#define DBG_ONLY(x) do { if (0) { (x); } } while (0)
#define DBG_PRINTF(...) DBG_ONLY(PRINTF(__VA_ARGS__))
#endif

using namespace Ocr::SimpleDbAllocator;

std::map<u64, int> guidHandle;

typedef struct
{
    ocrEdt_t FNC;
    ocrGuid_t TML;
    ocrGuid_t EDT;
    ocrGuid_t OET;
} TempestOcrTask_t;

typedef struct
{
    u64 rank;
    u64 lenGeometric;
    u64 lenActive;
    u64 lenBuffer;
    u64 lenAux;
    u64 thisStep;
    u64 nSteps;
    u64 nRanks;
    ocrEdt_t FNC;
    ocrGuid_t TML;
    ocrGuid_t EDT;
    ocrGuid_t OET;
    ocrGuid_t DONE;
} updateStateInfo_t;

typedef struct
{
    Ocr::RelPtr <Model> m;
    Ocr::RelPtr <GridCartesianGLL> g;
} MG;

ocrGuid_t updatePatch(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    int i, rank, len, dlen, step, maxStep, nRanks;

    double * localDataGeometric = (double *) depv[0].ptr;
    updateStateInfo_t * stateInfo = (updateStateInfo_t *) depv[1].ptr;

    rank = stateInfo->rank;
    len = stateInfo->lenGeometric;
    step = stateInfo->thisStep;
    maxStep = stateInfo->nSteps;
    nRanks = stateInfo->nRanks;

    // TODO: Create a single guid with list of neighbor guids; double buffer events for each worker

    dlen = len/8;
    if (len == 0) {
        PRINTF ("ERROR!\n");
        ocrAbort(-1);
    }
    DBG_PRINTF ("Rank %d in updatePatch will update %d bytes! (%d)\n", rank, len, dlen);
    double *newData;
    newData = (double *) localDataGeometric;


    void * localArena =  depv[2].ptr;
    ocrAllocatorSetDb(localArena, (size_t) ARENA_SIZE, false);
    Grid * pGrid =  ((MG *) localArena)->g;
    DBG_PRINTF("GJDEBUG: rank= %d grid pointer in updatePatch %lx\n", rank, pGrid);

    // save pre-patch allocator state
    AllocatorState alloc_state = ocrAllocatorGet().saveState();
    DBG_PRINTF("NV: [%d] offset  = %ld\n", rank, alloc_state.offset);

    // activate the patch
    GridPatch * pPatch = pGrid->ActivateEmptyPatch(rank);
    DBG_PRINTF("GJDEBUG: rank= %d active patches %d\n",  rank, pGrid->GetActivePatchCount ());

    // Get DataContainers associated with GridPatch

    DataContainer & dataGeometric = pPatch->GetDataContainerGeometric();

    unsigned char * pDataGeometric = (unsigned char*)(depv[0].ptr);


    // Proof of concept to update data in a GridPatch
    //TODO: Perform a functional update
    //TODO: Destroy neighbor events
    //TODO: unpack neighbor data
    for (i=0; i<dlen; i++) {
        newData [i] = newData [i] + (double) rank;
    }
    //TODO: pack data for neighbor
    //TODO: satisfy events for neighbors: note we need to double the events; alternate even/odd during iterations
    //
    //deactivate the GridPatch
    DBG_PRINTF ("Rank %d deactivates its patch in step %d \n", rank, step);
    pGrid->DeactivatePatch(rank);
    //TODO:create neighbor events; use the guids from list created in main

    // DEBUG print current allocator state
    AllocatorState alloc_state2 = ocrAllocatorGet().saveState();
    DBG_PRINTF("NV: [%d] offset' = %ld\n", rank, alloc_state2.offset);

    // restore pre-patch allocator state
    //ocrAllocatorGet().restoreState(alloc_state);

    // create clone for next timestep or trigger output
    if (step < maxStep) {
        stateInfo->thisStep++;
        // change to local guid for EDT
        ocrEdtCreate(&stateInfo->EDT, stateInfo->TML, EDT_PARAM_DEF, paramv,
                EDT_PARAM_DEF, NULL, EDT_PROP_NONE, NULL_HINT, NULL);
        // release data blocks
        ocrAddDependence(depv[0].guid, stateInfo->EDT, 0, DB_MODE_RW );
        ocrAddDependence(depv[1].guid, stateInfo->EDT, 1, DB_MODE_RW );
        ocrAddDependence(depv[2].guid, stateInfo->EDT, 2, DB_MODE_RW );

        //    This data is currently not being used
        //    ocrAddDependence(depv[2].guid, stateInfo->EDT, 2, DB_MODE_RW );
        //    ocrAddDependence(depv[3].guid, stateInfo->EDT, 3, DB_MODE_RW );
        //    ocrAddDependence(depv[4].guid, stateInfo->EDT, 4, DB_MODE_RW );

        // TODO: Create "magic" neighbor events
    } else {
        // TODO:  Release the data block which has been updated and is printed by outputEDT; important for x86-mpi;
        ocrDbRelease (depv[0].guid);
        ocrDbRelease (depv[1].guid);
        ocrDbRelease (depv[2].guid);
        ocrEventSatisfy (stateInfo->DONE, NULL_GUID);
        DBG_PRINTF ("Good-by from Rank %d in updatePatch\n", rank);
    }

    fflush (stdout);

    return NULL_GUID;
}

ocrGuid_t outputEdt (u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    char * localDataGeometric = (char *) depv[0].ptr;
    int nWorkers = (int) paramv [0];
    int lastStep = (int) paramv[1];
    double sum = 0.0;
    double* newData = (double *) localDataGeometric;
    int i, j;
    for (i = 0; i<nWorkers; i++) {
        double* newData = (double *) depv[i].ptr;
        PRINTF ("Output of rank %d\n", i);
        for (j = 0; j<10; j++) {
            PRINTF( "%f ", newData [j]);
        }
        if (newData [5] ==  (i + i * lastStep)) {
            PRINTF ("\n\n");
            PRINTF ("Test passed!\n");
        } else {
            PRINTF ("\n\n");
            PRINTF ("Test FAILED! %f\n", sum);
        }
        PRINTF ("\n\n");
    }
    DBG_PRINTF ("Good-by from outputEdt!\n");
    DBG_PRINTF ("***********************\n");
    fflush (stdout);
    ocrShutdown ();
    return NULL_GUID;
}

///////////////////////////////////////////////////////////////////////////////

void ocxxr::Main(ocxxr::Datablock<ocxxr::MainTaskArgs> args) {
    DBG_PRINTF("Hello from DataContainerTest-OCR!\n");
    u32 argc=args->argc();
    DBG_PRINTF("argc = %d.\n", argc);
    u32 nWorkers;
    u32 nSteps;
    if (argc != 3) {
        nWorkers = 4;
        nSteps = 4;
    } else {
        u32 i = 1;
        nWorkers = (u32) atoi(args->argv(i++));
        nSteps = (u32) atoi(args->argv(i++));
    }
    DBG_PRINTF("Using %d workers, %d steps.\n", nWorkers, nSteps);
    try {
        DBG_ONLY(std::cout << "Initializing Model and Grid ... " << std::endl);

        // Model parameters
        const int nResolutionX = 12;
        const int nResolutionY = 10;
        const int nHorizontalOrder = 4;
        const int nVerticalOrder = 4;
        const int nLevels = 40;

        double dGDim[6];
        dGDim[0] = 0.0;
        dGDim[1] = 1000.0;
        dGDim[2] = -1000.0;
        dGDim[3] = 1000.0;
        dGDim[4] = 0.0;
        dGDim[5] = 1000.0;

        const double dRefLat = 0.0;
        const double dTopoHeight = 0.0;

        Grid::VerticalStaggering eVerticalStaggering =
            Grid::VerticalStaggering_Levels;

        // Setup the Model
        // Model model(EquationSet::PrimitiveNonhydrostaticEquations);

        // Setup the Model

        // pointer to the memories serving as backups for ocrDblock to hold Model and Grids
        void *arenaPtr[nWorkers];
        MG *myMG[nWorkers];
        ocrGuid_t arenaGuid[nWorkers];
        ModelParameters param;
        for (int i = 0; i<nWorkers; i++) {
            ocrDbCreate(&arenaGuid[i], &arenaPtr[i], ARENA_SIZE, DB_PROP_NONE, NULL_HINT, NO_ALLOC);
        }
        ////////// test test test ////////////////////////
        // FIXME: I don't know why the linker fails if I don't set up this constant
        const int EQUATION_TYPE = EquationSet::PrimitiveNonhydrostaticEquations;
        //        MG testMG;
        //        void *testArenaPtr;
        //        ocrGuid_t testArenaGuid;
        //        ocrDbCreate(&testArenaGuid, &testArenaPtr, ARENA_SIZE, DB_PROP_NONE, NULL_HINT, NO_ALLOC);
        //        ocrAllocatorSetDb(testArenaPtr, (size_t) ARENA_SIZE, true);
        //        Model * testModel;
        //        testMG.m = ocrNew (Model, EQUATION_TYPE);
        //        testModel = testMG.m;
        //        assert((void*)testModel == testArenaPtr);
        Model * model [nWorkers];
        for (int i=0; i<nWorkers; i++) {
            ocrAllocatorSetDb(arenaPtr[i], (size_t) ARENA_SIZE, true);
            myMG[i] = ocrNew(MG);
            myMG[i]->m = ocrNew (Model, EQUATION_TYPE);
            model [i] = myMG[i]->m;
            assert((void*)myMG[i] == arenaPtr [i]);
            // Set the parameters
            model [i]->SetParameters(param);
            model [i]->SetHorizontalDynamics(
                    new HorizontalDynamicsStub(*model [i]));
            model [i]->SetTimestepScheme(new TimestepSchemeStrang(*model [i]));
            model [i]->SetVerticalDynamics(
                    new VerticalDynamicsStub(*model [i]));
        }

        // Set the model grid (one patch Cartesian grid)
        GridCartesianGLL * pGrid [nWorkers];
        for (int i = 0; i<nWorkers; i++) {
            ocrAllocatorSetDb(arenaPtr[i], (size_t) ARENA_SIZE, false);
            myMG[i]->g = ocrNew (GridCartesianGLL,
                    *model [i],
                    nWorkers,
                    nResolutionX,
                    nResolutionY,
                    4,
                    nHorizontalOrder,
                    nVerticalOrder,
                    nLevels,
                    dGDim,
                    dRefLat,
                    eVerticalStaggering);
            pGrid [i] = myMG[i]->g;

        }
        // Create testGrid in a data block

        //GridCartesianGLL * testGrid;
        //myMG.g = ocrNew (GridCartesianGLL, *testModel, nWorkers, nResolutionX, nResolutionY, 4,
        //                  nHorizontalOrder, nVerticalOrder, nLevels, dGDim, dRefLat, eVerticalStaggering);
        //testGrid = myMG.g;
        //testGrid->ApplyDefaultPatchLayout(nWorkers);
        // const PatchBox & testBox = testGrid->GetPatchBox (0);
        // auto testPatch = GridPatchCartesianGLL(*testGrid, 0, testBox, nHorizontalOrder, nVerticalOrder);
        // int testIndex = testPatch.GetPatchIndex ();
        // testPatch.InitializeDataLocal(false, false, false, false);
        // DataContainer & testGeometric = testPatch.GetDataContainerGeometric();
        // std::cout << "testPatch.Geometric Size:   " << testGeometric.GetTotalByteSize() << " bytes" << std::endl;
        // Apply the default patch layout
        for (int i = 0; i<nWorkers; i++) {
            ocrAllocatorSetDb(arenaPtr[i], (size_t) ARENA_SIZE, false);
            pGrid[i]->ApplyDefaultPatchLayout(nWorkers);
        }

        //////////////////////////////////////////////////////////////////
        // BEGIN MAIN PROGRAM BLOCK
        //DBG_PRINTF("GJDEBUG: grid pointer in mainEdt %lx\n", pGrid);

        // Create a GridPatch
        GridPatch *pPatchFirst [nWorkers];
        for (int i = 0; i<nWorkers; i++) {
            ocrAllocatorSetDb(arenaPtr[i], (size_t) ARENA_SIZE, false);
            const PatchBox & box = pGrid[i]->GetPatchBox(i);
            assert(&box != nullptr);
            pPatchFirst [i] =
                Ocr::New<GridPatchCartesianGLL>(
                        (*pGrid [i]),
                        0,
                        box,
                        nHorizontalOrder,
                        nVerticalOrder);
        }

        // Build the DataContainer object for the GridPatch objects

        ocrGuid_t dataGuidGeometric [nWorkers];
        ocrGuid_t dataGuidActiveState [nWorkers];
        ocrGuid_t dataGuidBufferState [nWorkers];
        ocrGuid_t dataGuidAuxiliary [nWorkers];
        ocrGuid_t dataGuidState [nWorkers];
        ocrGuid_t dataGuidGrid [1];

        ocrGuid_t updateEdtTemplate;
        ocrGuid_t outputEventGuid [nWorkers];
        ocrGuid_t updatePatch_DONE [nWorkers];
        updateStateInfo_t stateInfo [nWorkers];
        TempestOcrTask_t updatePatch_t [nWorkers];
        TempestOcrTask_t output_t;

        // Craete Template for output Edt
        // Parameters:
        //      # workers
        //      # steps
        // Dependences:
        //      # workers * updated data blocks
        //      # workers * DONE events
        output_t.FNC = outputEdt;
        ocrEdtTemplateCreate(&output_t.TML, output_t.FNC, 2, 2*nWorkers);
        u64 output_paramv [2];
        output_paramv [0] = (u64) nWorkers;
        output_paramv [1] = (u64) nSteps;

        // Create the output EDT:
        ocrEdtCreate(&output_t.EDT, output_t.TML, EDT_PARAM_DEF, output_paramv, EDT_PARAM_DEF, NULL, EDT_PROP_FINISH, NULL_HINT, NULL );


        for (int i = 0; i<nWorkers; i++) {
            pPatchFirst[i]->InitializeDataLocal(false, false, false, false);
            // Get DataContainers associated with GridPatch
            DataContainer & dataGeometric = pPatchFirst[i]->GetDataContainerGeometric();
            DataContainer & dataActiveState = pPatchFirst[i]->GetDataContainerActiveState();
            DataContainer & dataBufferState = pPatchFirst[i]->GetDataContainerBufferState();
            DataContainer & dataAuxiliary = pPatchFirst[i]->GetDataContainerAuxiliary();

            // Output the size requirements (in bytes) of each DataContainer
            DBG_ONLY(std::cout << "GridPatch.Geometric Size:   " << dataGeometric.GetTotalByteSize() << " bytes" << std::endl);
            DBG_ONLY(std::cout << "GridPatch.ActiveState Size: " << dataActiveState.GetTotalByteSize() << " bytes" << std::endl);
            DBG_ONLY(std::cout << "GridPatch.BufferState Size: " << dataBufferState.GetTotalByteSize() << " bytes" << std::endl);
            DBG_ONLY(std::cout << "GridPatch.Auxiliary Size:   " << dataAuxiliary.GetTotalByteSize() << " bytes" << std::endl);

            // Allocate data
            DBG_ONLY(std::cout << "Allocating data ... " << std::endl);
            unsigned char * pDataGeometric;
            unsigned char * pDataActiveState;
            unsigned char * pDataBufferState;
            unsigned char * pDataAuxiliary;
            updateStateInfo_t * pStateInfo;
            updatePatch_t[i].FNC = updatePatch;

            // Craete Template for updatePatch Edt
            // Parameters:
            //       1 pointer to global grid
            // Dependences:3
            //      updated geometric data block
            //      state information
            //      arena holding grid+model data
            //      TODO: Add dependences on "magic" neighbor events to handle the data exchange; 1 event per neighbor

            ocrEdtTemplateCreate(&updatePatch_t[i].TML, updatePatch_t[i].FNC, 0, 3);

            ocrDbCreate (&dataGuidState [i], (void **)&pStateInfo, (u64) (sizeof(updateStateInfo_t)), 0, NULL_HINT, NO_ALLOC);
            ocrDbCreate (&dataGuidGeometric[i], (void **)&pDataGeometric, (u64) dataGeometric.GetTotalByteSize(), 0, NULL_HINT, NO_ALLOC);

            // These containers are currently not being used for the update since we are only performing a proof of concept update

            ocrDbCreate (&dataGuidActiveState[i], (void **)&pDataActiveState, (u64) dataActiveState.GetTotalByteSize(), 0, NULL_HINT, NO_ALLOC);
            ocrDbCreate (&dataGuidBufferState[i], (void **)&pDataBufferState, (u64) dataBufferState.GetTotalByteSize(), 0, NULL_HINT, NO_ALLOC);
            ocrDbCreate (&dataGuidAuxiliary[i], (void **)&pDataAuxiliary, (u64) dataAuxiliary.GetTotalByteSize(), 0, NULL_HINT, NO_ALLOC);

            // Initialize data to zero
            memset(pDataGeometric, 0, dataGeometric.GetTotalByteSize());
            memset(pDataActiveState, 0, dataActiveState.GetTotalByteSize());
            memset(pDataBufferState, 0, dataBufferState.GetTotalByteSize());
            memset(pDataAuxiliary, 0, dataAuxiliary.GetTotalByteSize());

            // Each updatePatch EDT gets stateInfo: rank, sizes, time step information, information how to create next Edt
            int thisStep = 0;
            pStateInfo->rank = i;
            pStateInfo->lenGeometric = (u64) dataGeometric.GetTotalByteSize();
            pStateInfo->lenActive = (u64) dataActiveState.GetTotalByteSize();
            pStateInfo->lenBuffer = (u64) dataBufferState.GetTotalByteSize();
            pStateInfo->lenAux = (u64) dataAuxiliary.GetTotalByteSize();
            pStateInfo->thisStep = (u64) thisStep;
            pStateInfo->nSteps = (u64) nSteps;
            pStateInfo->nRanks = (u64) nWorkers;
            pStateInfo->EDT =  updatePatch_t[i].EDT;
            pStateInfo->TML =  updatePatch_t[i].TML;
            pStateInfo->FNC =  updatePatch_t[i].FNC;


            ocrEventCreate( &updatePatch_DONE [i], OCR_EVENT_STICKY_T, false );
            pStateInfo->DONE =  updatePatch_DONE [i];

            //u64 update_paramv [1] = {(u64) pGrid [i] };
            ocrEdtCreate(&updatePatch_t[i].EDT, updatePatch_t[i].TML, EDT_PARAM_DEF, NULL,
                    EDT_PARAM_DEF, NULL, EDT_PROP_NONE, NULL_HINT, NULL);

        }
        // add data dependences for updatePatch EDT
        //
        s32 _idep;
        for (int i=0; i<nWorkers; i++) {
            _idep = 0;
            ocrAddDependence(dataGuidGeometric[i], updatePatch_t[i].EDT, _idep++, DB_MODE_RW );
            ocrAddDependence(dataGuidState[i], updatePatch_t[i].EDT, _idep++, DB_MODE_RW );
            ocrAddDependence(arenaGuid[i], updatePatch_t[i].EDT, _idep++, DB_MODE_RW );
            // This data is currently not being used, since our update is just a proof of concept
            //ocrAddDependence(dataGuidActiveState[i], updatePatch_t[i].EDT, _idep++, DB_MODE_RW );
            //ocrAddDependence(dataGuidBufferState[i], updatePatch_t[i].EDT, _idep++, DB_MODE_RW );
            //ocrAddDependence(dataGuidAuxiliary[i], updatePatch_t[i].EDT, _idep++, DB_MODE_RW );
        }

        _idep = 0;
        for (int i=0; i<nWorkers; i++) {
            ocrAddDependence( dataGuidGeometric [i], output_t.EDT, _idep++, DB_MODE_RW );
        }
        // chain output EDT with all of the updatePatch EDTs
        for (int i=0; i<nWorkers; i++) {
            ocrAddDependence( updatePatch_DONE [i], output_t.EDT, _idep++, DB_MODE_NULL );
        }


    } catch(Exception & e) {
        std::cout << e.ToString() << std::endl;
    }

    // Deinitialize Tempest
    //MPI_Finalize();
}

///////////////////////////////////////////////////////////////////////////////
