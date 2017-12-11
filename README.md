# csmanager

**csmanager** is used hardlink files into a directory which is synced
to cloud storage.



## SYNOPSIS

**csmanager** \[options] source_dir


## EXAMPLE
**csmanager** ~ // hardlinks all files under ordinary dirs into the
dir that is synced into the cloud. Creates dirs under the cloud dir
as needed. Dot dirs are ignored.

**csmanager** -D ~ // From all hidden dirs, make a tarball of each.
The name of each tarball is the name of the dir with the leading '.'
removed.

## WARNING
This is very new. The *INSTALL* file needs instructions appropriate to
**github** and the the manpage, *csmanager.1* is presently empty.
