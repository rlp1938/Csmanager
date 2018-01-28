# csmanager

**csmanager** is used to hardlink files into a directory which is synced
to cloud storage.

## SYNOPSIS

**csmanager** \[options] [source_dir]


## EXAMPLE
**csmanager** // hardlinks all files under ordinary dirs into the
dir that is synced into the cloud. Creates dirs under the cloud dir
as needed. If the *source_dir* is not specified $HOME is used. Hidden
dirs are treated specially. These dirs are created and synced into a
sub-dir to the target synced dir.
