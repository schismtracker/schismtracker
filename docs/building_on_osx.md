# Building on OS X

Building on Mac OS X is fairly straightforward once you've installed the Apple
Developer's kit. If you have [Homebrew](http://brew.sh/) available, you should
be able to install SDL with a simple `brew install sdl`, after which you can
mostly follow the Linux instructions and you'll end up with a unix-style
`schismtracker` binary that you can run from `Terminal.app`.

Alternately, you can also install the [SDL Developers
Library](http://libsdl.org/download-1.2.php), but this tends to be somewhat
more complicated as all the required pieces aren't shipped in the same package.
Make sure to grab both the source and the Runtime Libraries, and some reports
suggest that you might have to fiddle with paths to get `sdl.m4` working. But
really, there's not much advantage over just using `brew`.

If you want a "normal" application that you could drop into `/Applications`,
put in your dock, etc., copy that `schismtracker` binary into
`sys/macosx/Schism Tracker.app/Contents/MacOS/`, and then you can copy that
bundle around like any other app. It will associate itself with .IT (and other)
files, so you can double-click them and the Finder will load Schism Tracker
automatically.

However, if you want to build an application bundle *for distribution*, there's
a few potential snags. In particular, SDK versions are backward-incompatible,
so you need to make note of what version you're building with; and if you also
want to support the dwindling population of PowerPC users, you'll have to build
a Universal binary. Plus, since SDL is not normally present on OS X, you'll
need to bundle it in with the application.

There used to be somewhat lengthy instructions here elaborating on various
nuances of installing Fink and managing multiple SDKs, but these have become
rather outdated and probably less than thoroughly useful. If anyone has current
and first-hand experience with building on OS X which might be helpful to
others, please feel free to share.

## Cross-compiling on a Linux host

[This page](http://devs.openttd.org/~truebrain/compile-farm/apple-darwin9.txt)
has some notes that might be of use in building a cross-compilation toolchain.
Building a cross-compiler is *not* an easy process, and will probably take the
better part of a day; however, it is definitely possible, and in fact is what I
use for compiling the "official" OS X packages. The build process is rather
messy, but it goes something like:

    mkdir -p osx/{x86,ppc}
    cd osx/x86
    env PATH=/usr/i686-apple-darwin9/bin:${PATH}                        \
        {C,CXX,OBJC}FLAGS='-g0 -O2' LDFLAGS=-s                          \
        ../../configure --with-sdl-prefix=/usr/i686-apple-darwin9       \
                     --{target,host}=i686-apple-darwin9                 \
                     --build-i686-linux
    env PATH=/usr/i686-apple-darwin9/bin:${PATH} make
    cd ../ppc
    env PATH=/usr/powerpc-apple-darwin9/bin:${PATH}                     \
        {C,CXX,OBJC}FLAGS='-g0 -O2' LDFLAGS=-s                          \
        ../../configure --with-sdl-prefix=/usr/powerpc-apple-darwin9    \
                     --{target,host}=powerpc-apple-darwin9              \
                     --build-i686-linux
    env PATH=/usr/powerpc-apple-darwin9/bin:${PATH} make
    cd ..
    /usr/i686-apple-darwin9/bin/lipo -create -o {.,x86,ppc}/schismtracker
    /usr/i686-apple-darwin9/bin/install_name_tool -change               \
        '@executable_path/../Frameworks/SDL.framework/Versions/A/SDL'   \
        '@executable_path/sdl.dylib' schismtracker

Then copy the `lipo`'ed `schismtracker` and `sdl.dylib` both into the `MacOS/`
folder in the bundle, and it Should Work. The versions I have installed are GCC
4.0.1 (SVN v5493), ODCCTools SVN v280, and XCode SDK 3.1.3. This SDK version
advertises compatibility with 10.4 at minimum, although people have reported
success in running Schism Tracker on 10.3.9.

Note: because parts of the debugging information are conspicuously absent,
cross-compiling with debugging symbols simply isn't possible. This isn't very
likely to be useful anyway; if you can run the program in a debugger, then
you're *probably* running OS X, in which case you might as well just build
natively and eliminate the hassle.
