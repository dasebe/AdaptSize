# The AdaptSize Caching System

AdaptSize is a caching system for the first-level memory cache in a CDN or in a reverse proxy of a large website.

CDN Memory caches typically have to serve high traffic volumes. They are also rarely sharded (sharding is used for second-level SSD caches). This means that hit ratios of first-level memory caches are low and highly variable.

AdaptSize's mission is to

 - maximize the hit ratio, and
 - stabilize the hit ratio (less variability), while
 - not imposing any throughput overhead.

AdaptSize is built on top of [Varnish Cache](https://github.com/varnishcache/varnish-cache/), the "high-performance HTTP accelerator".

## Example: comparison to Varnish cache on Akamai production traffic

We replay a production trace from an Akamai edge cache that serves highly multiplexed traffic which is hard to cache. An unmodified Varnish version achieves an average hit ratio of 0.42 due to many objects being evicted before being requested again. Varnish performance is also highly variable: the hit ratio's [coefficient of variation](https://en.wikipedia.org/wiki/Coefficient_of_variation) is 23%.

AdaptSize achieves a hit ratio of 0.66, which is a 1.57x improvement over unmodified Varnish. Additionally, AdaptSize stabilizes performance: the hit ratio's coefficient of variation is 5%, which is a 4.6x improvement.


![hitratio_overtime](https://cloud.githubusercontent.com/assets/9959772/22971000/796f6354-f374-11e6-8993-d454c6fb8f4b.png)

While AdaptSize significantly improves the hit ratio, it does not impose a throughput overhead. Specifically, AdaptSize does not add any locks and thus scales exactly like an unmodified Varnish does.

![o6-throughput](https://cloud.githubusercontent.com/assets/9959772/22971202/40cf0576-f375-11e6-933f-d5c4722b0ab0.png)

## How AdaptSize works

AdaptSize is a new caching policy. Caching policies make two types of decisions, which objects to admit into the cache, and which ones to evict.

Almost all prior work on caching policies focuses on the eviction policy (see [this Wikipedia article](https://en.wikipedia.org/wiki/Cache_replacement_policies) or the [webcachesim code base](https://github.com/dasebe/webcachesim)). Popular eviction policies are often LRU or FIFO variants. Varnish uses a "concurrent" LRU variant and admits every object by default.

AdaptSize introduces a new new cache **admission policy**, which limits which objects get admitted into the cache. Admission decisions are based on the following intuition:

> If cache space is limited (as in memory caches), admitting large objects can be bad for the hit ratio (as they force the eviction of many other objects, which then won't be in the cache on their next request). Accordingly, large objects need to prove their worth before being allowed into the cache.

AdaptSize uses a new mathematical theory to continuously tune the admission decision.

![o10-randomization-time-hr-pathex10hk](https://cloud.githubusercontent.com/assets/9959772/22971164/1d026372-f375-11e6-816c-b166487cf83e.png)


## Installing AdaptSize

### Step 1: Download Varnish Source Code

Obtain a copy of Varnish 4.1.2 from https://varnish-cache.org/releases/rel4.1.2.html.
Unpack the copy into a folder named varnish-4.1.2.

    cd AdaptSize
    wget https://repo.varnish-cache.org/source/varnish-4.1.2.tar.gz
    tar xfvz varnish-4.1.2.tar.gz


### Step 2: Patch, Compile, and Install Varnish

We need to apply three small patches to the Varnish code base.

    patch varnish-4.1.2/bin/varnishd/cache/cache_req_fsm.c < VarnishPatches/cache_req_fsm.patch
    patch varnish-4.1.2/include/tbl/params.h < VarnishPatches/params.patch
    patch varnish-4.1.2/lib/libvarnishapi/vsl_dispatch.c < VarnishPatches/vsl_dispatch.patch

Check your dependencies. Something like this might work

    sudo apt-get install -y autotools-dev make automake libtool pkg-config libvarnishapi1 libvarnishapi-dev


Then we can compile and install as usual

    cd varnish-4.1.2
    ./configure --prefix=/usr/local/varnish/
    make
    make install
    

### Step 3: Compile and Install AdaptSize Vmod

The AdaptSizeVmod performs the actual admission control and relies on the second patch from above.
You may need to adjust the config path to your actual install path.

    cd AdaptSizeVmod
    export PKG_CONFIG_PATH=/usr/local/varnish/lib/pkgconfig
    ./autogen.sh --prefix=/usr/local/varnish
    ./configure --prefix=/usr/local/varnish/
    make
    make install


### Step 4: Compile AdaptSize Tuning Module

This program is run in parallel to Varnish and automatically tunes the size threshold parameter on live statistics from the cache.

     cd AdaptSizeTuner
     make

## Installing Additional Tools

These programs are not part of AdaptSize but were used to create plots and statistics.

Detailed hit ratio statistics

      cd VarnishHitStats
      make

Other types of statistics

      cd VarnishOtherStats
      make
