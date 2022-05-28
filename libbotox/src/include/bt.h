#ifndef BT_H
#define BT_H
#include "x.h"

#define NO_CHILDREN NO_SIBLINGS_LEFT 
#define NO_SIBLINGS_LEFT (0)  /* Root can never be a sibling, so this works. */

#define BBSeed_(name_, ...) \
  KeyValPair name_##BbKvPair[] = {__VA_ARGS__};\
  BBSeed name_##BBSeed = {\
    .nKeyValPairs = sizeof(name_##BbKvPair) / sizeof(KeyValPair), \
    .keyValPairA = name_##BbKvPair\
  };

struct Node;

// RUNNING indicates that ECS is running checks on active components. When a check reports something, the
// tree reiterates from the root. (Multiple nodes could be waiting on different checks at the same time.) 
// That "RUNNING" node will eventually see the updated state (from a map whose keys are BT array indices; 
// its values are states) and performs its next block of code if it meets its conditions. 

typedef enum {READY, RUNNING, COMPLETE, FAILED, ERROR} NodeStat;

typedef void* (*BbLookup)(Key key);

typedef struct {
  U8 nKeyValPairs;
  KeyValPair *keyValPairA;
} BBSeed;

typedef struct {
  NodeStat *nodeStatA;  // tree node's status (complete, failed, running, error, etc.)
  Map      *conditionMP;  // maps node indices to U32 condition flags specifically enumerated for the condition node
} BTreeStatus;

typedef NodeStat (*NodeCb)(struct Node *rootP, struct Node *currNodeP, BTreeStatus *bTStatP, BbLookup bbLookupU, Message *outboxF);  

// SrcNode is read-only memory. It gets translated to Node stored in an array at load-time.
typedef struct SrcNode {
  U8 nChildren;
  U8 condition;
  NodeCb nodeCb;
  struct SrcNode **childrenPA;
} SrcNode;

#define LeafNodeFuncDef_(name_, cb_, ...)\
  SrcNode *name_##ChildrenA[] = {__VA_ARGS__};\
  SrcNode name_ = {\
    .nodeCb = cb_,\
    .nChildren = nArgs_(SrcNode*, __VA_ARGS__),\
    .childrenPA = name_##ChildrenA\
  };

#define SelectorNodeFuncDef_(name_, ...)\
  SrcNode *name_##ChildrenA[] = {__VA_ARGS__};\
  SrcNode name_ = {\
    .nodeCb = btSelector,\
    .nChildren = nArgs_(SrcNode*, __VA_ARGS__),\
    .childrenPA = name_##ChildrenA\
  };

#define SequenceNodeFuncDef_(name_, ...)\
  SrcNode *name_##ChildrenA[] = {__VA_ARGS__};\
  SrcNode name_ = {\
    .nodeCb = btSequence,\
    .nChildren = nArgs_(SrcNode*, __VA_ARGS__),\
    .childrenPA = name_##ChildrenA\
  };

// NodeFuncDef_ enforces conformity to node signatures. This way you don't have to remember how to implement a callback node.
#define NodeFuncDef_(name_) NodeStat name_(Node *rootP, Node *currNodeP, BTreeStatus *btStatP, BbLookup bbLookupU, Message *outboxF) 
#define nodeRun_ _nodeRun(rootP, currNodeP, btStatP, bbLookupU, outboxF)

typedef struct Node {
  U8 firstChildIdx;
  U8 nextSiblingIdx;  // allows easy "while(nodeP->nextSibling)" condition-check
  U8 thisIdx;         // allows updating of its own NodeStat and for keying condition map
  U8 condition;       // bit-flag conditions which must be met to run this node
  NodeCb nodeCb;      // because nobody points at root @ index 0 as sibling or child
} Node;

typedef struct NodeA {
  U8 *firstChildIdxA;
  U8 *nextSiblingIdxA;  // allows easy "while(nodeP->nextSibling)" condition-check
  U8 *thisIdxA;         // allows updating of its own NodeStat and for keying condition map
  U8 *conditionA;
  NodeCb *nodeCbA;      // because nobody points at root @ index 0 as sibling or child
} NodeA;

typedef struct {
  Node *rootP;
	SrcNode *rootSrcP;
} BTree;  

#define BTree_(name_, root_) \
	BTree name_ = {\
		.treeP = NULL,\
		.rootSrcP = root_\
	}

// Functions
Error btIni(BTree *treeP);
void btClr(BTree *treeP);
NodeStat btRun(BTree *treeP, BTreeStatus *btStatP, BbLookup bbLookupU, Message *outboxF);  
Error btStatNew(BTreeStatus **btStatPP, Node *rootP);
void btStatDel(BTreeStatus **btStatPP);
// Entity will be passed in as well as outbox. But it'll only ever be used to fill out the outbox messages. 
// However, btRun is separate from Nodes. That doesn't matter if every callback wants to know how to send a
// message to whatever system each is interested in. Guess I have to kiss my beautiful simple design goodbye.
//
//
NodeFuncDef_(btSequence);
NodeFuncDef_(btSelector);
NodeFuncDef_(btCondition);   // easy-to-check condition (e.g. world state)
NodeFuncDef_(btXCondition);  // ECS-based condition

#endif

