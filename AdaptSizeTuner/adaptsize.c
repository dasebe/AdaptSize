//#include "vsha256.h"
#include "../AdaptSizeLibrary/libadaptsize.h"

static const char ourname[] = "stattest";

int
main(int argc, char * const *argv)
{
  if(argc<6) {
    fprintf(stderr, "params: varnishfolder varnishadm cachesize cachetype searchmethod");
    exit(1);
  }
  char * varnishfolder = argv[1];
  int vflen = strlen(varnishfolder);
  char vsmfile[vflen+7];
  snprintf(vsmfile, vflen+7, "%s/_.vsm",varnishfolder);
  char * vadmexe = argv[2];

  if( access( vsmfile, F_OK ) == -1 ) {
    fprintf(stderr, "no vsmfile at %s",vsmfile);
    exit(1);
  }
  if( access( vadmexe, F_OK ) == -1 ) {
    fprintf(stderr, "no vadmexe at %s",vadmexe);
    exit(1);
  }

  cache_size = 1024L*1024L*atol(argv[3]);
  cacheType = argv[4];
  search_method = atoi(argv[5]);

  // param init
  totalrecc = 0;
  objcount = 0;
  current_size1 = 0L;
  current_objectc1 = 0L;
  current_size2 = 0L;
  current_objectc2 = 0L;
  ignoreVSM = 0;
  // golden section search
  v=1.0-r;
  x3f = (double) log2(cache_size); // upper bound 
  // first (default) param
  if(search_method == 2)
    param = 0.92*x3f; // advantage
  else
    param = x3f; //i.e. start with no limit at all
  if(param>31)
    param = 31;
  if(param<1)
    param = 1;
  if(enforceParam(param,varnishfolder,vadmexe)) {
    fprintf(stderr, "cannot execute varnishadm at %s",vadmexe);
    exit(1);
  }

  hitc = 0;
  recc = 0;
  markovUniqueBytesBelowParam = 0;
  resetCache();
  VSMinit(vsmfile, ourname);
  VTAILQ_INIT(&cacheList1);
  VTAILQ_INIT(&cacheList2);

  int thres = 0;
  unsigned ltime = (unsigned)time(NULL);
  const long markov_interval_length = 40000+ 3*(long)pow((double)cache_size,8.0/15.0);
  const long hillclimb_interval_length = x3f>=28 ? (x3f-28)*2*20000+20000 : 10000;

  while(VUT.sigint == 0) {
    idlec=0; // i.e., no idle periods (VSM overruns)
    thres = 1; // i.e. no new param

    // output status
    if(ltime+4<(unsigned)time(NULL) && recc!=0 ) {
      if( (search_method == 1 || search_method == 9))
	printf("nrecc(%lu) markov(%f)\n",recc,recc/(double)markov_interval_length);
      if( (search_method == 2  || search_method == 9) )
	printf("nrecc(%lu) hillclimb(%f)\n",recc, recc/(double)hillclimb_interval_length);
      ltime = (unsigned)time(NULL);
      fflush(stdout);
    }
     
    VUT_Main();

    // Markov model
    if( (search_method == 1 || search_method == 9) && (recc>markov_interval_length ) ) { //  && (markovUniqueBytesBelowParam/(double)cache_size > byteratioMIN) ){
      recc = 0;
      hitc = 0;
      thres = findParam();
    }
    // HILLCLIMBING shadow queues
    if( (search_method == 2  || search_method == 9) && (recc>hillclimb_interval_length ) ) { // 
      recc = 0;
      hitc = 0;
      printf("hillclimbing: %f=%lu %f=%lu\n",param-lookSize,hitc1,log2(2*pow(2,param)-pow(2,param-lookSize)),hitc2);
      hillclimbing(hitc1,hitc2);
      thres = 0;
      resetCache();
    }
    // new param: submit param to Varnish
    if(thres==0) { 
      if(param > x3f)
	param = x3f;
      if(param < 1)
	param = 1;
      if(hitc*10<recc) //sanity check: more than 10% hit ratio, we could be stopping anything to get into cache
	param = x3f;
      if(param > 31) // sanity check: max threshold (long int)
	param = 31;
      enforceParam(param,varnishfolder,vadmexe);
    } else
      nanosleep((const struct timespec[]){{0, 100000000L}}, NULL);
  }
  // finish up
  VUT_Fini();
  exit(0);
}
