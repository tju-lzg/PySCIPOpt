/*
 * This file is based on the "branch_fullstrong.c" file from the software
 * SCIP --- Solving Constraint Integer Programs, distributed by
 * Konrad-Zuse-Zentrum fuer Informationstechnik Berlin. For the original
 * source code and further information visit scip.zib.de.
 */

/**@file   branch_fullstrong_vanilla.c
 * @brief  full strong LP branching rule, vanilla version
 * @author Maxime Gasse
 */

#include "blockmemshell/memory.h"
#include "branch_fullstrong_vanilla.h"
#include "scip/pub_branch.h"
#include "scip/pub_message.h"
#include "scip/pub_tree.h"
#include "scip/pub_var.h"
#include "scip/scip_branch.h"
#include "scip/scip_general.h"
#include "scip/scip_lp.h"
#include "scip/scip_mem.h"
#include "scip/scip_message.h"
#include "scip/scip_numerics.h"
#include "scip/scip_param.h"
#include "scip/scip_prob.h"
#include "scip/scip_solvingstats.h"
#include "scip/scip_tree.h"
#include "scip/scip_var.h"
#include "scip/lp.h"
#include <string.h>


#define BRANCHRULE_NAME          "fullstrong-vanilla"
#define BRANCHRULE_DESC          "full strong branching vanilla"
#define BRANCHRULE_PRIORITY      -1
#define BRANCHRULE_MAXDEPTH      -1
#define BRANCHRULE_MAXBOUNDDIST  1.0

#define DEFAULT_FORCESTRONGBRANCH TRUE      /**< should strong branching be evaluated for all candidates no matter what? */


/** branching rule data */
struct SCIP_BranchruleData
{
   SCIP_Bool             forcestrongbranch;  /**< should strong branching be evaluated for all candidates no matter what ? */
   int                   scoresize;
   SCIP_Real*            latestscores;
   SCIP_Bool*            validscores;
};


/*
 * Callback methods
 */

/** copy method for branchrule plugins (called when SCIP copies plugins) */
static
SCIP_DECL_BRANCHCOPY(branchCopyFullstrong)
{  /*lint --e{715}*/
   assert(scip != NULL);
   assert(branchrule != NULL);
   assert(strcmp(SCIPbranchruleGetName(branchrule), BRANCHRULE_NAME) == 0);

   /* call inclusion method of branchrule */
   SCIP_CALL( SCIPincludeBranchruleFullstrongVanilla(scip) );

   return SCIP_OKAY;
}

/** destructor of branching rule to free user data (called when SCIP is exiting) */
static
SCIP_DECL_BRANCHFREE(branchFreeFullstrong)
{  /*lint --e{715}*/
   SCIP_BRANCHRULEDATA* branchruledata;

   /* free branching rule data */
   branchruledata = SCIPbranchruleGetData(branchrule);
   assert(branchruledata != NULL);

   SCIPfreeBlockMemoryArrayNull(scip, &branchruledata->latestscores, branchruledata->scoresize);
   SCIPfreeBlockMemoryArrayNull(scip, &branchruledata->validscores, branchruledata->scoresize);

   SCIPfreeBlockMemory(scip, &branchruledata);
   SCIPbranchruleSetData(branchrule, NULL);

   return SCIP_OKAY;
}


/** initialization method of branching rule (called after problem was transformed) */
static
SCIP_DECL_BRANCHINIT(branchInitFullstrong)
{  /*lint --e{715}*/

   return SCIP_OKAY;
}

/** deinitialization method of branching rule (called before transformed problem is freed) */
static
SCIP_DECL_BRANCHEXIT(branchExitFullstrong)
{  /*lint --e{715}*/

   return SCIP_OKAY;
}

/** gets strong branching information on column variable with fractional value
 *
 *  Before calling this method, the strong branching mode must have been activated by calling SCIPstartStrongbranch();
 *  after strong branching was done for all candidate variables, the strong branching mode must be ended by
 *  SCIPendStrongbranch(). Since this method does not apply domain propagation before strongbranching,
 *  propagation should not be enabled in the SCIPstartStrongbranch() call.
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 *  @pre This method can be called if @p scip is in one of the following stages:
 *       - \ref SCIP_STAGE_PRESOLVED
 *       - \ref SCIP_STAGE_SOLVING
 */
SCIP_RETCODE SCIPgetVarStrongbranchFracVanilla(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_VAR*             var,                /**< variable to get strong branching values for */
   int                   itlim,              /**< iteration limit for strong branchings */
   SCIP_Real*            down,               /**< stores dual bound after branching column down */
   SCIP_Real*            up,                 /**< stores dual bound after branching column up */
   SCIP_Bool*            downvalid,          /**< stores whether the returned down value is a valid dual bound, or NULL;
                                              *   otherwise, it can only be used as an estimate value */
   SCIP_Bool*            upvalid,            /**< stores whether the returned up value is a valid dual bound, or NULL;
                                              *   otherwise, it can only be used as an estimate value */
   SCIP_Bool*            lperror             /**< pointer to store whether an unresolved LP error occurred or the
                                              *   solving process should be stopped (e.g., due to a time limit) */
   )
{
   SCIP_COL* col;

   assert(scip != NULL);
   assert(var != NULL);
   assert(lperror != NULL);
   assert(!SCIPtreeProbing(scip->tree)); /* we should not be in strong branching with propagation mode */
   assert(var->scip == scip);

   SCIP_CALL( SCIPcheckStage(scip, "SCIPgetVarStrongbranchFrac", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE) );

   if( downvalid != NULL )
      *downvalid = FALSE;
   if( upvalid != NULL )
      *upvalid = FALSE;

   if( SCIPvarGetStatus(var) != SCIP_VARSTATUS_COLUMN )
   {
      SCIPerrorMessage("cannot get strong branching information on non-COLUMN variable <%s>\n", SCIPvarGetName(var));
      return SCIP_INVALIDDATA;
   }

   col = SCIPvarGetCol(var);
   assert(col != NULL);

   if( !SCIPcolIsInLP(col) )
   {
      SCIPerrorMessage("cannot get strong branching information on variable <%s> not in current LP\n", SCIPvarGetName(var));
      return SCIP_INVALIDDATA;
   }

   /* check if the solving process should be aborted */
   if( SCIPsolveIsStopped(scip->set, scip->stat, FALSE) )
   {
      /* mark this as if the LP failed */
      *lperror = TRUE;
      return SCIP_OKAY;
   }

   /* call strong branching for column with fractional value */
   SCIP_CALL( SCIPcolGetStrongbranch(col, FALSE, scip->set, scip->stat, scip->transprob, scip->lp, itlim,
         down, up, downvalid, upvalid, lperror) );

   return SCIP_OKAY;
}

/**
 * Selects a variable from a set of candidates by strong branching
 *
 *  @return \ref SCIP_OKAY is returned if everything worked. Otherwise a suitable error code is passed. See \ref
 *          SCIP_Retcode "SCIP_RETCODE" for a complete list of error codes.
 *
 * @note The variables in the lpcands array must have a fractional value in the current LP solution
 */
SCIP_RETCODE SCIPselectVarStrongBranchingVanilla(
   SCIP*                 scip,               /**< original SCIP data structure                        */
   SCIP_VAR**            lpcands,            /**< branching candidates                                */
   SCIP_Real*            lpcandssol,         /**< solution values of the branching candidates         */
   SCIP_Real*            lpcandsfrac,        /**< fractional values of the branching candidates       */
   int                   nlpcands,           /**< number of branching candidates                      */
   int                   npriolpcands,       /**< number of priority branching candidates             */
   SCIP_Bool             forcestrongbranch,  /**< should strong branching be evaluated for all candidates no matter what? */
   int*                  bestcand,           /**< best candidate for branching                        */
   SCIP_Real*            bestdown,           /**< objective value of the down branch for bestcand     */
   SCIP_Real*            bestup,             /**< objective value of the up branch for bestcand       */
   SCIP_Real*            bestscore,          /**< score for bestcand                                  */
   SCIP_Bool*            bestdownvalid,      /**< is bestdown a valid dual bound for the down branch? */
   SCIP_Bool*            bestupvalid,        /**< is bestup a valid dual bound for the up branch?     */
   SCIP_Real*            provedbound,        /**< proved dual bound for current subtree               */
   SCIP_Real*            latestscores,
   SCIP_Bool*            validscores,
   SCIP_RESULT*          result              /**< result pointer                                      */
   )
{  /*lint --e{715}*/
   SCIP_Real down;
   SCIP_Real up;
   SCIP_Real downgain;
   SCIP_Real upgain;
   SCIP_Real score;
   SCIP_Real lpobjval;
   SCIP_Bool lperror;
   SCIP_Bool downvalid;
   SCIP_Bool upvalid;
   SCIP_Bool downinf;
   SCIP_Bool upinf;
   int c;

   SCIP_Bool besthasinf;

   assert(scip != NULL);
   assert(lpcands != NULL);
   assert(lpcandssol != NULL);
   assert(lpcandsfrac != NULL);
   assert(bestcand != NULL);
   assert(bestdown != NULL);
   assert(bestup != NULL);
   assert(bestscore != NULL);
   assert(bestdownvalid != NULL);
   assert(bestupvalid != NULL);
   assert(provedbound != NULL);
   assert(result != NULL);
   assert(nlpcands > 0);

   /* get current LP objective bound of the local sub problem and global cutoff bound */
   lpobjval = SCIPgetLPObjval(scip);
   *provedbound = lpobjval;

   *bestcand = 0;
   *bestdown = lpobjval;
   *bestup = lpobjval;
   *bestdownvalid = TRUE;
   *bestupvalid = TRUE;
   *bestscore = -SCIPinfinity(scip);

   /* if only one candidate exists, choose this one without applying strong branching; also, when SCIP is about to be
    * stopped, all strongbranching evaluations will be aborted anyway, thus we can return immediately
    */
   if( (!forcestrongbranch && nlpcands == 1) || SCIPisStopped(scip) )
      return SCIP_OKAY;

   /* this assert may not hold if SCIP is stopped, thus we only check it here */
   assert(SCIPgetLPSolstat(scip) == SCIP_LPSOLSTAT_OPTIMAL);

    /* initialize strong branching without propagation */
   SCIP_CALL( SCIPstartStrongbranch(scip, FALSE) );


   /* empty the score buffer */
   for( c = 0; c < nlpcands; ++c)
   {
       latestscores[c] = -SCIPinfinity(scip);
       validscores[c] = FALSE;
   }

   /* search the full strong candidate
    * cycle through the candidates
    */
   score = *bestscore;
   besthasinf = FALSE;
   for( c = 0; c < nlpcands; ++c )
   {
      assert(lpcands[c] != NULL);

      SCIPdebugMsg(scip, "applying strong branching on variable <%s> with solution %g\n",
         SCIPvarGetName(lpcands[c]), lpcandssol[c]);

      /* apply strong branching */
      up = -SCIPinfinity(scip);
      down = -SCIPinfinity(scip);

      SCIP_CALL( SCIPgetVarStrongbranchFracVanilla(scip, lpcands[c], INT_MAX,
         &down, &up, &downvalid, &upvalid, &lperror) );

      /* check for an error in strong branching */
      if( lperror )
      {
         SCIPverbMessage(scip, SCIP_VERBLEVEL_HIGH, NULL,
            "(node %" SCIP_LONGINT_FORMAT ") error in strong branching call for variable <%s> with solution %g\n",
            SCIPgetNNodes(scip), SCIPvarGetName(lpcands[c]), lpcandssol[c]);
         break;
      }

      /* evaluate strong branching */
      down = MAX(down, lpobjval);
      up = MAX(up, lpobjval);
      downgain = down - lpobjval;
      upgain = up - lpobjval;
      downinf = downvalid && SCIPisGE(scip, down, SCIPgetCutoffbound(scip));
      upinf = upvalid && SCIPisGE(scip, up, SCIPgetCutoffbound(scip));

      /* check if there are infeasible roundings */
      if( downinf && upinf )
      {
         score = SCIPinfinity(scip);
         SCIPdebugMsg(scip, " -> variable <%s> is infeasible in both directions\n", SCIPvarGetName(lpcands[c]));
      }
      else if( downinf )
      {
         score = upgain * SCIPvarGetBranchFactor(lpcands[c]);
         SCIPdebugMsg(scip, " -> variable <%s> is infeasible in downward branch\n", SCIPvarGetName(lpcands[c]));
      }
      else if( upinf )
      {
         score = downgain * SCIPvarGetBranchFactor(lpcands[c]);
         SCIPdebugMsg(scip, " -> variable <%s> is infeasible in upward branch\n", SCIPvarGetName(lpcands[c]));
      }
      else
      {
         score = SCIPgetBranchScore(scip, lpcands[c], downgain, upgain);
         validscores[c] = TRUE;
      }

      /* Update score buffer */
      latestscores[c] = score;

      if( ( score > *bestscore && ( downinf || upinf || !besthasinf ) ) || ( !besthasinf && ( downinf || upinf ) ) )
      {
         *bestcand = c;
         *bestdown = down;
         *bestup = up;
         *bestdownvalid = downvalid;
         *bestupvalid = upvalid;
         *bestscore = score;

         if( downinf || upinf )
         {
            besthasinf = TRUE;
         }

         if( !forcestrongbranch && downinf && upinf )
         {
            break;
         }
      }

      SCIPdebugMsg(scip, " -> cand %d/%d (prio:%d) var <%s> (solval=%g, downgain=%g, upgain=%g, score=%g) -- best: <%s> (%g)\n",
         c, nlpcands, npriolpcands, SCIPvarGetName(lpcands[c]), lpcandssol[c], downgain, upgain, score,
         SCIPvarGetName(lpcands[*bestcand]), *bestscore);
   }

   /* end strong branching */
   SCIP_CALL( SCIPendStrongbranch(scip) );

   return SCIP_OKAY;
}

/** branching execution method for fractional LP solutions */
static
SCIP_DECL_BRANCHEXECLP(branchExeclpFullstrong)
{  /*lint --e{715}*/
   SCIP_BRANCHRULEDATA* branchruledata;
   SCIP_VAR** tmplpcands;
   SCIP_VAR** lpcands;
   SCIP_Real* tmplpcandssol;
   SCIP_Real* lpcandssol;
   SCIP_Real* tmplpcandsfrac;
   SCIP_Real* lpcandsfrac;
   SCIP_Real bestdown;
   SCIP_Real bestup;
   SCIP_Real bestscore;
   SCIP_Real provedbound;
   SCIP_Bool bestdownvalid;
   SCIP_Bool bestupvalid;
   int nlpcands;
   int npriolpcands;
   int bestcand;

   assert(branchrule != NULL);
   assert(strcmp(SCIPbranchruleGetName(branchrule), BRANCHRULE_NAME) == 0);
   assert(scip != NULL);
   assert(result != NULL);

   SCIPdebugMsg(scip, "Execlp method of fullstrong branching\n");

   *result = SCIP_DIDNOTRUN;

   /* get branching rule data */
   branchruledata = SCIPbranchruleGetData(branchrule);
   assert(branchruledata != NULL);

   /* get branching candidates */
   SCIP_CALL( SCIPgetLPBranchCands(scip, &tmplpcands, &tmplpcandssol, &tmplpcandsfrac, &nlpcands, &npriolpcands, NULL) );
   assert(nlpcands > 0);
   assert(npriolpcands > 0);

   /* copy LP banching candidates and solution values, because they will be updated w.r.t. the strong branching LP
    * solution
    */
   SCIP_CALL( SCIPduplicateBufferArray(scip, &lpcands, tmplpcands, nlpcands) );
   SCIP_CALL( SCIPduplicateBufferArray(scip, &lpcandssol, tmplpcandssol, nlpcands) );
   SCIP_CALL( SCIPduplicateBufferArray(scip, &lpcandsfrac, tmplpcandsfrac, nlpcands) );

   /* initialize branching rule data */
   assert((branchruledata->latestscores != NULL) == (branchruledata->validscores != NULL));

   if( branchruledata->latestscores != NULL )
   {
      SCIPfreeBlockMemoryArray(scip, &branchruledata->latestscores, branchruledata->scoresize);
      SCIPfreeBlockMemoryArray(scip, &branchruledata->validscores, branchruledata->scoresize);
   }

   branchruledata->scoresize = nlpcands;
   SCIP_CALL( SCIPallocBlockMemoryArray(scip, &branchruledata->latestscores, branchruledata->scoresize) );
   SCIP_CALL( SCIPallocBlockMemoryArray(scip, &branchruledata->validscores, branchruledata->scoresize) );
   BMSclearMemoryArray(branchruledata->latestscores, branchruledata->scoresize);
   BMSclearMemoryArray(branchruledata->validscores, branchruledata->scoresize);

   SCIP_CALL( SCIPselectVarStrongBranchingVanilla(scip, lpcands, lpcandssol, lpcandsfrac,
         nlpcands, npriolpcands, branchruledata->forcestrongbranch, &bestcand,
         &bestdown, &bestup, &bestscore, &bestdownvalid, &bestupvalid, &provedbound,
         branchruledata->latestscores, branchruledata->validscores,
         result) );

   SCIP_NODE* downchild;
   SCIP_NODE* upchild;
   SCIP_VAR* var;
   SCIP_Real val;

   assert(*result == SCIP_DIDNOTRUN);
   assert(0 <= bestcand && bestcand < nlpcands);
   assert(SCIPisLT(scip, provedbound, SCIPgetCutoffbound(scip)));

   var = lpcands[bestcand];
   val = lpcandssol[bestcand];

   /* perform the branching */
   SCIPdebugMsg(scip, " -> %d candidates, selected candidate %d: variable <%s> (solval=%g, down=%g, up=%g, score=%g)\n",
      nlpcands, bestcand, SCIPvarGetName(var), lpcandssol[bestcand], bestdown, bestup, bestscore);
   SCIP_CALL( SCIPbranchVarVal(scip, var, val, &downchild, NULL, &upchild) );
   assert(downchild != NULL || upchild != NULL);

   if( downchild != NULL )
   {
       SCIPdebugMsg(scip, " -> down child's lowerbound: %g\n", SCIPnodeGetLowerbound(downchild));
   }
   if( upchild != NULL )
   {
       SCIPdebugMsg(scip, " -> up child's lowerbound: %g\n", SCIPnodeGetLowerbound(upchild));
   }

   *result = SCIP_BRANCHED;

   SCIPfreeBufferArray(scip, &lpcandsfrac);
   SCIPfreeBufferArray(scip, &lpcandssol);
   SCIPfreeBufferArray(scip, &lpcands);

   return SCIP_OKAY;
}


/*
 * branching specific interface methods
 */

/** creates the full strong LP branching rule and includes it in SCIP */
SCIP_RETCODE SCIPincludeBranchruleFullstrongVanilla(
   SCIP*                 scip                /**< SCIP data structure */
   )
{
   SCIP_BRANCHRULEDATA* branchruledata;
   SCIP_BRANCHRULE* branchrule;

   /* create fullstrong branching rule data */
   SCIP_CALL( SCIPallocBlockMemory(scip, &branchruledata) );
   branchruledata->latestscores = NULL;
   branchruledata->validscores = NULL;
   branchruledata->scoresize = 0;

   /* include branching rule */
   SCIP_CALL( SCIPincludeBranchruleBasic(scip, &branchrule, BRANCHRULE_NAME, BRANCHRULE_DESC, BRANCHRULE_PRIORITY,
         BRANCHRULE_MAXDEPTH, BRANCHRULE_MAXBOUNDDIST, branchruledata) );

   assert(branchrule != NULL);

   /* set non-fundamental callbacks via specific setter functions*/
   SCIP_CALL( SCIPsetBranchruleCopy(scip, branchrule, branchCopyFullstrong) );
   SCIP_CALL( SCIPsetBranchruleFree(scip, branchrule, branchFreeFullstrong) );
   SCIP_CALL( SCIPsetBranchruleInit(scip, branchrule, branchInitFullstrong) );
   SCIP_CALL( SCIPsetBranchruleExit(scip, branchrule, branchExitFullstrong) );
   SCIP_CALL( SCIPsetBranchruleExecLp(scip, branchrule, branchExeclpFullstrong) );

   /* fullstrong branching rule parameters */
   SCIP_CALL( SCIPaddBoolParam(scip,
         "branching/fullstrong-vanilla/forcestrongbranch",
         "should strong branching be evaluated for all candidates no matter what?",
         &branchruledata->forcestrongbranch, TRUE, DEFAULT_FORCESTRONGBRANCH, NULL, NULL) );

   return SCIP_OKAY;
}

SCIP_Real* SCIPgetFullstrongVanillaLatestScores(
   SCIP*                 scip                /**< SCIP data structure */
)
{
   SCIP_BRANCHRULEDATA* branchruledata;
   SCIP_BRANCHRULE* branchrule;

   branchrule = SCIPfindBranchrule(scip, BRANCHRULE_NAME);
   branchruledata = SCIPbranchruleGetData(branchrule);

   return branchruledata->latestscores;
}

SCIP_Bool* SCIPgetFullstrongVanillaValidScores(
   SCIP*                 scip                /**< SCIP data structure */
)
{
   SCIP_BRANCHRULEDATA* branchruledata;
   SCIP_BRANCHRULE* branchrule;

   branchrule = SCIPfindBranchrule(scip, BRANCHRULE_NAME);
   branchruledata = SCIPbranchruleGetData(branchrule);

   return branchruledata->validscores;
}
