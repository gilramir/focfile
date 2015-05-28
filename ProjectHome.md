This was released in 1997, and hasn't been updated since. It was retrieved from
the Internet Way Back machine, and copied to this hosting side.


---

FOCUS and PC/FOCUS are trademarks of Information Builder's Inc (IBI). My
FocFile library is in no way related to or supported by IBI; if you have
any questions, comments, suggestions, or flames, send them to me, not
to IBI.

---


Why C++?  Why a 3GL?  Why not FOCUS?

I wrote FocFile to make a particularly difficult data-extraction program
faster. I am very pleased with the increase in speed as a result of using
optimized C++ in place of the FOCUS 4GL (TABLE). Of course the development
time was longer.... :-)

You might consider trying FocFile if your data-extraction program
runs slowly because of the complex arrangement of your FOCUS files.
Since FocFile is optimized for speed, the extra effort of using C++
may pay off handsomely.
FocFile can also overcome some limitations in FOCUS. The most relevant
case is the TABLE language's limit of 64 verb-objects in a request. With
C++ and FocFile, if you've got the memory, you can retrieve as much data
as you want.

The FocFile library can read FOCUS files that were created on Intel
platforms. Specifically, PC/FOCUS 6.01 for DOS is the target of the
library since I use that version the most. FocFile might work
to some extent on other platforms, but the portability must be worked on
some more.    If you are reading a FOCUS file, your C++ program must be
compiled on the same architecture that your FOCUS file was created on.
FocFile has been compiled with the GNU C++ compiler (g++) under Linux
and DOS. If you write C or C++ in DOS, I highly recommend djgpp, D. J.
Delorie's port of the GNU compiler suite. You can read about it at the
DJGPP Home Page <http://www.delorie.com/djgpp/>.

Perhaps in the future FocFile can be extended to read FOCUS files on
other platforms (Sun, HP, MVS, etc.). This can only be accomplished with
the help of other interested programmers, as the author's collection
of C++ compilers and access to FOCUS versions only intersect on the
Intel platform.

This is the first alpha distribution, so don't expect too much. Actually,
non-index functions (next, match, hold) seem to work very well. I am
still testing the index-related functions, so don't be surprised if they
don't work. I am releasing FocFile now since parts of it are functional.
If you are lucky, indices will work for you. There are different versions
of FOCUS indices, so don't worry when FocFile dies on your file. Let me
know about it, and together we can patch FocFile so that it can read your
index.