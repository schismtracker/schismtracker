# Building on OS X

Start by installing [Homebrew](http://brew.sh/). Open up the Terminal and paste in the following command.

```
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```

After Homebrew has been successfully installed, you need to install `automake`, `autoconf`, `SDL` and `git`.

```
brew install automake autoconf sdl git
```

Now clone the GitHub repo:

```
git clone https://github.com/schismtracker/schismtracker.git
```

Enter the Schismtracker folder and run `autoreconf -i`

```
cd schismtracker
autoreconf -i
```

Now you will need to create the `build` -folder, enter it and start `../configure`

```
mkdir -p build
cd build
../configure && make
```

Test Schismtracker from the commandline by typing

```
./schismtracker
```

If it worked, you are ready to start the updating of the **Schism Tracker.app**


## Baking Schism Tracker into an App ready to be put in /Applications

If you are in the `build` -folder, discover the `Schism_Tracker.app` subfolder `Contents` and, after creating the `MacOS` -folder, copy the newly built `schismtracker` there - then test the `Schism_Tracker.app` by clicking on it in Finder. Here are the instructions on how to do it (this will open a Finder window showing the `sys/macosx` -folder, where-in you will see the app itself. 

```
cd ../sys/macosx/Schism_Tracker.app/Contents/
mkdir MacOS
cd MacOS
cp ../../../../../build/schismtracker .
cd ../../../
open .
```

If this newly baked version of `Schism_Tracker.app` worked, just copy it to your `/Applications` -folder.

Enjoy.


## Building for distribution

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
