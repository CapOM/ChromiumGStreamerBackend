# Running layout tests on Linux

1.  Build `blink_tests` (see LinuxBuildInstructions)
1.  Checkout the layout tests
    *   If you have an entry in your `.gclient` file that includes
        "LayoutTests", you may need to comment it out and sync.
    *   You can run a subset of the tests by passing in a path relative to
        `src/third_party/WebKit/LayoutTests/`.  For example,
        `run_layout_tests.py fast` will only run the tests under
        `src/third_party/WebKit/LayoutTests/fast/`.
1.  When the tests finish, any unexpected results should be displayed.

See
[Running WebKit Layout Tests](http://dev.chromium.org/developers/testing/webkit-layout-tests)
for full documentation about set up and available options.

## Pixel Tests

The pixel test results were generated on Ubuntu 10.4 (Lucid). If you're running
a newer version of Ubuntu, you will get some pixel test failures due to changes
in freetype or fonts.  In this case, you can create a Lucid 64 chroot using
`build/install-chroot.sh` to compile and run tests.

## Fonts

Make sure you have all the necessary fonts installed.

```shell
sudo apt-get install apache2 wdiff php5-cgi ttf-indic-fonts \
   msttcorefonts ttf-dejavu-core ttf-kochi-gothic ttf-kochi-mincho \
   ttf-thai-tlwg
```

You can also just run `build/install-build-deps.sh` again.

## Plugins

If `fast/dom/object-plugin-hides-properties.html` and
`plugins/embed-attributes-style.html` are failing, try uninstalling
`totem-mozilla` from your system:

    sudo apt-get remove totem-mozilla


## Running layout tests under valgrind on Linux

As above, but use `tools/valgrind/chrome_tests.sh -t webkit` instead.  e.g.

    sh tools/valgrind/chrome_tests.sh -t webkit LayoutTests/fast/

This defaults to using --debug.  Read the script for more details.

If you're trying to reproduce a run from the valgrind buildbot, look for the
`--run_chunk=XX:YY` line in the bot's log. You can rerun exactly as the bot did
with the commands.

```shell
cd ~/chromium/src
echo XX > valgrind_layout_chunk.txt
sh tools/valgrind/chrome_tests.sh -t layout -n YY
```

That will run the XXth chunk of YY layout tests.

## Configuration tips

*   Use an optimized `content_shell` when rebaselining or running a lot of
    tests. ([bug 8475](https://crbug.com/8475) is about how the debug output
    differs from the optimized output.)

    `ninja -C out/Release content_shell`

*   Make sure you have wdiff installed: `sudo apt-get install wdiff` to get
    prettier diff output.
*   Some pixel tests may fail due to processor-specific rounding errors. Build
    using a chroot jail with Lucid 64-bit user space to be sure that your system
    matches the checked in baselines.  You can use `build/install-chroot.sh` to
    set up a Lucid 64 chroot. Learn more about
    [using a linux chroot](using_a_linux_chroot.md).

## Getting a layout test into a debugger

There are two ways:

1.  Run `content_shell` directly rather than using `run_layout_tests.py`. You
    will need to pass some options:
    *   `--no-timeout` to give you plenty of time to debug
    *   the fully qualified path of the layout test (rather than relative to
        `WebKit/LayoutTests`).
1.  Or, run as normal but with the
    `--additional-drt-flag=--renderer-startup-dialog
    --additional-drt-flag=--no-timeout --time-out-ms=86400000` flags. The first
    one makes content\_shell bring up a dialog before running, which then would
    let you attach to the process via `gdb -p PID_OF_DUMPRENDERTREE`. The others
    help avoid the test shell and DumpRenderTree timeouts during the debug
    session.

## Using an embedded X server

If you try to use your computer while the tests are running, you may get annoyed
as windows are opened and closed automatically.  To get around this, you can
create a separate X server for running the tests.

1.  Install Xephyr (`sudo apt-get install xserver-xephyr`)
1.  Start Xephyr as display 4: `Xephyr :4 -screen 1024x768x24`
1.  Run the layout tests in the Xephyr: `DISPLAY=:4 run_layout_tests.py`

Xephyr supports debugging repainting. See the
[Xephyr README](http://cgit.freedesktop.org/xorg/xserver/tree/hw/kdrive/ephyr/README)
for details. In brief:

1.  `XEPHYR_PAUSE=$((500*1000)) Xephyr ...etc...  # 500 ms repaint flash`
1.  `kill -USR1 $(pidof Xephyr)`

If you don't want to see anything at all, you can use Xvfb (should already be
installed).

1.  Start Xvfb as display 4: `Xvfb :4 -screen 0 1024x768x24`
1.  Run the layout tests in the Xvfb: `DISPLAY=:4 run_layout_tests.py`

## Tiling Window managers

The layout tests want to run with the window at a particular size down to the
pixel level.  This means if your window manager resizes the window it'll cause
test failures.  This is another good reason to use an embedded X server.

### xmonad

In your `.xmonad/xmonad.hs`, change your config to include a manageHook along
these lines:

```
test_shell_manage = className =? "Test_shell" --> doFloat
main = xmonad $
  defaultConfig
    { manageHook = test_shell_manage <+> manageHook defaultConfig
    ...
```
