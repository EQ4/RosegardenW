This is the README file from the main source code trunk.


Rosegarden v10.02 and Later
===========================

Rosegarden is a MIDI and audio sequencer and musical notation editor for Linux.

  http://www.rosegardenmusic.com/

Please keep an eye on the FAQ for known problems and workarounds:

  http://rosegardenmusic.com/wiki/frequently_asked_questions

When you find bugs, first check whether a newer version of Rosegarden
has been released yet; if not, please continue on to:

  http://rosegardenmusic.com/tutorials/bug-guidelines.html


Build requirements
------------------

For a complete list of build requirements, please see:

http://www.rosegardenmusic.com/wiki/dev:contributing


Installation instructions
-------------------------

If the directory where you found this README file does not already contain a
configure script, you must generate one by running:

    sh ./bootstrap.sh

Once you have a configure script, ensure that all of your build dependencies
have been installed, and then run:

    ./configure [ --prefix=[PREFIX] QTDIR=[QTDIR] [--enable-debug] ]
    make
    make install

New starting with 10.02, most of the application data files are bundled in the
rosegarden binary.  The install process will only copy a few files to various
directories under [PREFIX]:

    [PREFIX]/bin                                   application binary
    [PREFIX]/share/icons/hicolor/.../mimetypes     MIME type icons
    [PREFIX]/share/mime/packages                   MIME type configuration
    [PREFIX]/share/applications                    .desktop file
    [PREFIX]/share/icons/hicolor/32x32/apps        application icon

You may need to specify [QTDIR] on the configure line, so that the build can
find the Qt4 libraries.

The optional [--enable-debug] will build Rosegarden so that it is useful for
debugging, which can greatly improve our ability to find and correct bugs by
allowing Rosegarden to produce useful stack traces when it crashes.  WARNING!
Enabling this option results in an approximately 300 MB rosegarden binary!


Runtime requirements
--------------------

In order to be fully functional and provide the optimal user experience,
Rosegarden requires the following external applications.

  - General MIDI soft synth (TiMidity + Freepats or better)
  - LilyPond
  - Okular, Evince, or Acroread
  - lpr or lp
  - QjackCtl (JACK Audio Connection Kit - Qt GUI Interface)
  - FLAC
  - WavPack
  - DSSI plugins (any your distro carries)
  - LADSPA plugins (any your distro carries)

User documentation
------------------

Please see rosegardenmusic.com


SPECIAL NOTES FOR PACKAGE MAINTAINERS
-------------------------------------

As of 10.02 all formerly optional dependencies are now mandatory dependencies.
We've taken this step because it is a real support hassle working through a set
of problems to finally uncover the reason something has broken is due to the
user's distro package being built without a particular feature turned on.

Most major distros already carry everything we depend on, so we don't anticipate
that the build requirements will be a serious irritation for package
maintainers.

Thank you for your cooperation, and thank you for making Rosegarden available to
our users!  If you need to patch Rosegarden for one reason or another in the
course of packaging it, please do keep upstream in the loop at
rosegarden-devel@lists.sourceforge.net or by contacting
michael.mcintyre@rosegardenmusic.com directly.  Thanks!


Authors and copyright
---------------------

Rosegarden is Copyright 2000-2012 The Rosegarden Development Team

See http://rosegardenmusic.com/resources/authors/ for a complete list of
developers past and present, and to learn something about the history of our
project.

Rosegarden is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at
your option) any later version.  See the file COPYING for more details.
