# bitcorder
LGPL video live streaming tool

Bitcorder is based on a cross platform library, and intends to eventually support Linux, Windows, Android including TV, Mac, iOS, and Raspberry Pi. 
Bitcorder can currently composite together desktop capture, camera, and static image into a video stream
with sound across the room, to YouTube Live, or Twitch using intel hardware accelerated video compression on Linux.
It can also save a local file. Compositing supports alpha blending and a few OpenGL shader effects. It has some rudimentary .deb packaging, and in application documentation.

debian packaging is currently in a separate branch.

# Mini history

Originally the goal was Wayland support, and I did get as far as building a tool to convert the wcap Weston capture format to mkv, but have currently set X11 as highest priority. Most apps that currently support Wayland are built with a library that also supports X11, and can be switched with an environment variable. There isn't currently a standard way to record Wayland which potentially means a different method for each Wayland compositor. 

The current version started out as proof of concept prototypes from the gst-launch command, and slowly transforming those into a C program with command line options. This has some very significant limitations. For example launching the program starts the livestream immediately or not at all. There are no controls to change any setting while the program is running. Picking sound, and video sources is also awkward as instead of a list of found hardware the exact source needs to be known before starting the program. 

# Known Bugs

It has one major bug inherited from the gstreamer in that resized windows, and windows moved beyond the edge of the screen usually terminate the program. I have put much work into a new capture source without this bug, but it isn't ready to be integrated yet. It's about as big as the whole rest of the program. This also captures popup menus that many other capture tools miss. There are many FIXME markings in the code for minor bugs. Many of then are bite sized for those interested in helping.

# Roadmap

The next stage is getting it easier to use with interactive controls both for command line, and GUI. I've also put a huge amount of work into learning win32 programming so that I can finish the first port to another OS. The intention is that it can build and run within Wine so that it can mostly be tested on a single system. Wine doesn't support webcams as of the last time I looked so I don't think everything will be testable unless people add features to Wine. Doing a port early I hope helps prevent the code from being too tightly bound to an OS, and UI framework. Why win32 instead of using a cross platform toolkit like Qt? I had to go down below the toolkit level to work on fixing the bug with X11 capture, and similarly on Windows I'm going to need to go to a relatively low level to achieve the features I want. Most of the Windows related code in gstreamer itself uses the win32 API. 

Long term there are many more goals. Beyond just broadcasting video, network video streams can be used for things like extending the desktop to another screen, video conferencing, and security cameras. The core technology is mostly the same. Screen casting is already partially supported. Gstreamer itself has hundreds of plugins. One of the new and exciting ones is WebRTC. 

# Donations accepted
If you would like to help me get this project done you can just send me money directly or through a crowd funding service. 

teknotus@gmail.com    
https://www.patreon.com/teknotus  
https://www.gofundme.com/bitcorder  
There are also bug fixing opportunities.

Special thanks to Dave Nielsen for coming up with the name Bitcorder.
