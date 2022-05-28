#include "x.h"
#include "data.h"

// Copied these from data.c since I don't know how to inline across files.
inline static U32 _fast_arrayGetElemSz(const void *arryP) {
	return *(((U32*)arryP) - 2);
}

inline static Key* _getCompIdxPByEntity(System *sP, Entity entity) {
  return (Key*) mapGet(sP->e2cIdxMP, entity);
}

inline static void* _getCompPByEntity(System *sP, Entity entity) {
	return (void*) ((U8*) sP->cF + (*_getCompIdxPByEntity(sP, entity) * _fast_arrayGetElemSz(sP->cF)));
}

inline static Entity _getEntityByCompIdx(System *sP, Key compIdx) {
  return sP->cIdx2eA[compIdx];
}

void* xGetCompPByEntity(System *sP, Entity entity) {
  return _getCompPByEntity(sP, entity);
}

Entity xGetEntityByCompIdx(System *sP, Key compIdx) {
  return _getEntityByCompIdx(sP, compIdx);
}

static void _xSwap(System *sP, Key *origIdxP, Key newIdx) {
  Key *newIdxP  = mapGet(sP->e2cIdxMP, _getEntityByCompIdx(sP, newIdx));
  swap_(sP->cIdx2eA[newIdx], sP->cIdx2eA[*origIdxP]);
  swap_(*origIdxP, *newIdxP);
}

void xActivateComponentByEntity(System *sP, Entity entity) {
  // We use a pointer instead of a new one to swap its actual location later.
  Key *compOrigIdxP = _getCompIdxPByEntity(sP, entity);
  Key  compNewIdx   = frayActivate(sP->cF, *compOrigIdxP);
  if (compNewIdx != *compOrigIdxP) {
    // Existence of paused elements requires an additional, intermediate swap.
    if (!frayGetNPaused(sP->cF)) 
      _xSwap(sP, compOrigIdxP, compNewIdx);
    else {
      Key intermediateIdx = frayGetFirstInactiveIdx(sP->cF) - 1;
      _xSwap(sP, compOrigIdxP, intermediateIdx);
      _xSwap(sP, &intermediateIdx, compNewIdx);
    }
    // Example: If somebody starts moving, coll sys and trackers will want to know!
    mailboxWrite(sP->outboxF, 0, entity, sP->id, ACTIVATED);  
  }
}

void xDeactivateComponentByEntity(System *sP, Entity entity) {
  Key *compOrigIdxP = _getCompIdxPByEntity(sP, entity);
  Key  compNewIdx   = frayDeactivate(sP->cF, *compOrigIdxP);
  if (compNewIdx != *compOrigIdxP) {
    // Existence of paused elements requires an additional, intermediate swap.
    Key nPaused = frayGetNPaused(sP->cF);
    if (nPaused) 
      _xSwap(sP, compOrigIdxP, compNewIdx);
    else {
      Key intermediateIdx = frayGetFirstInactiveIdx(sP->cF) - nPaused;
      _xSwap(sP, compOrigIdxP, intermediateIdx);
      _xSwap(sP, &intermediateIdx, compNewIdx);
    }
    // Example: If somebody starts moving, coll sys and trackers will want to know!
    mailboxWrite(sP->outboxF, 0, entity, sP->id, DEACTIVATED);
  }
}

void xPauseComponentByEntity(System *sP, Entity entity) {
  Key *compOrigIdxP = _getCompIdxPByEntity(sP, entity);
  _xSwap(sP, compOrigIdxP, frayPause(sP->cF, *compOrigIdxP));
}

void xUnpauseComponentByEntity(System *sP, Entity entity) {
  Key *compOrigIdxP = _getCompIdxPByEntity(sP, entity);
  _xSwap(sP, compOrigIdxP, frayUnpause(sP->cF, *compOrigIdxP));
}

void xActivateComponentByIdx(System *sP, Key compOrigIdx) {
  xActivateComponentByEntity(sP, sP->cIdx2eA[compOrigIdx]);
}

void xDeactivateComponentByIdx(System *sP, Key compOrigIdx) {
  xDeactivateComponentByEntity(sP, sP->cIdx2eA[compOrigIdx]);
}

U32 xGetNComponents(System *sP) {
	return arrayGetNElems(sP->cF);
}

Error xAddComp(System *sP, Entity entity, Key compType, void *compDataP, void *compDataSrcP, XSwitchCompU switchCompU) {
  if (!sP || !compDataP)
    return E_BAD_ARGS;
  // Skip entities who already have a component in this system.
  if (mapGet(sP->e2cIdxMP, entity))
    return SUCCESS;
  // Make sure the component belongs to this system. This is only checked at load-time.
  if (compType != sP->id)
    return E_SYS_CMP_MISMATCH;
  
  // Put component in first empty slot. (Will be garbage if this is a map. That's okay.)
  U32 cIdx; 
  Error e = frayAdd(sP->cF, compDataP, &cIdx); 
  if (!e && !(sP->flags & FLG_NO_CF_SRC_A_)) 
    e = arraySetVoidElem(sP->cFSrcA, cIdx, compDataSrcP);
  // Assign this component to its entity and, if necessary, prepare it for the system.
  if (!e)
    e = sP->iniComp(sP, compDataP, compDataSrcP);  
  if (!e && switchCompU)   
    e = mapSet(sP->switchMP, entity, switchCompU);
  // Add lookups from C -> E and E -> C.
  if (!e)
    e = mapSet(sP->e2cIdxMP, entity, &cIdx);
  if (!e)
    sP->cIdx2eA[cIdx] = entity;

  return e;
}

Error xIniSys(System *sP, U32 nComps, void *miscP) {
  // Sytems with special parts need to initialize maps in sIniU().
  Error e = frayNew((void**) &sP->cF, sP->compSz, nComps);
  if (!e)
    e = arrayNew((void**) &sP->cIdx2eA, sizeof(Entity), nComps);
  if (!e)
    e = mapNew(&sP->e2cIdxMP, sizeof(Key), nComps);
  if (!e && !(sP->flags & FLG_NO_SWITCHES_))
    e = mapNew(&sP->switchMP, sizeof(XSwitchCompU), nComps);
  if (!e && !(sP->flags & FLG_NO_CHECKS_))
    e = frayNew((void**) &sP->checkF, sizeof(Check), nComps);
  if (!e && !(sP->flags & FLG_NO_CF_SRC_A_))
    e = arrayNew((void**) &sP->cFSrcA, sizeof(void*), nComps);
	// Allocate inbox ONLY. Caller points sP->outboxF at another system's inboxF.
	if (!e)
		e = frayNew((void**) &sP->inboxF, sP->id, nComps);
  // Finally, call the system's unique initializer.
  if (!e)
    e = (*sP->iniSys)(sP, miscP);
  // Clean up if there are any problems.
  if (e) 
    xClr(sP);
  
  return e;
}

// Don't erase everything in a system. Some things should be permanent.
void xClr(System *sP) {
  sP->clr(sP);  // This MUST run first as it may rely on things we're about to erase,
  frayDel((void**) &sP->cF);         // ... like maps of switches with cleanup cases.
  frayDel((void**) &sP->checkF);
  frayDel((void**) &sP->inboxF);
  mapDel(&sP->e2cIdxMP);
  mapDel(&sP->switchMP);
  arrayDel((void**) &sP->cIdx2eA);
  arrayDel((void**) &sP->cFSrcA);
  sP->outboxF = NULL;
}

static Error _xDoChecks(System *sP) {
  Error e = SUCCESS;
  Check *checkP = sP->checkF;
  Check *checkEndP = checkP + frayGetFirstInactiveIdx((void*) checkP);
  // For each active check...
  while (checkP < checkEndP) {
    Entity entity = _getEntityByCompIdx(sP, *checkP->cIdxP);
    // Run check. If it returns true, update target flag. 
    if (checkP->cbA[checkP->currCbIdx](entity, checkP->operandP)) {
      *checkP->resultFlagsP |= checkP->outputIfTrueA[checkP->currCbIdx];
      // Tell potentially sleeping tree to wake up.
      e = mailboxWrite(sP->outboxF, entity, checkP->root, 0, 0);
      // Toggle to alternate check if necessary. Otherwise, deactivate this check.
      if (checkP->doesToggle) { 
        checkP->currCbIdx = !checkP->currCbIdx;
        checkP++;
      }
      else {
        frayDeactivate(sP->checkF, checkP - sP->checkF);
        --checkEndP;
      }
    }
  }
  return e;
}

static void _switch(System *sP, Message *msgP) {
  // Get the entity's switch function. 
  // (Keep in mind this function can also just grab into an array if you tell it to.)
  XSwitchCompU switchU = (XSwitchCompU) mapGet(sP->switchMP, msgP->attn);
  if (switchU) {
    void* compP = _getCompPByEntity(sP, msgP->attn);
    if (compP) {
      void *tmpP = switchU(msgP->arg);
      if (tmpP)
        memcpy(compP, tmpP, sP->compSz);
    }
  }
}

void _xReadInbox(System *sP) {
  Message *msgP = sP->inboxF;
  Message *msgEndP = msgP + frayGetFirstInactiveIdx(sP->inboxF);
  for (; msgP < msgEndP; msgP++) {
    switch(msgP->cmd) {
      case SWITCH_AND_ACTIVATE: 
        _switch(sP, msgP);
      case ACTIVATE:
        xActivateComponentByEntity(sP, msgP->attn);
        break;
      case SWITCH_AND_DEACTIVATE: 
        _switch(sP, msgP);
      case DEACTIVATE:
        xDeactivateComponentByEntity(sP, msgP->attn);
        break;
      case SWITCH:
        _switch(sP, msgP);
        break;
      default:
        sP->processMessage(sP, msgP);  // some systems require more specific treatment
        break;
    }
  }
}

/* This is how the entire ECS framework works. */
Error xRun(System *sP) {
  _xReadInbox(sP);
  frayClr(sP->inboxF);
  Error e = sP->run(sP);
  if (!e)
    e = _xDoChecks(sP);
	return e;
}
