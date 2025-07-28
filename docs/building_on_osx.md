# Building on OS X

Start by installing [Homebrew](http://brew.sh/). Open up the Terminal and paste
in the following command:

    /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"

After Homebrew has been successfully installed, you need to install `automake`,
`autoconf`, `sdl2` and `git`.

    brew install automake autoconf sdl2 git perl pkg-config utf8proc

Alternatively, if you have MacPorts installed, you can use this command
instead:

    sudo port install automake autoconf libtool libsdl2 git perl5 pkg-config libutf8proc

For FLAC sample loading support, you will also need development versions of the
`flac` and `libogg` libraries.

In this case, you may have to open a new terminal shell, or else you may get
warnings about the version of autoconf/automake you're using.

Now clone the GitHub repo:

    git clone https://github.com/schismtracker/schismtracker.git

Enter the Schismtracker folder and run `autoreconf -i`:

    cd schismtracker
    autoreconf -i

Now you will need to create the `build` folder, enter it and start the build:

    mkdir -p build
    cd build
    ../configure && make

Test Schismtracker from the commandline by typing:

    ./schismtracker

If it worked, you are ready to start the updating of the **Schism
Tracker.app**.


# Automated testing

If you are doing ongoing development, you should enable Schism Tracker's
automated test suite. This is done by passing `--enable-tests` to
`./configure`.

    ../configure --enable-tests

The resulting `Makefile` will produce a second binary alongside
`schismtracker` called `schismtrackertests`:

    $ make
    (..)
    $ ./schismtrackertest
    TEST: test_bshift_arithmetic ..................................... PASS (0 ms)
    TEST: test_bshift_right_shift_negative ........................... PASS (0 ms)
    TEST: test_bshift_left_shift_overflow ............................ PASS (0 ms)
    Results: 3 passed, 0 failed
    $

In this build mode, if you make a change that requires corresponding changes
to tests, you discover immediately on the next build.


## Baking Schism Tracker into an App ready to be put in /Applications

If you are in the `build` folder, find the `Schism_Tracker.app` subfolder
`Contents` and, after creating the `MacOS` folder, copy the newly built
`schismtracker` there. Then test the `Schism_Tracker.app` by clicking on it in
Finder. Here are the instructions on how to do it (this will open a Finder
window showing the `sys/macosx` folder, wherein you will see the app itself.

	cd ../sys/macosx/Schism_Tracker.app/Contents/
	mkdir MacOS
	cd MacOS
	cp ../../../../../build/schismtracker .
	cd ../../../
	open .

If this newly baked version of `Schism_Tracker.app` worked, just copy it to
your `/Applications` -folder.

Enjoy.


## Building for distribution

See the `macos` section of `.github/workflows/osx.yml` for how Schism
currently does it.
