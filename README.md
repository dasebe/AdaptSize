# The AdaptSize Caching System


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
