/**CFile****************************************************************

  FileName    [intM114.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Interpolation engine.]

  Synopsis    [Intepolation using ABC's proof generator added to MiniSat-1.14c.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 24, 2008.]

  Revision    [$Id: intM114.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "intInt.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Returns the SAT solver for one interpolation run.]

  Description [pInter is the previous interpolant. pAig is one time frame.
  pFrames is the unrolled time frames.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
sat_solver * Inter_ManDeriveSatSolver( 
    Aig_Man_t * pInter, Cnf_Dat_t * pCnfInter, 
    Aig_Man_t * pAig, Cnf_Dat_t * pCnfAig, 
    Aig_Man_t * pFrames, Cnf_Dat_t * pCnfFrames, 
    Vec_Int_t * vVarsAB, int fUseBackward )
{
    sat_solver * pSat;
    Aig_Obj_t * pObj, * pObj2;
    int i, Lits[2];

//Aig_ManDumpBlif( pInter,  "out_inter.blif", NULL, NULL );
//Aig_ManDumpBlif( pAig,    "out_aig.blif", NULL, NULL );
//Aig_ManDumpBlif( pFrames, "out_frames.blif", NULL, NULL );

    // sanity checks
    assert( Aig_ManRegNum(pInter) == 0 );
    assert( Aig_ManRegNum(pAig) > 0 );
    assert( Aig_ManRegNum(pFrames) == 0 );
    assert( Aig_ManPoNum(pInter) == 1 );
    assert( Aig_ManPoNum(pFrames) == fUseBackward? Saig_ManRegNum(pAig) : 1 );
    assert( fUseBackward || Aig_ManPiNum(pInter) == Aig_ManRegNum(pAig) );
//    assert( (Aig_ManPiNum(pFrames) - Aig_ManRegNum(pAig)) % Saig_ManPiNum(pAig) == 0 );

    // prepare CNFs
    Cnf_DataLift( pCnfAig,   pCnfFrames->nVars );
    Cnf_DataLift( pCnfInter, pCnfFrames->nVars + pCnfAig->nVars );

    // start the solver
    pSat = sat_solver_new();
    sat_solver_store_alloc( pSat );
    sat_solver_setnvars( pSat, pCnfInter->nVars + pCnfAig->nVars + pCnfFrames->nVars );

    // add clauses of A
    // interpolant
    for ( i = 0; i < pCnfInter->nClauses; i++ )
    {
        if ( !sat_solver_addclause( pSat, pCnfInter->pClauses[i], pCnfInter->pClauses[i+1] ) )
        {
            sat_solver_delete( pSat );
            // return clauses to the original state
            Cnf_DataLift( pCnfAig, -pCnfFrames->nVars );
            Cnf_DataLift( pCnfInter, -pCnfFrames->nVars -pCnfAig->nVars );
            return NULL;
        }
    }
    // connector clauses
    if ( fUseBackward )
    {
        Saig_ManForEachLi( pAig, pObj2, i )
        {
            if ( Saig_ManRegNum(pAig) == Aig_ManPiNum(pInter) )
                pObj = Aig_ManPi( pInter, i );
            else
            {
                assert( Aig_ManPiNum(pAig) == Aig_ManPiNum(pInter) );
                pObj = Aig_ManPi( pInter, Aig_ManPiNum(pAig)-Saig_ManRegNum(pAig) + i );
            }

            Lits[0] = toLitCond( pCnfInter->pVarNums[pObj->Id], 0 );
            Lits[1] = toLitCond( pCnfAig->pVarNums[pObj2->Id], 1 );
            if ( !sat_solver_addclause( pSat, Lits, Lits+2 ) )
                assert( 0 );
            Lits[0] = toLitCond( pCnfInter->pVarNums[pObj->Id], 1 );
            Lits[1] = toLitCond( pCnfAig->pVarNums[pObj2->Id], 0 );
            if ( !sat_solver_addclause( pSat, Lits, Lits+2 ) )
                assert( 0 );
        }
    }
    else
    {
        Aig_ManForEachPi( pInter, pObj, i )
        {
            pObj2 = Saig_ManLo( pAig, i );

            Lits[0] = toLitCond( pCnfInter->pVarNums[pObj->Id], 0 );
            Lits[1] = toLitCond( pCnfAig->pVarNums[pObj2->Id], 1 );
            if ( !sat_solver_addclause( pSat, Lits, Lits+2 ) )
                assert( 0 );
            Lits[0] = toLitCond( pCnfInter->pVarNums[pObj->Id], 1 );
            Lits[1] = toLitCond( pCnfAig->pVarNums[pObj2->Id], 0 );
            if ( !sat_solver_addclause( pSat, Lits, Lits+2 ) )
                assert( 0 );
        }
    }
    // one timeframe
    for ( i = 0; i < pCnfAig->nClauses; i++ )
    {
        if ( !sat_solver_addclause( pSat, pCnfAig->pClauses[i], pCnfAig->pClauses[i+1] ) )
            assert( 0 );
    }
    // connector clauses
    Vec_IntClear( vVarsAB );
    if ( fUseBackward )
    {
        Aig_ManForEachPo( pFrames, pObj, i )
        {
            assert( pCnfFrames->pVarNums[pObj->Id] >= 0 );
            Vec_IntPush( vVarsAB, pCnfFrames->pVarNums[pObj->Id] );

            pObj2 = Saig_ManLo( pAig, i );
            Lits[0] = toLitCond( pCnfFrames->pVarNums[pObj->Id], 0 );
            Lits[1] = toLitCond( pCnfAig->pVarNums[pObj2->Id], 1 );
            if ( !sat_solver_addclause( pSat, Lits, Lits+2 ) )
                assert( 0 );
            Lits[0] = toLitCond( pCnfFrames->pVarNums[pObj->Id], 1 );
            Lits[1] = toLitCond( pCnfAig->pVarNums[pObj2->Id], 0 );
            if ( !sat_solver_addclause( pSat, Lits, Lits+2 ) )
                assert( 0 );
        }
    }
    else
    {
        Aig_ManForEachPi( pFrames, pObj, i )
        {
            if ( i == Aig_ManRegNum(pAig) )
                break;
            Vec_IntPush( vVarsAB, pCnfFrames->pVarNums[pObj->Id] );

            pObj2 = Saig_ManLi( pAig, i );
            Lits[0] = toLitCond( pCnfFrames->pVarNums[pObj->Id], 0 );
            Lits[1] = toLitCond( pCnfAig->pVarNums[pObj2->Id], 1 );
            if ( !sat_solver_addclause( pSat, Lits, Lits+2 ) )
                assert( 0 );
            Lits[0] = toLitCond( pCnfFrames->pVarNums[pObj->Id], 1 );
            Lits[1] = toLitCond( pCnfAig->pVarNums[pObj2->Id], 0 );
            if ( !sat_solver_addclause( pSat, Lits, Lits+2 ) )
                assert( 0 );
        }
    }
    // add clauses of B
    sat_solver_store_mark_clauses_a( pSat );
    for ( i = 0; i < pCnfFrames->nClauses; i++ )
    {
        if ( !sat_solver_addclause( pSat, pCnfFrames->pClauses[i], pCnfFrames->pClauses[i+1] ) )
        {
            pSat->fSolved = 1;
            break;
        }
    }
    sat_solver_store_mark_roots( pSat );
    // return clauses to the original state
    Cnf_DataLift( pCnfAig, -pCnfFrames->nVars );
    Cnf_DataLift( pCnfInter, -pCnfFrames->nVars -pCnfAig->nVars );
    return pSat;
}

/**Function*************************************************************

  Synopsis    [Performs one SAT run with interpolation.]

  Description [Returns 1 if proven. 0 if failed. -1 if undecided.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Inter_ManPerformOneStep( Inter_Man_t * p, int fUseBias, int fUseBackward )
{
    sat_solver * pSat;
    void * pSatCnf = NULL;
    Inta_Man_t * pManInterA; 
//    Intb_Man_t * pManInterB; 
    int * pGlobalVars;
    int clk, status, RetValue;
    int i, Var;
//    assert( p->pInterNew == NULL );

    // derive the SAT solver
    pSat = Inter_ManDeriveSatSolver( p->pInter, p->pCnfInter, p->pAigTrans, p->pCnfAig, p->pFrames, p->pCnfFrames, p->vVarsAB, fUseBackward );
    if ( pSat == NULL )
    {
        p->pInterNew = NULL;
        return 1;
    }

    // collect global variables
    pGlobalVars = CALLOC( int, sat_solver_nvars(pSat) );
    Vec_IntForEachEntry( p->vVarsAB, Var, i )
        pGlobalVars[Var] = 1;
    pSat->pGlobalVars = fUseBias? pGlobalVars : NULL;

    // solve the problem
clk = clock();
    status = sat_solver_solve( pSat, NULL, NULL, (sint64)p->nConfLimit, (sint64)0, (sint64)0, (sint64)0 );
    p->nConfCur = pSat->stats.conflicts;
p->timeSat += clock() - clk;

    pSat->pGlobalVars = NULL;
    FREE( pGlobalVars );
    if ( status == l_False )
    {
        pSatCnf = sat_solver_store_release( pSat );
        RetValue = 1;
    }
    else if ( status == l_True )
    {
        RetValue = 0;
    } 
    else
    {
        RetValue = -1;
    }
    sat_solver_delete( pSat );
    if ( pSatCnf == NULL )
        return RetValue;

    // create the resulting manager
clk = clock();
/*
    if ( !fUseIp )
    {
        pManInterA = Inta_ManAlloc();
        p->pInterNew = Inta_ManInterpolate( pManInterA, pSatCnf, p->vVarsAB, 0 );
        Inta_ManFree( pManInterA );
    }
    else
    {
        Aig_Man_t * pInterNew2;
        int RetValue;

        pManInterA = Inta_ManAlloc();
        p->pInterNew = Inta_ManInterpolate( pManInterA, pSatCnf, p->vVarsAB, 0 );
//        Inter_ManVerifyInterpolant1( pManInterA, pSatCnf, p->pInterNew );
        Inta_ManFree( pManInterA );

        pManInterB = Intb_ManAlloc();
        pInterNew2 = Intb_ManInterpolate( pManInterB, pSatCnf, p->vVarsAB, 0 );
        Inter_ManVerifyInterpolant2( pManInterB, pSatCnf, pInterNew2 );
        Intb_ManFree( pManInterB );

        // check relationship
        RetValue = Inter_ManCheckEquivalence( pInterNew2, p->pInterNew );
        if ( RetValue )
            printf( "Equivalence \"Ip == Im\" holds\n" );
        else
        {
//            printf( "Equivalence \"Ip == Im\" does not hold\n" );
            RetValue = Inter_ManCheckContainment( pInterNew2, p->pInterNew );
            if ( RetValue )
                printf( "Containment \"Ip -> Im\" holds\n" );
            else
                printf( "Containment \"Ip -> Im\" does not hold\n" );

            RetValue = Inter_ManCheckContainment( p->pInterNew, pInterNew2 );
            if ( RetValue )
                printf( "Containment \"Im -> Ip\" holds\n" );
            else
                printf( "Containment \"Im -> Ip\" does not hold\n" );
        }

        Aig_ManStop( pInterNew2 );
    }
*/

    pManInterA = Inta_ManAlloc();
    p->pInterNew = Inta_ManInterpolate( pManInterA, pSatCnf, p->vVarsAB, 0 );
    Inta_ManFree( pManInterA );

p->timeInt += clock() - clk;
    Sto_ManFree( pSatCnf );
    return RetValue;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


