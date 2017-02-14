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

static const char progname[] = "histstat";
int viewcount;
static int match_tag_header;
static int match_tag_ts;
static int match_tag_call;
static int match_tag_resp;
static int match_tag_storage;
const char * store1 = "malloc Memory";

long unsigned hitc1, hitcT, recc, failc, hitbytec1, hitbytecT, bytec;
double tstart,treq,twait,tfetch,tprocess,tresp;
unsigned ltime;

unsigned idlec;
static const unsigned idleMax = 100;

static int __match_proto__(VUT_cb_f)
     sighup(void)
{
  return (1);
}

static int __match_proto__(VUT_cb_f)
     idle(void)
{
  if(idlec++>idleMax)
    return(1);
  return (0);
}


void printAndReset() {
  printf("%u %lu %lu %lu %lu %lu %lu %lu %f %f %f %f %f %f\n",
	 (unsigned)time(NULL),hitc1,hitcT,recc,failc,hitbytec1,hitbytecT,bytec,tstart,treq,twait,tfetch,tprocess,tresp);
  fflush(stdout);
  viewcount = 0;
  hitc1 = 0;
  hitcT = 0;
  recc = 0;
  failc = 0;
  hitbytec1 = 0;
  hitbytecT = 0;
  bytec = 0;
  tstart=0;
  treq=0;
  twait=0;
  tfetch=0;
  tprocess=0;
  tresp=0;
  ltime = (unsigned)time(NULL);
}


static int __match_proto__(VSLQ_dispatch_f)
     accumulate(struct VSL_data *vsl, struct VSL_transaction * const pt[],
		void *priv)
{

  //  char * durl = NULL;
  unsigned long dlength = 0;
  unsigned tag;
  int hit = 0;
  int storeid = -1;
  double ts1=0,ts2=0,ts3=0;
  int fstart=0,freq=0,fwait=0,ffetch=0,fprocess=0,fresp=0;

  struct VSL_transaction *tr;

  (void)priv;

  for (tr = pt[0]; tr != NULL; tr = *++pt) {
    while ((1 == VSL_Next(tr->c))) {
      if (!VSL_Match(vsl, tr->c))
	continue;
      tag = VSL_TAG(tr->c->rec.ptr);

      if (tag == match_tag_header) { //first-most common tag: HEADER
	if(dlength==0) {
	  const int i = sscanf(VSL_CDATA(tr->c->rec.ptr), "Content-Length: %lu", &dlength);
	  if (i == 1)
	    	bytec += dlength;
	}
	continue;
      } else if (tag == match_tag_ts) { //second-most common tag: TIMESTAMP
	if(fstart==0) {
	  const int i = sscanf(VSL_CDATA(tr->c->rec.ptr), "Start: %lf %lf %lf", &ts1, &ts2, &ts3);
	  if (i == 3) {
	    tstart+=ts3;
	    fstart=1;
	    continue;
	  }
	}
	if(freq==0) {
	  const int i = sscanf(VSL_CDATA(tr->c->rec.ptr), "Req: %lf %lf %lf", &ts1, &ts2, &ts3);
	  if (i == 3) {
	    treq+=ts3;
	    freq=1;
	    continue;
	  }
	}
	if(fwait==0) {
	  const int i = sscanf(VSL_CDATA(tr->c->rec.ptr), "Waitinglist: %lf %lf %lf", &ts1, &ts2, &ts3);
	  if (i == 3) {
	    twait+=ts3;
	    fwait=1;
	    continue;
	  }
	}
	if(ffetch==0) {
	  const int i = sscanf(VSL_CDATA(tr->c->rec.ptr), "Fetch: %lf %lf %lf", &ts1, &ts2, &ts3);
	  if (i == 3) {
	    tfetch+=ts3;
	    ffetch=1;
	    continue;
	  }
	}
	if(fprocess==0) {
	  const int i = sscanf(VSL_CDATA(tr->c->rec.ptr), "Process: %lf %lf %lf", &ts1, &ts2, &ts3);
	  if (i == 3) {
	    tprocess+=ts3;
	    fprocess=1;
	    continue;
	  }
	}
	if(fresp==0) {
	  const int i = sscanf(VSL_CDATA(tr->c->rec.ptr), "Resp: %lf %lf %lf", &ts1, &ts2, &ts3);
	  if (i == 3) {
	    tresp+=ts3;
	    fresp=1;
	    continue;
	  }
	}
	continue;
      } else if (tag == match_tag_call) { // common tag: VCL_call
	if (strcmp(VSL_CDATA(tr->c->rec.ptr),"HIT") == 0) 
	  hit = 1;
	else if (strcmp(VSL_CDATA(tr->c->rec.ptr),"RECV") == 0)
	  recc++;
      } else if (tag == match_tag_resp) { //RespReason
	if (strcmp(VSL_CDATA(tr->c->rec.ptr),"OK") != 0)
	  failc++;
      } else if (tag == match_tag_storage) {// artificial tag: Storage
	if (strcmp(VSL_CDATA(tr->c->rec.ptr),store1) == 0)
	  storeid = 1;
	else if (strcmp(VSL_CDATA(tr->c->rec.ptr),"malloc Transient") == 0)
	  storeid = 4;
      }
    }
  }

  if (storeid == 1) {
    hitc1 += hit;
    hitbytec1 += dlength;
  } else if (storeid == 4) {
    hitcT += hit;
    hitbytecT += dlength;
  }

  return (0);
}


int
main(int argc, char * const *argv)
{
  if(argc<2) {
    fprintf(stderr, "too few params");
    exit(1);
  }

  viewcount = 0;

  hitc1 = 0;
  hitcT = 0;
  recc = 0;
  failc = 0;
  hitbytec1 = 0;
  hitbytecT = 0;
  bytec = 0;
  tstart=0;
  treq=0;
  twait=0;
  tfetch=0;
  tprocess=0;
  tresp=0;

  int opt;
  if ((opt = getopt(argc, argv, vopt_optstring)) != -1) {

    VUT_Init(progname);
    viewcount = 0;

    switch (opt) {
    case 'N':
      if (!VUT_Arg(opt, optarg)) {
	fprintf(stderr,"failed to get N param");
	exit(1);
	break;
      default:
	fprintf(stderr,"unkown param");
	exit(1);
	break;
      }
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

    match_tag_ts = VSL_Name2Tag     ("Timestamp", 9);
    match_tag_header = VSL_Name2Tag ("RespHeader", 10);
    match_tag_call = VSL_Name2Tag   ("VCL_call", 8);
    match_tag_resp = VSL_Name2Tag   ("RespReason", 10);
    match_tag_storage = VSL_Name2Tag("Storage", 7);
    if (match_tag_header < 0 || match_tag_call < 0 || match_tag_resp < 0) {
      fprintf(stderr,
	      "fail tag name\n");
      exit(1);
    }

    VUT_Arg('i', "Timestamp");
    VUT_Arg('i', "RespHeader");
    VUT_Arg('i', "VCL_call");
    VUT_Arg('i', "RespReason");
    VUT_Arg('i', "Storage");
    //      VUT_Arg('g', "");

    /* Setup output */
    VUT.dispatch_f = &accumulate;
    VUT.sighup_f = sighup;
    VUT.idle_f = idle;

    VUT_Setup();
    ltime = (unsigned)time(NULL);
    while(VUT.sigint == 0) {
      idlec = 0;
      VUT_Main();
      if(ltime+4<(unsigned)time(NULL) && recc!=0 )
	printAndReset();
      else
      sleep(1);
    }
    VUT_Fini();

  }
  exit(0);
}
