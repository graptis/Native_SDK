===================
IntroducingPVRShell
===================

.. figure:: ./IntroducingPVRShell.png

This training course introduces the PVRShell library.

Description
-----------
This demo shows basic use of the PVRShell library.  The PowerVR Shell handles all OS specific initialisation code, and has several built in command line features which allow for the specifying of attributes such as window width/height, quitting after a number of frames, taking screenshots and others. When using the PVR Shell, the application uses the class 'pvr::Shell' as its base class, and is constructed and returned from a 'pvr::newDemo' function.

APIS
----
* Vulkan
* OpenGL ES 2.0+

Controls
--------
- Quit- Close the application