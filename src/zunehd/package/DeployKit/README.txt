DeployKit, DeployKit Engine, and Exploit Deployment Tool
README
-------------------------------------------------------------------------------
What is it
==========

DeployKit provides a streamlined graphical interface for installing
applications on the Zune. While any Zune application can be installed using
DeployKit, it is primarily intended for applications built using the OpenZDK,
which cannot be deployed using .ccgame files due to the IL verification step
imposed by that feature. DeployKit makes it possible for native applications
to be distributed in binary form, freeing the user of the burden of installing
a complicated toolchain just to load up a simple application. DeployKit is
intended as a complement to source code, not a substitute; using DeployKit
to facilitate binary-only distribution is not encouraged.

The Exploit Deployment Tool (EDT) builds upon the DeployKit engine to provide a
graphical interface for unlocking the XNA runtime on pre-HD Zune models. EDT
can also be used to restore the unmodified XNA runtime on all Zune models,
including the Zune HD.

This package contains the DeployKit binaries as well as the source code to
DeployKit, the DeployKit engine, and the Exploit Deployment Tool. Additionally,
files for preparing 7-zip self-extracting executables (SFXs) are provided to
assist in building packages using DeployKit. While comprehensive documentation
is not included, the default configuration file at bin\application.cfg is
heavily commented with information on the directives that can be used to
tailor DeployKit to your application.

More information
================

More information on DeployKit and the Exploit Deployment Tool can be found at
http://zunedevwiki.org/.