SKELEDIT (Skeletal Mesh Editor)
(c) Konstantin Nosov (Gildor), 2007-2015


Please support the development by making a donation here:
http://www.gildor.org/en/donate


System requirements
~~~~~~~~~~~~~~~~~~~
Windows operating system
x86-compatible CPU
OpenGL 1.1 videocard


Web resources
~~~~~~~~~~~~~
SkelEdit thread on www.gildor.org forums:
http://www.gildor.org/smf/index.php/topic,91.0.html


Command line options
~~~~~~~~~~~~~~~~~~~~
SkelEdit            - launch SkelEdit application
SkelEdit <filename> - open psk or pskx file on startup


Additional information
~~~~~~~~~~~~~~~~~~~~~~
SkelEdit were made as a tool for importing and tuning ActorX meshes and animations for one game
project. It's creation is relative to the umodel, if you need information about reasons of its
appearance and its purpose you may get some information here: http://www.gildor.org/en/about
(find "SkelEdit" word in the text if you're too lazy to read full article). Also check the forum
thread mentioned in this readme above.


Changes
~~~~~~~
17.09.2015
- recompiled with VC10

26.03.2012
- added command line option to open psk/pskx files

13.03.2012
- implemented pskx support (both SkeletalMesh and StaticMesh)

12.03.2012
- recompiled with VC9

18.04.2011
- upgraded to wxWidgets 2.9.1 with integrated wxPropertyGrid, compiled with VC7

21.03.2011
- fixed memory leak when assigning materials to the mesh

05.11.2010
- added background gradient effect

13.04.2009
- first public release
