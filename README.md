# Introduction (aka. "Why and What For")
It all started in Spring of 2014 when I, for a reason which I no longer remember, wanted to access 3.5" floppies of an 8-bit ZX Spectrum computer clone (branded _Didaktik_) on a modern PC. I already knew by then that this is possible as I saw (and worked with) several PC-designed utilities that specialized themselves in creating images of floppies of Atari ST, Amiga, Pentagon (another ZX Spectrum clone), etc. The utilities had poor interfaces (if any) and very limited possibilities on how to manipulate files (they were rather dumping tools instead of disk explorers). I knew there existed the [OmniFlop](http://www.shlock.co.uk/Utils/OmniFlop) tool for… well, I don't know – I never managed to run it due to strange (yet free) licencing and the author not responding to my e-mail requests to get a licence. But as far as I know, it should be another purely dumping tool. I knew there existed the [SamDisk](http://simonowen.com/samdisk) tool which I worked with until then as it offered soft-grained possibilities on dumping a floppy to various images, including the _heavily copy-protected ones_!

I liked that idea of OmniFlop bridging floppy formats of dozens of disk operating systems (DOSes), althrough really not understanding them (recall, its a dumping tool). I also liked the clear usage of SamDisk and its possibility to work with even copy-protected floppies. I liked the specialized tools that could (in more or less cumbersome way) extract the files of "their" floppies by _understanding_ the floppy contents. So I asked myself: _Has anyone already juggled everything into a single application?_ If yes, I unfortunately couldn't find such application.
# The Real and Imaginary Disk Editor (RIDE)
As I was missing something in each application I tried to directly access the floppy drive with, I created my own one and named it _Real and Imaginary Disk Editor_ (or simply _RIDE_). It serves for direct access to filesystems of some obscure or obsolete platforms, including the once very popular _MS-DOS_. The filesystems can be stored both on physical floppies and in images (hence the name "real and imaginary"). RIDE is suitable for both novice and expert users in a given platform. Novice users will find it easy to use for their early experimentations, while expert users will appreciate its advanced features that facilitate direct data modification and recovery. If the DOS you need is not implemented, you at least can access the floppy at the sectors level, or implement it yourself and then ideally commit the result to this repository, so that we all can benefit from your efforts.

Here I'd like to pinpoint some highlights that RIDE can offer for your retro-computing archeology:

- It attempts to bridge the gap between classical "import/export tools" and a simple data recovery applications in the realm of free software.
- It can automatically recognize the disk operating system (DOS) and corresponding disk format without user's intervention. Just insert the disk, access it via RIDE and immediatelly work with the files.
- It supports the hard-to-find _MDOS 2.0_ filing system (originally developed by _Didaktik_).
- It can read/write/format non-standard _MS-DOS_ track structures, including FAT32 for hard-drive images (Master Boot Record currently not supported, however).
- It doesn't attempt to shield you from any information available in a given filing system – even critical values are at your disposal.
- It allows you to at least dump sectors of unsupported filing systems, including the most common errors (usually part of a copy-protection scheme).
- It supports high-DPI screens.
# Compilation and Running (no installation needed, ever!)
RIDE needs Visual Studio 2010 or higher to compile, and Windows XP or higher to run. After cloning the repository, simply click _Build → Build Solution_ while having selected either the _Debug_ or _Release_ configuration, and that's it.

The third _Release MFC 4.2_ configuration is virtually for my purposes only – I use it to create new public releases here on GitHub. To compile using this configuration, you need to have installed the _Windows SDK_ (that contains all the sources and libraries of the legacy MFC 4.2 platform), plus properly set paths. However, the burden associated with compiling under the _Release MFC 4.2_ configuration is not worth it when there's the _Release_ configuration, so I won't elaborate the details.
# Frequently Asked Questions
Here is just a list of the _really_ frequent ones. The full list can be accessed through _Help → FAQ_ or [here on-line](http://nestorovic.hyperlink.cz/ride/html/faq.html).
- [What filesystems are supported?](http://nestorovic.hyperlink.cz/ride/html/faq_supportedSystems.html)
- [How do I access a real floppy?](http://nestorovic.hyperlink.cz/ride/html/faq_accessFloppy.html)
- [How do I create an image of a real floppy?](http://nestorovic.hyperlink.cz/ride/html/faq_floppy2image.html)
- [How do I dump an image back to a floppy?](http://nestorovic.hyperlink.cz/ride/html/faq_image2floppy.html)
- [How do I format a realy floppy?](http://nestorovic.hyperlink.cz/ride/html/faq_formatFloppy.html)
- [How do I convert one image to another?](http://nestorovic.hyperlink.cz/ride/html/faq_convertImage.html)
# Contributing
You can contribute in two ways by either _giving me a suggestion on improvement_ (if you found a bug or want a new feature), or _implementing a DOS in which you feel you are an expert_ (you know the structure of the disk, how files are stored on it, how to eventually tweak the filesystem, how to verify the disk is intact, etc.).
