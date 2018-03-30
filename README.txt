foo_controlserver Version 1.1.5
-------------------------------
Foo_controlserver is a Foobar2000 plug-in enabling remote control of Foobar2000
via an Ethernet socket connection.

You would use it in a setup composed of:

Foobar2000 running on a server PC on your local network, and with the
foo_controlserver plug-in installed and configured.

You would then use a remote client app to connect to and control Foobar2000
over your local network.

You can use any remote client app that sends and receives commands defined
for foo_controlserver. One such remote client app is 'Foobar2000 Copilot',
a Windows Phone app.  See : http://www.foobar2000copilot.com

INSTALLING the foo_controlserver.zip component in Foobar2000
------------------------------------------------------------
For instructions, go to : 

http://foobar2000copilot.com/app-user-manual/foo-controlserver-install

If you are using Foobar2000 Copilot as your remote app, for instructions
on setting it up to communicate with Foobar2000 go to:

http://foobar2000copilot.com/app-user-manual/foobar2000-copilot-server-setup-connecting

The online user manual for Foobar2000 Copilot is at:

http://foobar2000copilot.com/app-user-manual


The 'foo_controlserver.zip' file in the GitHub archive is the only file you will
need to install the component in Foobar2000, but comes bundled to also include the
full source code under the terms of the GPL license as per the original developer.

The foo_controlserver plug-in was originally developed by Jason Poloski,
who released it into the public domain. See his original GPL license document
in the archive.

foo_controlserver is validated to run on Windows 7 and above. It does not currently
run on Windows XP.  If you are running Windows XP, you can still run the v1.0.2 version, 
which is also available under Releases -- just click on the foo_controlserver.zip link
under Assets for that version. That older version, however, does not do library searches
and album art.

------------------------------------------------------------

VERSION HISTORY:
>> foo_controlserver Version 1.1.5 Dec 2017 - updates by Walter Hartman
   - Updated socket message sending due to potential performance issues,
     optimized code for larger album art and playlist msgs

>> foo_controlserver Version 1.1.4 Oct 2017 - updates by Walter Hartman
   - Added track title and album title to 'now playing' album art msg response
   
>> foo_controlserver Version 1.1.3 Sept 2017 - updates by Walter Hartman
   - Rebuilt using VS2015 Community
   - Rebuilt with new Foobar2000 SDK version 2015-08-03
   - Updated preferences dialog to use the new foobar2000 SDK APIs
     (you will no longer see the 'this is a legacy preferences page' warning)
   - Added IP address display of local PC to preferences dialog
   - Added get album art command, returns image (jpg or png), encoded as a base64 string
   - Added media library search command
   - Fixed some bugs related to the 'now playing' track

>> foo_controlserver Version 1.0.2 - original baseline version by Jason Poloski 


