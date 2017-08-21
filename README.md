# The AdaptSize Caching System

AdaptSize is a caching system for the first-level memory cache in a CDN or in a reverse proxy of a large website.

CDN Memory caches serve high traffic volumes and are rarely sharded (sharding is used for second-level SSD caches). Typically, this means that hit ratios of first-level memory caches are low and highly variable.

AdaptSize's mission is

 - to maximize memory cache hit ratios for CDN workloads,
 - to make the cache robust against traffic variability,
 - while not imposing any throughput overhead.

AdaptSize is built on top of [Varnish Cache](https://github.com/varnishcache/varnish-cache/), the "high-performance HTTP accelerator".

A detailed description of AdaptSize is available in our [Paper (PDF)](https://www.usenix.org/system/files/conference/nsdi17/nsdi17-berger.pdf) and our [talk slides (PDF)](https://www.usenix.org/sites/default/files/conference/protected-files/nsdi17_slides_berger.pdf)  or as [audio/video recording](https://www.usenix.org/conference/nsdi17/technical-sessions/presentation/berger).

## Example: comparison to Varnish cache on production traffic

We replay a production trace from an edge cache that serves highly multiplexed traffic with a variety of different traffic patterns. An unmodified Varnish version achieves an average hit ratio of 0.42. We find that many web objects are evicted before being requested again. Varnish performance is also highly variable: the hit ratio's [coefficient of variation](https://en.wikipedia.org/wiki/Coefficient_of_variation) is 23%.

AdaptSize achieves a hit ratio of 0.66, which is a 1.57x improvement over unmodified Varnish. Additionally, AdaptSize stabilizes performance: the hit ratio's coefficient of variation is 5%, which is a 4.6x improvement.

![Hit ratio of AdaptSize and Varnish on a production trace](https://cloud.githubusercontent.com/assets/9959772/22971000/796f6354-f374-11e6-8993-d454c6fb8f4b.png)

**Figure 1: AdaptSize consistently improves the hit ratio when compared to unmodified Varnish.**

While AdaptSize significantly improves the hit ratio, it does not impose any throughput overhead. Specifically, AdaptSize does not add any synchronization locks and thus scales exactly like an unmodified Varnish does.

![Throughput of AdaptSize and Varnish](https://cloud.githubusercontent.com/assets/9959772/22971202/40cf0576-f375-11e6-933f-d5c4722b0ab0.png)

**Figure 2: AdaptSize achieves the same throughput as Varnish, for any hit ratio. Left plot shows high hit ratio scenario, right plot shows low hit ratio scenario.**

## How AdaptSize works

AdaptSize is a new caching policy. Caching policies make two types of decisions, which objects to admit into the cache, and which ones to evict.

Almost all prior work on caching policies focuses on the eviction policy (see [this Wikipedia article](https://en.wikipedia.org/wiki/Cache_replacement_policies) or the [webcachesim code base](https://github.com/dasebe/webcachesim)). Popular eviction policies are often LRU or FIFO variants. Varnish uses a "concurrent" LRU variant and admits every object by default.

To see why eviction policies by themselves are not enough, consider this scenario.

> Imagine that there are only two types of objects: 9999 small objects of size 100 KB (say, small web pages) and 1 large object of size 500 MB (say, a software download). Further, assume that all objects are equally popular and requested forever in round-robin order. Suppose that our HOC has a capacity of 1 GB.
> A HOC that does not use admission control cannot achieve an OHR above 0.5. Every time the large object is requested, it pushes out ~5000 small objects. It does not matter which objects are evicted: when the evicted objects are requested, they cannot contribute to the OHR.

While this simplifies things a lot, variants of this actually happen frequently under real production traffic. In fact, production traces regularly contain requests to objects between one 1B and several GBs.

Note that a simple admission policy can boost the hit ratio in the toy scenario above. For example, if the cache admits no object above 100 KB, the overall hit ratio will almost double to 0.99. Unfortunately, simple size thresholds like this are not very robust against changes in the request traffic.

AdaptSize uses a new admission policy that incorporates both object size and popularity. The idea is to use a probability that depends on the object size: 

- small objects are admitted with high proabability
- medium-sized objects are amitted with a small probability, so if frequently requested they'll get admitted eventually
- very large objects have such a small admission probability, they rarely get admitted (unless very popular).

AdaptSize continuously optimizes this mapping of size to admission probability using a new mathematical model. This model works based on observations of the most recent traffic and is used to derive the admission policy that maximizes the cache hit ratio.


## Installing AdaptSize

AdaptSize is a proof of concept, and not ready for production use. This repository contains the source code of the AdaptSize library (the math model) and the glue code to incorporate this model into the Varnish caching system.

Here are the steps to recreate our test setup.

### Step 0: Install dependencies

We need to compile Varnish from scratch and thus need [the same dependencies](https://varnish-cache.org/docs/trunk/installation/install.html).

Something like this might work

    sudo apt-get install -y autotools-dev make automake libtool pkg-config


### Step 1: Checkout AdaptSize and download Varnish source code

Obtain a copy of [AdaptSize](https://github.com/dasebe/AdaptSize/archive/master.zip) and [Varnish 4.1.2](https://varnish-cache.org/releases/rel4.1.2.html).

Unpack AdaptSize and navigate into the AdaptSize folder. In that folder, unpack the copy into a subdirectory named varnish-4.1.2.

    wget https://repo.varnish-cache.org/source/varnish-4.1.2.tar.gz
    tar xfvz varnish-4.1.2.tar.gz


### Step 2: Patch, compile, and install Varnish

We need to apply three small patches to the Varnish code base.

    patch varnish-4.1.2/bin/varnishd/cache/cache_req_fsm.c < VarnishPatches/cache_req_fsm.patch
    patch varnish-4.1.2/include/tbl/params.h < VarnishPatches/params.patch
    patch varnish-4.1.2/lib/libvarnishapi/vsl_dispatch.c < VarnishPatches/vsl_dispatch.patch



Then we can compile and install as usual

    cd varnish-4.1.2
    ./configure --prefix=/usr/local/varnish/
    make
    make install
    

### Step 3: Compile and install AdaptSize Vmod

The AdaptSize Vmod performs the actual admission control and relies on the second patch from above.
You may need to adjust the config path to your actual install path.

    cd AdaptSizeVmod
    export PKG_CONFIG_PATH=/usr/local/varnish/lib/pkgconfig
    ./autogen.sh --prefix=/usr/local/varnish
    ./configure --prefix=/usr/local/varnish/
    make
    make install


### Step 4: Compile AdaptSize tuning module

This program is run in parallel to Varnish and automatically tunes the size threshold parameter on live statistics from the cache.

     cd AdaptSizeTuner
     make

## Installing additional tools

These programs are not part of AdaptSize but were used to create plots and statistics.

Detailed hit ratio statistics

      cd VarnishHitStats
      make

Other types of statistics

      cd VarnishOtherStats
      make

### Step 5: Run and experiment

Create an experimental setup with a client and backend service, e.g., the one [we used ourselves](https://github.com/dasebe/webtracereplay).

Configure Varnish with a [VCL-file](http://varnish-cache.org/docs/4.0/reference/vcl.html) that enforces the admission decisions made by the AdaptSizeVMOD. Specifically, autoparam is called on a cache miss (aka vcl_backend_response) and is called with the object's size (aka beresp.http.Content-Length). It returns a bool which indicates whether the object should bypass the cache or not.

An example VCL could look like this:

      vcl 4.0;
      
      # use the AdaptSizeVmod	aka autoparam
      import autoparam;
      
      backend default {
          .host = "127.0.0.1";
          .port = "8000";
      }
      
      sub vcl_backend_response {
        if (!autoparam.explru(beresp.http.Content-Length)) {
           set beresp.uncacheable = true;
           set beresp.ttl = 0s;
           return (deliver);
        }
      }




## References

We ask academic works, which built on this code, to reference the AdaptSize paper:

    AdaptSize: Orchestrating the Hot Object Memory Cache in a CDN
    Daniel S. Berger, Ramesh K. Sitaraman, Mor Harchol-Balter
    To appear in USENIX NSDI in March 2017.
    
You can find more information on [USENIX NSDI 2017 here.](https://www.usenix.org/conference/nsdi17/technical-sessions)
