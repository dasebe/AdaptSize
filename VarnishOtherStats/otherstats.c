#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "vapi/vsm.h"
#include "vapi/vsc.h"
#include "vas.h"

int
main(int argc, char * const *argv)
{
  int opt;
  struct VSM_data *vd;
  int i;

  if ((opt = getopt(argc, argv, VSC_ARGS "1f:lVxjt:")) == -1) {
    fprintf(stderr,"failed to get N param");
    return(1);
  }
	    
  vd = VSM_New();
  AN(vd);

  if(opt != 'N' || VSC_Arg(vd, opt, optarg) == 0) {
    fprintf(stderr,"failed to get N param2");
    return(1);
  }

  i = VSM_Open(vd);
  if (i) {
    fprintf(stderr,"failed to open");
    return(1);
  }

  printf("uptime client_req s_req cache_hit cache_hitpass cache_miss backend_fail fetch_failed threads n_lru_nuked n_lru_moved s_resp_bodybytes\n");
  long unsigned oclr=0, osreq=0, och=0, ochp=0, ocm=0, obf=0, off=0, olnu=0, olmo=0, obyt=0;

  struct VSC_C_main * cstat = NULL;

  while(!cstat)
    cstat = VSC_Main(vd, NULL);

  oclr = cstat->client_req;
  osreq = cstat->s_req;
  och = cstat->cache_hit;
  ochp = cstat->cache_hitpass;
  ocm = cstat->cache_miss;
  obf = cstat->backend_fail;
  off = cstat->fetch_failed;
  olnu = cstat->n_lru_nuked;
  olmo = cstat->n_lru_moved;
  obyt = cstat->s_resp_bodybytes;
  sleep(5);

  for(;;) {
    cstat = VSC_Main(vd, NULL);
    if(!cstat)
      continue;
    printf("%lu %f %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu\n",cstat->uptime,
	   (double)(cstat->cache_hit-och)/((double)(cstat->client_req-oclr)),
	   cstat->client_req-oclr,cstat->s_req-osreq,cstat->cache_hit-och,cstat->cache_hitpass-ochp,cstat->cache_miss-ocm,cstat->backend_fail-obf,cstat->fetch_failed-off,cstat->threads,cstat->n_lru_nuked-olnu,cstat->n_lru_moved-olmo,cstat->s_resp_bodybytes-obyt);
    fflush(stdout);
    oclr = cstat->client_req;
    osreq = cstat->s_req;
    och = cstat->cache_hit;
    ochp = cstat->cache_hitpass;
    ocm = cstat->cache_miss;
    obf = cstat->backend_fail;
    off = cstat->fetch_failed;
    olnu = cstat->n_lru_nuked;
    olmo = cstat->n_lru_moved;
    obyt = cstat->s_resp_bodybytes;
    sleep(5);
  }
  return(0);
}
