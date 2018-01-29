#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>

#include "vapi/vsm.h"
#include "vapi/vsl.h"
#include "vapi/voptget.h"
#include "vdef.h"
#include "vut.h"
#include "vtree.h"
#include "vas.h"
#include "vdef.h"
#include "vcs.h"
#include <time.h>
#include "vqueue.h"
#include "miniobj.h"


// ASSUMPTION: request URLs already hashed to UInt!


int search_method;

// golden section search
static const int maxIterations = 15;
const double r = 0.61803399;
double v;
const double tol = 3.0e-8;
const double x0f;
double x3f;
#define SHFT2(a,b,c) (a)=(b);(b)=(c);
#define SHFT3(a,b,c,d) (a)=(b);(b)=(c);(c)=(d);

// data aggregation
static int match_tag_length;
static int match_tag_url;
static int match_tag_call;
long unsigned objcount;
int ignoreVSM;

long long unsigned totalrecc;

// cache tuning
long cache_size;
double param;
long hitc, recc;
const char * cacheType; //

// idle checker
unsigned idlec;
static const unsigned idleMax=50;

// Markov chain stats based on Varnish's red tree implementation
long markovUniqueBytesBelowParam;
struct log {
  long hash;
  unsigned long size;
  unsigned long count;
  double pastCount;
  VRB_ENTRY(log) e_key;
};

static VRB_HEAD(t_key, log) h_key = VRB_INITIALIZER(&h_key);

static inline int
cmp_key(const struct log *a, const struct log *b)
{
  if (a->hash != b->hash)
    return (a->hash - b->hash);
  return (a->size - b->size);
}


VRB_PROTOTYPE_STATIC(t_key, log, e_key, cmp_key);
VRB_GENERATE_STATIC(t_key, log, e_key, cmp_key);


#define oP1(T,l,p) (l * p * T * (840.0 + 60.0 * l * T + 20.0 * l*l * T*T + l*l*l * T*T*T))
#define oP2(T,l,p) (840.0 + 120.0 * l * (-3.0 + 7.0 * p) * T + 60.0 * l*l * (1.0 + p) * T*T + 4.0 * l*l*l * (-1.0 + 5.0 * p) * T*T*T + l*l*l*l * p * T*T*T*T)

double predictPoiss (double * reqcounts, double * reqsizes, long unsigned ppobjcount, const char * cType, double thparam)
{
  double old_T, the_T, the_C;
  double sum_val = 0;
  ///// ThLRU //////
  if (strcmp("ThLRU",cType)==0) {
    for (long int i=0; i<ppobjcount; i++) {
      sum_val += reqcounts[i] * (reqsizes[i] < pow(2,thparam)) * reqsizes[i];
    }
    if(sum_val <= 0)
      return(0.0);
    the_T = cache_size / sum_val;
    // 20 iterations to calculate TTL
    for(int j = 0; j<20; j++) {
      the_C = 0;
      for (long int i=0; i<ppobjcount; i++)
	the_C += (1-exp(-the_T*reqcounts[i]* (reqsizes[i] < pow(2,thparam)))) * reqsizes[i]; //special case ThLRU: fast approximation  via exp(-p*lambda*T)
      old_T = the_T;
      the_T = cache_size * old_T/the_C;

      // parse VSM so we're not overrun
      VUT_Main();
    }
    assert(the_C > 0);


    // calculate object hit ratio
    double weighted_hitratio_sum = 0.0;
    for (long int i=0; i<ppobjcount; i++) {
      weighted_hitratio_sum += reqcounts[i] * (1-exp(-the_T*reqcounts[i]* (reqsizes[i] < pow(2,thparam))));
    }
    return (weighted_hitratio_sum);

    ///// ExpLRU p2 //////
  } else if (strcmp("ExpLRU",cType)==0) {
    for (long int i=0; i<ppobjcount; i++) {
      sum_val += reqcounts[i] * (exp(-reqsizes[i]/ pow(2,thparam))) * reqsizes[i];
    }
    if(sum_val <= 0)
      return(0);
    the_T = cache_size / sum_val;
    // 20 iterations to calculate TTL
    for(int j = 0; j<20; j++) {
      the_C = 0;
      if(the_T > 1e70)
	break;
      for (long int i=0; i<ppobjcount; i++) {
	const double admProb = exp(-reqsizes[i]/ pow(2.0,thparam)); 
	const double tmp01= oP1(the_T,reqcounts[i],admProb); 
	const double tmp02= oP2(the_T,reqcounts[i],admProb);
	double tmp;
	if(tmp01!=0 && tmp02==0)
	  tmp = 0.0;
	else tmp= tmp01/tmp02;
	if(tmp<0.0)
	  tmp = 0.0;
	else if (tmp>1.0)
	  tmp = 1.0;
	the_C += reqsizes[i] * tmp;
      }
      old_T = the_T;
      the_T = cache_size * old_T/the_C;

      // parse VSM so we're not overrun
      VUT_Main();
    }

    // parse VSM so we're not overrun
    VUT_Main();

    // calculate object hit ratio
    double weighted_hitratio_sum = 0;
    for (long int i=0; i<ppobjcount; i++) {
      const double admProb = exp(-reqsizes[i]/ pow(2.0,thparam)); 
      const double tmp01= oP1(the_T,reqcounts[i],admProb); 
      const double tmp02= oP2(the_T,reqcounts[i],admProb);
      double tmp;
      if(tmp01!=0 && tmp02==0)
	tmp = 0.0;
      else tmp= tmp01/tmp02;
      if(tmp<0.0)
	tmp = 0.0;
      else if (tmp>1.0)
	tmp = 1.0;
      weighted_hitratio_sum += reqcounts[i] * tmp;
    }
    return (weighted_hitratio_sum);

    ///// InvLRU //////
  } else if (strcmp("InvLRU",cType)==0) {
    for (long int i=0; i<ppobjcount; i++) {
      const double admProb = (pow(2,thparam)/reqsizes[i]);
      sum_val += reqcounts[i] * (admProb>1?1:admProb) * reqsizes[i];
    }
    if(sum_val <= 0)
      return(0);
    the_T = cache_size / sum_val;
    // 20 iterations to calculate TTL
    for(int j = 0; j<20; j++) {
      the_C = 0;
      for (long int i=0; i<ppobjcount; i++) {
	const double admProb = (pow(2,thparam)/reqsizes[i]);
	const double tmp01= oP1(the_T,reqcounts[i],admProb); 
	const double tmp02= oP2(the_T,reqcounts[i],admProb);
	double tmp;
	if(tmp01!=0 && tmp02==0)
	  tmp = 0.0;
	else tmp= tmp01/tmp02;
	if(tmp<0.0)
	  tmp = 0.0;
	else if (tmp>1.0)
	  tmp = 1.0;
	the_C += reqsizes[i] * tmp;
      }
      old_T = the_T;
      the_T = cache_size * old_T/the_C;
    }
    // calculate object hit ratio
    double weighted_hitratio_sum = 0;
    for (long int i=0; i<ppobjcount; i++) {
      const double admProb = (pow(2,thparam)/reqsizes[i]);
      const double tmp01= oP1(the_T,reqcounts[i],admProb); 
      const double tmp02= oP2(the_T,reqcounts[i],admProb);
      double tmp;
      if(tmp01!=0 && tmp02==0)
	tmp = 0.0;
      else tmp= tmp01/tmp02;
      if(tmp<0.0)
	tmp = 0.0;
      else if (tmp>1.0)
	tmp = 1.0;
      weighted_hitratio_sum += reqcounts[i] * tmp;
    }
    return (weighted_hitratio_sum);
  }
  exit(1);
  return(0.0);
}




// Shadow queue data structures

// varnish VTAILQ list 
struct cacheLT {
  unsigned                magic;
#define CACHELT_MAGIC             0xA7D4005C
  VTAILQ_ENTRY(cacheLT)     list;
  long hash;
  unsigned long size;
};
VTAILQ_HEAD(cacheLT_head1, cacheLT);
VTAILQ_HEAD(cacheLT_head2, cacheLT);

// varnish hashmaptree
struct cacheMT {
  long hash;
  unsigned long size;
  VRB_ENTRY(cacheMT) e_key;
  struct cacheLT * listEntry;
};

static VRB_HEAD(t_key1, cacheMT) h_key1 = VRB_INITIALIZER(&h_key1);
static VRB_HEAD(t_key2, cacheMT) h_key2 = VRB_INITIALIZER(&h_key2);

static inline int
lcmp_key(const struct cacheMT *a, const struct cacheMT *b)
{
  if (a->hash > b->hash) 
    return (1);
  else if(a->hash < b->hash)
    return(-1);
  else {
    if(a->size > b->size)
      return(1);
    else if(a->size < b->size)
      return(-1);
    else return(0);
  }
}

VRB_PROTOTYPE_STATIC(t_key1, cacheMT, e_key, lcmp_key);
VRB_GENERATE_STATIC(t_key1, cacheMT, e_key, lcmp_key);
VRB_PROTOTYPE_STATIC(t_key2, cacheMT, e_key, lcmp_key);
VRB_GENERATE_STATIC(t_key2, cacheMT, e_key, lcmp_key);

struct cacheLT_head1 cacheList1;
struct cacheLT_head2 cacheList2;
long current_objectc1;
long current_objectc2;
long current_size1;
long current_size2;
long unsigned hitc1, hitc2;

// hillclimbing
double lookSize;
double stepSize;

int
lru_request(long id, long size, double tparam, int whichCache) {
  struct cacheMT *lp, l;
  l.hash = id;
  l.size = size;
  if(whichCache)
    lp = VRB_FIND(t_key1, &h_key1, &l);
  else
    lp = VRB_FIND(t_key2, &h_key2, &l);
  if(lp) {
    struct cacheLT *w2;
    w2 = lp->listEntry;
    if(whichCache) {
      VTAILQ_REMOVE(&cacheList1, w2, list);
      VTAILQ_INSERT_TAIL(&cacheList1, w2, list);
    } else {
      VTAILQ_REMOVE(&cacheList2, w2, list);
      VTAILQ_INSERT_TAIL(&cacheList2, w2, list);
    }
    return(1);
  }

  // check object size
  if(size>cache_size)
    return(0); //cannot admit at all
  // check if want to admit
  if( (strcmp("ThLRU",cacheType)==0 && pow(2,tparam) < size) || (strcmp("ExpLRU",cacheType)==0 && exp(-1*((double)size)/ pow(2,tparam)) < drand48()) ) {
    //    printf("%s: dont admit\n",cacheType);
    return(0);
  }

  // check if space
  long * current_size;
  if(whichCache)
    current_size = &current_size1;
  else
    current_size = &current_size2;
  while(*current_size + size>cache_size) {
    struct cacheLT *wr;
    if(whichCache)
      wr = VTAILQ_FIRST(&cacheList1);
    else
      wr = VTAILQ_FIRST(&cacheList2);
    assert(wr);
    struct cacheMT tpt, * tpr;
    tpt.hash = wr->hash;
    tpt.size = wr->size;
    if(whichCache)
      tpr = VRB_FIND(t_key1, &h_key1, &tpt);
    else
      tpr = VRB_FIND(t_key2, &h_key2, &tpt);
    if(!tpr) {
      printf("not found: %lu %lu [cache:%i]\n",wr->hash,wr->size,whichCache);
    }
    assert(tpr);
    //    printf("evict: %lu %lu (%li|%li|%li) \n",wr->hash,wr->size,*current_size,cache_size,size);
    fflush(stdout);
    *current_size -= wr->size;
    if(whichCache) {
      VRB_REMOVE(t_key1, &h_key1, tpr);
      VTAILQ_REMOVE(&cacheList1, wr, list);
      current_objectc1--;
    } else {
      VRB_REMOVE(t_key2, &h_key2, tpr);
      VTAILQ_REMOVE(&cacheList2, wr, list);
      current_objectc2--;
    }
    free(tpr);
    free(wr);
  }
  // insert into cacheList
  struct cacheLT *w;
  ALLOC_OBJ(w, CACHELT_MAGIC);
  w->hash = id;
  w->size = size;
  //  printf("insert: %lu %lu (%li|%li)\n",w->hash,w->size,*current_size,cache_size);
  fflush(stdout);
  *current_size += size;
  if(whichCache)
    VTAILQ_INSERT_TAIL(&cacheList1, w, list);
  else
    VTAILQ_INSERT_TAIL(&cacheList2, w, list);
  // insert into cacheMT tree
  lp = malloc(sizeof(struct cacheMT));
  lp->hash = id;
  lp->size = size;
  lp->listEntry = w;
  if(whichCache) {
    VRB_INSERT(t_key1, &h_key1, lp);
    current_objectc1++;
  } else {
    VRB_INSERT(t_key2, &h_key2, lp);
    current_objectc2++;
  }
  return(0);
}





// Varnish shared memory data function

static int __match_proto__(VUT_cb_f)
     sighup(void)
{
  printf("SIGHUB CALLED\n");
  return (1);
}

// no idling at all
static int __match_proto__(VUT_cb_f)
     idle(void)
{
  if(idlec++<idleMax)
    return(0);
  return(1);
}


static int __match_proto__(VSLQ_dispatch_f)
     accumulate(struct VSL_data *vsl, struct VSL_transaction * const pt[],
		void *priv)
{
  totalrecc++;
  if(ignoreVSM)
    return 0;

  //  char * durl = NULL;
  char * durl = NULL;
  unsigned ulen = 0;
  long dlength = 0;

  unsigned tag;

  struct VSL_transaction *tr;

  (void)priv;

  for (tr = pt[0]; tr != NULL; tr = *++pt) {

    while ((1 == VSL_Next(tr->c))) {

      if (!VSL_Match(vsl, tr->c))
	continue;
      tag = VSL_TAG(tr->c->rec.ptr);

      if (tag == match_tag_url) {
	const char *b, *e, *p;
	b = VSL_CDATA(tr->c->rec.ptr);
	e = b + VSL_LEN(tr->c->rec.ptr);
	for (p = b; p <= e; p++) { // scan up to VSL_LEN bytes for end char
	  if (*p == '\0')
	    break;
	}
	ulen = p - b;
	if (ulen == 0)
	  continue;
	durl = malloc((ulen)*sizeof(*durl));
	memcpy(durl,VSL_CDATA(tr->c->rec.ptr)+1,ulen);
	//	printf("URL %s\n",durl);
      }
      else if (tag == match_tag_length) {
	// get value as double
	int i = sscanf(VSL_CDATA(tr->c->rec.ptr), "Content-Length: %li", &dlength);
	if (i != 1) 
	  continue;
	//	printf("LENGTH %lu\n", dlength);
      } else if (tag == match_tag_call) {
	if (strcmp(VSL_CDATA(tr->c->rec.ptr),"HIT") == 0) 
	  hitc++;
	else if (strcmp(VSL_CDATA(tr->c->rec.ptr),"RECV") == 0)
	  recc++;
      }
    }
  }
  struct log *lp, l;

  // check both url (durl) and object size (dlength) present
  if(durl!=NULL && dlength) {
    // Markov chain method
    if (search_method == 1 || search_method==9) {
      l.hash = atol(durl);
      l.size = dlength;
      lp = VRB_FIND(t_key, &h_key, &l);
      if(lp) {
	// new object in this period: count as unique byte
	if( lp->count==0 )
	  markovUniqueBytesBelowParam += dlength;
	lp->count++;
	//      printf("seen before: %s %lu %lu\n",lp->durl,lp->length,lp->count);
      } else {
	lp = malloc(sizeof(struct log));
	lp->hash = atol(durl);
	lp->size = dlength;
	lp->count = 1;
	lp->pastCount = 0.0;
	VRB_INSERT(t_key, &h_key, lp);
	//      printf("new: %s %lu\n",lp->durl,lp->length);
	objcount++;
	// new object: count as unique byte
	markovUniqueBytesBelowParam += dlength;
      }
    }
    // shadow queue method
    if (search_method == 2 || search_method==9) {
      /* SHA256_Init(&c); */
      /* SHA256_Update(&c, durl, strlen(durl)); */
      /* unsigned char o[32]; */
      /* SHA256_Final(o, &c); */
      hitc1+= lru_request(atol(durl), (long)dlength, param-lookSize, 1);
      hitc2+= lru_request(atol(durl), (long)dlength, log2(2*pow(2,param)-pow(2,param-lookSize)), 0);
    }
  }

  free(durl);
  return (0);
}

int enforceParam(double curParam,char * varnishfolder,char * vadmexe) {
  printf("Enforcing_param new_log2(%f) New_bytes(%lu) Total #reqs (%llu)\n",curParam,(long unsigned)pow(2,curParam),totalrecc);
  char command[350];
  snprintf(command, sizeof(command), "%s -n %s 'param.set opt_threshold %lu'",
	   vadmexe,varnishfolder,(long unsigned)pow(2,curParam));
  int err = system(command);
  if (err) {
    fprintf(stderr, "failed to %s\n", command);
    return 1;
  }
  return 0;
}



void hillclimbing(double leftHitRatio, double rightHitRatio) {
  // try out two params and then
  if(leftHitRatio>rightHitRatio)
    param -= stepSize;
  if(rightHitRatio>leftHitRatio)
    param += stepSize;
  return;
}



int findParam() {
  if(VUT.sigint || objcount<1000)
    return 1;

  // parse VSM so we're not overrun
  VUT_Main();
  ignoreVSM = 1;
  
  // move into arrays for vectorization + exp smoothing
  struct log *tp, *tp2;
  double * reqcounts = malloc((objcount)*sizeof(*reqcounts));
  double * reqsizes = malloc((objcount)*sizeof(*reqsizes));
  double pptotalreqs = 0.0;
  long ppUniqueBytes = 0;
  long unsigned ppobjcount = 0;
  long unsigned removedobjs = 0;

  int vsm_interval = 100000;

  for (tp = VRB_MIN(t_key, &h_key); tp != NULL; tp = tp2) {
    tp2 = VRB_NEXT(t_key, &h_key, tp);
    // request counts: use exponential smoothing
    tp->pastCount = 0.7*((double)(tp->count)) + 0.3*tp->pastCount;
    tp->count = 0;
    reqcounts[ppobjcount] = tp->pastCount;
    pptotalreqs += reqcounts[ppobjcount];
    // request sizes
    reqsizes[ppobjcount] = tp->size;
    ppUniqueBytes += reqsizes[ppobjcount];
    ppobjcount++;
    // cleanup if too few requests overall
    if(tp->pastCount < 0.1) {
      VRB_REMOVE(t_key, &h_key, tp);
      free(tp); 
      removedobjs ++;
    }
    if(vsm_interval--<1) {
      // parse VSM so we're not overrun
      VUT_Main();
      vsm_interval = 100000;
    }
  }

  assert(objcount == ppobjcount);
  objcount -= removedobjs;
  ignoreVSM = 0;

  // parse VSM so we're not overrun
  VUT_Main();

  // first evaluation
  double x0 = x0f;
  double x3 = x3f;

  double bestx1 = 0.0;
  double besth1 = 0.0;

  for (double xtest= x0f+1; xtest<x3; xtest+=1.0) {
    // parse VSM so we're not overrun
    VUT_Main();
    const double tmph1 = predictPoiss(reqcounts, reqsizes, ppobjcount, cacheType, xtest);
    bestx1 = tmph1 > besth1 ? xtest : bestx1;
    besth1 = tmph1 > besth1 ? tmph1 : besth1;
    //    printf("%f %f\n",besth1,bestx1);
  }

  // parse VSM so we're not overrun
  VUT_Main();

  double x1 = bestx1;
  double h1 = besth1;

  printf("Last param(%f) Best guess(%f) Measured OHR(%f) Model OHR(%f) Total #Objects(%lu) In-model #Objects(%lu) Removed #Objects(%lu) Unique Byte Factor(%f)\n",
	 param, x1, h1/pptotalreqs, ((double)hitc)/((double)recc),objcount,ppobjcount,removedobjs,ppUniqueBytes/(double)cache_size);

  double x2;
  double h2;
  // start golden section search into larger segment
  if(x3-x1 > x1-x0) {// then above x1 is larger segment
    x2 = x1+v*(x3-x1);
    h2 = predictPoiss(reqcounts, reqsizes, ppobjcount, cacheType, x2);
  } else { //below x1 is larger segment
    x2 = x1;
    h2 = h1;
    x1 = x0+v*(x1-x0);
    h1 = predictPoiss(reqcounts, reqsizes, ppobjcount, cacheType, x1);
  }
  assert(x1<x2);
     
  //    int maxiterations = 20; // && maxiterations-->1
  int curIterations=0;
  while (curIterations++<maxIterations && fabs(x3-x0) > tol*(fabs(x1)+fabs(x2))) { // Numerical recipes in C termination condition
    //NAN check
    if( (h1!=h1) || (h2!=h2) ) 
      break;
    // parse VSM so we're not overrun by Varnish
    VUT_Main();
    if(VUT.sigint)
      return 1;
    printf("Model param low (%f) : ohr low (%f) | param high (%f) : ohr high (%f)\n",x1,h1/pptotalreqs,x2,h2/pptotalreqs);
    if (h2 > h1) {
      SHFT3(x0,x1,x2,r*x1+v*x3);
      SHFT2(h1,h2,predictPoiss(reqcounts, reqsizes, ppobjcount, cacheType, x2));
    } else {
      SHFT3(x3,x2,x1,r*x2+v*x0);
      SHFT2(h2,h1,predictPoiss(reqcounts, reqsizes, ppobjcount, cacheType, x1));
    }
  }

  // parse VSM so we're not overrun
  VUT_Main();

  free(reqcounts);
  free(reqsizes);

  // accept result
  if( (h1!=h1) || (h2!=h2) )
    return 1;
  if (h1 > h2) { // which one should be the final parameter
    param = x1;
    printf("Final param (%f) - model hit ratio (%f)\n",param,h1/pptotalreqs);
  } else {
    param = x2;
    printf("Final param (%f) - model hit ratio (%f)\n",param,h2/pptotalreqs);
  }
  return 0;
}

void VSMinit(char * vsmfile, const char progname[]) {
  // VSM init
  VUT_Init(progname);
  int opt='N';
  if (!VUT_Arg(opt, vsmfile)) {
    fprintf(stderr,"failed to get VSM");
    exit(1);
  }

  //  passively read VSM (read old data, don't wait for new
  opt = 'd';  
  if (!VUT_Arg(opt, "")) {  
    fprintf(stderr,"failed to set d param");  
    exit(1);  
  }  
  // read only client-side data not backend/session
  opt = 'c';  
  if (!VUT_Arg(opt, "")) {  
    fprintf(stderr,"failed to set c param");  
    exit(1);  
  }  

  // those are the fields we're interested, nothing else
  match_tag_length = VSL_Name2Tag("RespHeader", 10);
  match_tag_call = VSL_Name2Tag("VCL_call", 8);
  match_tag_url = VSL_Name2Tag("ReqURL", 6);
  if (match_tag_length < 0 || match_tag_call < 0 || match_tag_url < 0) {
    fprintf(stderr,
	    "fail tag name\n");
    exit(1);
  }

  VUT_Arg('i', "RespHeader");
  VUT_Arg('i', "VCL_call");
  VUT_Arg('i', "ReqURL");
  VUT_Arg('g', "request");

  /* Setup output */
  VUT.dispatch_f = &accumulate;
  VUT.sighup_f = sighup;
  VUT.idle_f = idle;

  VUT_Setup();
}


void resetCache()
{
  // hillclimbing
  stepSize=1;
  lookSize=1;

  struct cacheMT *ctp, *ctp2;
  struct cacheLT *clentry;


  if(hitc1<hitc2) {
    // flush inferior cache: cache1
    for (ctp = VRB_MIN(t_key1, &h_key1); ctp != NULL; ctp = ctp2) {
      ctp2 = VRB_NEXT(t_key1, &h_key1, ctp);
      clentry = ctp->listEntry;
      current_size1 -= clentry->size;
      current_objectc1--;
      //    printf("%lu %lu - %lu %lu\n",ctp->hash,ctp->size,clentry->hash,clentry->size);
      VTAILQ_REMOVE(&cacheList1, clentry, list);
      VRB_REMOVE(t_key1, &h_key1, ctp); 
      //    free(ctp->durl);
      free(ctp);
      free(clentry);
    }
    if(current_size1 != 0L || current_objectc1 != 0L)
      printf("1: cs>0 %lu co>0 %lu\n",current_size1,current_objectc1);
    assert(current_size1 == 0L);
    assert(current_objectc1 == 0L);

    // copy superior cache: cache2 to cache1
    struct cacheLT *newclentry;
    VTAILQ_FOREACH(clentry,&cacheList2, list) {
      ALLOC_OBJ(newclentry, CACHELT_MAGIC);
      newclentry->hash = clentry->hash;
      newclentry->size = clentry->size;
      VTAILQ_INSERT_TAIL(&cacheList1, newclentry, list);
      ctp = malloc(sizeof(struct cacheMT));
      ctp->hash = clentry->hash;
      ctp->size = clentry->size;
      ctp->listEntry = newclentry;
      VRB_INSERT(t_key1, &h_key1, ctp);      
      current_size1 += clentry->size;
      current_objectc1++;
    }
    assert(current_size1==current_size2);
    assert(current_objectc1==current_objectc2);
  }


  if(hitc2<hitc1) {
    // flush inferior cache: cache2
    for (ctp = VRB_MIN(t_key2, &h_key2); ctp != NULL; ctp = ctp2) {
      ctp2 = VRB_NEXT(t_key2, &h_key2, ctp);
      clentry = ctp->listEntry;
      current_size2 -= clentry->size;
      current_objectc2--;
      //    printf("%lu %lu - %lu %lu\n",ctp->hash,ctp->size,clentry->hash,clentry->size);
      VTAILQ_REMOVE(&cacheList2, clentry, list);
      VRB_REMOVE(t_key2, &h_key2, ctp); 
      //    free(ctp->durl);
      free(ctp);
      free(clentry);
    }
    if(current_size2 != 0L || current_objectc2 != 0L)
      printf("2: cs>0 %lu co>0 %lu\n",current_size2,current_objectc2);
    assert(current_size2 == 0L);
    assert(current_objectc2 == 0L);

    // copy superior cache: cache1 to cache2
    struct cacheLT *newclentry;
    VTAILQ_FOREACH(clentry,&cacheList1, list) {
      ALLOC_OBJ(newclentry, CACHELT_MAGIC);
      newclentry->hash = clentry->hash;
      newclentry->size = clentry->size;
      VTAILQ_INSERT_TAIL(&cacheList2, newclentry, list);
      ctp = malloc(sizeof(struct cacheMT));
      ctp->hash = clentry->hash;
      ctp->size = clentry->size;
      ctp->listEntry = newclentry;
      VRB_INSERT(t_key2, &h_key2, ctp);      
      current_size2 += clentry->size;
      current_objectc2++;
    }
    assert(current_size1==current_size2);
    assert(current_objectc1==current_objectc2);
  }
  
  hitc1 = 0;
  hitc2 = 0;
}
